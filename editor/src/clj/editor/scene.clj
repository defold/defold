(ns editor.scene
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [dynamo.util :as util]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.input :as i]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage DataBufferByte]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Tab]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]))

(set! *warn-on-reflection* true)

(def ^:dynamic *fps-debug* nil)

; Replacement for Screenshot/readToBufferedImage but without expensive y-axis flip.
; We flip in JavaFX instead
(defn- read-to-buffered-image [^long w ^long h]
  (let [image (BufferedImage. w h BufferedImage/TYPE_3BYTE_BGR)
        glc (GLContext/getCurrent)
        gl (.getGL glc)
        psm (GLPixelStorageModes.)]
   (.setPackAlignment psm gl 1)
   (.glReadPixels gl 0 0 w h GL2GL3/GL_BGR GL/GL_UNSIGNED_BYTE (ByteBuffer/wrap (.getData ^DataBufferByte (.getDataBuffer (.getRaster image)))))
   (.restore psm gl)
   image))

(def INDEX_SHIFT 4)

(def outline-color [(/ 43.0 255) (/ 25.0 255) (/ 116.0 255)])
(def selected-outline-color [(/ 69.0 255) (/ 255.0 255) (/ 162.0 255)])

(defn select-color [pass selected object-color]
  (if (or (= pass pass/outline) (= pass pass/icon-outline))
    (if selected selected-outline-color outline-color)
    object-color))

(defn vp-dims [^Region viewport]
  (let [w (- (.right viewport) (.left viewport))
        h (- (.bottom viewport) (.top viewport))]
    [w h]))

(defn vp-not-empty? [^Region viewport]
  (let [[w h] (vp-dims viewport)]
    (and (> w 0) (> h 0))))

(defn z-distance [camera viewport obj]
  (let [p (->> (Point3d.)
            (geom/world-space obj)
            (c/camera-project camera viewport))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera viewport obj]
  (- Long/MAX_VALUE
     (+ (z-distance camera viewport obj)
        (bit-shift-left (:index obj 0) INDEX_SHIFT))))

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn setup-pass
  ([context gl glu pass camera ^Region viewport]
    (setup-pass context gl glu pass camera viewport nil))
  ([context ^GL2 gl ^GLU glu pass camera ^Region viewport pick-rect]
    (.glMatrixMode gl GL2/GL_PROJECTION)
      (.glLoadIdentity gl)
      (when pick-rect
        (gl/glu-pick-matrix glu pick-rect viewport))
      (if (types/model-transform? pass)
        (gl/gl-mult-matrix-4d gl (c/camera-projection-matrix camera))
        (gl/glu-ortho glu viewport))
      (.glMatrixMode gl GL2/GL_MODELVIEW)
      (.glLoadIdentity gl)
      (when (types/model-transform? pass)
        (gl/gl-load-matrix-4d gl (c/camera-view-matrix camera)))
      (pass/prepare-gl pass gl glu)))

(defrecord TextRendererRef [^TextRenderer text-renderer ^GLContext context]
  clojure.lang.IDeref
  (deref [this] text-renderer)
  g/IDisposable
  (g/dispose [this]
    (prn "disposing text-renderer")
    (when context (.makeCurrent context))
    (.dispose text-renderer)
    (when context (.release context))))

(defmethod print-method TextRendererRef
  [^TextRendererRef v ^java.io.Writer w]
  (.write w (str "<TextRendererRef@" (:text-renderer v) ">")))

(g/defnk produce-drawable [self ^Region viewport]
  (when (vp-not-empty? viewport)
    (let [[w h]   (vp-dims viewport)
          profile (GLProfile/getDefault)
          factory (GLDrawableFactory/getFactory profile)
          caps    (GLCapabilities. profile)]
      (.setOnscreen caps false)
      (.setPBuffer caps true)
      (.setDoubleBuffered caps false)
      (let [^GLOffscreenAutoDrawable drawable (:gl-drawable self)
            drawable (if drawable
                       (do (.setSize drawable w h) drawable)
                       (.createOffscreenAutoDrawable factory nil caps nil w h nil))]
        (g/transact (g/set-property self :gl-drawable drawable))
        drawable))))

(defn- make-current [^Region viewport ^GLAutoDrawable drawable]
  (when (and drawable (vp-not-empty? viewport))
    (when-let [^GLContext context (.getContext drawable)]
      (doto context (.makeCurrent)))))

(defn render-nodes
  ([^GL2 gl render-args renderables count]
    (render-nodes gl render-args renderables count nil))
  ([^GL2 gl render-args renderables count gl-name]
    (when-let [render-fn (:render-fn (first renderables))]
      (try
        (when gl-name
          (.glPushName gl gl-name))
        (render-fn gl render-args renderables count)
        (catch Exception e
          (log/error :exception e
                     :pass (:pass render-args)
                     :render-fn render-fn
                     :message "skipping renderable"))
        (finally
          (when gl-name
          (.glPopName gl)))))))

(defn batch-render [gl render-args renderables gl-names? key-fn]
  (loop [renderables renderables
         offset 0
         batch-index 0
         batches (transient [])]
    (if-let [renderable (first renderables)]
      (let [first-key (key-fn renderable)
            first-render-fn (:render-fn renderable)
            count (loop [renderables (rest renderables)
                         count 1]
                    (let [renderable (first renderables)
                          key (key-fn renderable)
                          render-fn (:render-fn renderable)
                          break? (or (not= first-render-fn render-fn) (nil? first-key) (nil? key) (not= first-key key))]
                      (if break?
                        count
                        (recur (rest renderables) (inc count)))))]
        (when (> count 0)
          (let [gl-name (if gl-names? batch-index nil)]
            (render-nodes gl render-args (subvec renderables 0 count) count gl-name))
          (let [end (+ offset count)]
            ; TODO - long conversion should not be necessary?
            (recur (subvec renderables count) (long end) (inc batch-index) (conj! batches [offset end])))))
      (persistent! batches))))

(defn- render-sort [renderables camera viewport]
  (sort-by #(render-key camera viewport %) renderables))

(g/defnk produce-frame [^Region viewport ^GLAutoDrawable drawable camera ^TextRendererRef text-renderer renderables tool-renderables]
  (when-let [^GLContext context (make-current viewport drawable)]
    (let [gl ^GL2 (.getGL context)
          glu ^GLU (GLU.)
          render-args {:glu glu :camera camera :viewport viewport :text-renderer @text-renderer}
          renderables (apply merge-with (fn [renderables tool-renderables] (apply conj renderables tool-renderables)) renderables tool-renderables)]
      (.glClearColor gl 0.0 0.0 0.0 1.0)
      (gl/gl-clear gl 0.0 0.0 0.0 1)
      (.glColor4f gl 1.0 1.0 1.0 1.0)
      (gl-viewport gl viewport)
      (doseq [pass pass/render-passes
              :let [render-args (assoc render-args :pass pass)]]
        (setup-pass context gl glu pass camera viewport)
        (batch-render gl render-args (get renderables pass) false :batch-key))
      (let [[w h] (vp-dims viewport)
            buf-image (read-to-buffered-image w h)]
        (.release context)
        buf-image))))

(def pick-buffer-size 4096)

(defn- begin-select [^GL2 gl select-buffer]
  (.glSelectBuffer gl pick-buffer-size select-buffer)
  (.glRenderMode gl GL2/GL_SELECT)
  (.glInitNames gl))

(defn- unsigned-int [v]
  (unsigned-bit-shift-right (bit-shift-left (long v) 32) 32))

(defn- parse-select-buffer [hits ^IntBuffer select-buffer]
  (loop [offset 0
        hits-left hits
        selected-names []]
   (if (> hits-left 0)
     (let [name-count (int (.get select-buffer offset))
           min-z (unsigned-int (.get select-buffer (+ offset 1)))
           name (int (.get select-buffer (+ offset 3)))]
       (recur (inc (+ name-count offset 2)) (dec hits-left) (conj selected-names name)))
     selected-names)))

(defn- end-select [^GL2 gl select-buffer renderables batches]
  (.glFlush gl)
  (let [hits (.glRenderMode gl GL2/GL_RENDER)
        selected-names (parse-select-buffer hits select-buffer)]
    (loop [names selected-names
           selected (transient [])]
      (if-let [name (first names)]
        (let [batch (get batches name)]
          (doseq [renderable (subvec renderables (first batch) (second batch))]
            (conj! selected renderable))
          (recur (rest names) selected))
        (persistent! selected)))))

(g/defnk produce-selection [renderables ^GLAutoDrawable drawable viewport camera ^Rect picking-rect ^IntBuffer select-buffer selection]
  (if-let [^GLContext context (and picking-rect (make-current viewport drawable))]
    (try
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)
            render-args {:glu glu :camera camera :viewport viewport}
            selection-set (set selection)]
        (flatten
          (for [pass pass/selection-passes
                :let [render-args (assoc render-args :pass pass)]]
            (do
              (begin-select gl select-buffer)
              (setup-pass context gl glu pass camera viewport picking-rect)
              (let [renderables (get renderables pass)
                    batches (batch-render gl render-args renderables true :select-batch-key)]
                (render-sort (end-select gl select-buffer renderables batches) camera viewport))))))
      (finally
        (.release context)))
    []))

(g/defnk produce-tool-selection [tool-renderables ^GLAutoDrawable drawable viewport camera ^Rect tool-picking-rect ^IntBuffer select-buffer]
  (if-let [^GLContext context (and tool-picking-rect (make-current viewport drawable))]
    (try
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)
            render-args {:gl gl :glu glu :camera camera :viewport viewport}
            tool-renderables (apply merge-with concat tool-renderables)]
        (flatten
          (let [pass pass/manipulator-selection
                render-args (assoc render-args :pass pass)]
            (begin-select gl select-buffer)
            (setup-pass context gl glu pass camera viewport tool-picking-rect)
            (let [renderables (get tool-renderables pass)
                  batches (batch-render gl render-args renderables true :select-batch-key)]
              (render-sort (end-select gl select-buffer renderables batches) camera viewport)))))
      (finally
        (.release context)))
    []))

(g/defnk produce-selected-tool-renderables [tool-selection]
  (apply merge-with concat {} (map #(do {(:node-id %) [(:selection-data %)]}) tool-selection)))

(defn flatten-scene [scene selection-set ^Matrix4d world-transform out-renderables out-selected-renderables]
 (let [renderable (:renderable scene)
       trans-tmpl ^Matrix4d (or (:transform scene) geom/Identity4d)
       transform (Matrix4d. ^Matrix4d trans-tmpl)
       world-transform (doto (Matrix4d. world-transform) (.mul transform))
       selected (contains? selection-set (:node-id scene))
       new-renderable (assoc (dissoc scene :renderable) :render-fn (:render-fn renderable) :world-transform world-transform :selected selected :user-data (:user-data renderable) :batch-key (:batch-key renderable))]
   (doseq [pass (:passes renderable)]
     (conj! (get out-renderables pass) new-renderable)
     (when (and selected (types/selection? pass))
       (conj! out-selected-renderables new-renderable)))
   (doseq [child-scene (:children scene)]
     (flatten-scene child-scene selection-set world-transform out-renderables out-selected-renderables))))

(defn produce-render-data [scene selection aux-renderables camera viewport]
  (let [selection-set (set selection)
        out-renderables (into {} (map #(do [% (transient [])]) pass/all-passes))
        out-selected-renderables (transient [])
        world-transform (doto (Matrix4d.) (.setIdentity))
        render-data (flatten-scene scene selection-set world-transform out-renderables out-selected-renderables)
        out-renderables (merge-with (fn [renderables extras] (doseq [extra extras] (conj! renderables extra)) renderables) out-renderables (apply merge-with concat aux-renderables))
        out-renderables (into {} (map (fn [[pass renderables]] [pass (vec (render-sort (persistent! renderables) camera viewport))]) out-renderables))
        out-selected-renderables (persistent! out-selected-renderables)]
    {:all-renderables out-renderables
     :selected-renderables out-selected-renderables}))

(g/defnode SceneRenderer
  (property name g/Keyword (default :renderer))
  (property gl-drawable GLAutoDrawable)

  (input scene g/Any)
  (input selection g/Any)
  (input viewport Region)
  (input camera Camera)
  (input aux-renderables pass/RenderData :array)
  (input tool-renderables pass/RenderData :array)
  (input picking-rect Rect)
  (input tool-picking-rect Rect)

  (output render-data g/Any :cached (g/fnk [scene selection aux-renderables camera viewport] (produce-render-data scene selection aux-renderables camera viewport)))
  (output renderables pass/RenderData :cached (g/fnk [render-data] (:all-renderables render-data)))
  (output select-buffer IntBuffer :cached (g/fnk [] (-> (ByteBuffer/allocateDirect (* 4 pick-buffer-size))
                                                      (.order (ByteOrder/nativeOrder))
                                                      (.asIntBuffer))))
  (output drawable (g/maybe GLAutoDrawable) :cached produce-drawable)
  (output text-renderer TextRendererRef :cached (g/fnk [^GLAutoDrawable drawable] (->TextRendererRef (gl/text-renderer Font/SANS_SERIF Font/BOLD 12) (if drawable (.getContext drawable) nil))))
  (output frame BufferedImage :cached produce-frame)
  (output picking-selection g/Any :cached produce-selection)
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-renderables g/Any :cached (g/fnk [render-data] (:selected-renderables render-data)))
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables))

(defn dispatch-input [input-handlers action user-data]
  (reduce (fn [action input-handler]
            (let [node (first input-handler)
                  label (second input-handler)]
              (when action
                ((g/node-value node label) node action user-data))))
          action input-handlers))

(g/defnode SceneView
  (inherits core/Scope)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property repainter AnimationTimer)
  (property picking-rect (g/maybe Rect))

  (input frame BufferedImage)
  (input scene g/Any)
  (input input-handlers Runnable :array)
  (input selection g/Any)
  (input selected-tool-renderables g/Any)
  (input active-tool g/Keyword)
  (output active-tool g/Keyword (g/fnk [active-tool] active-tool))

  (output scene g/Any (g/fnk [scene] scene))
  (output image WritableImage :cached (g/fnk [^BufferedImage frame ^ImageView image-view] (when frame (SwingFXUtils/toFXImage frame (.getImage image-view)))))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene (geom/null-aabb)))) ; TODO - base aabb on selection
  (output selection g/Any (g/fnk [selection] selection))
  (output picking-rect Rect (g/fnk [picking-rect] picking-rect))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil))
  g/IDisposable
  (g/dispose [self]
           (prn "Disposing SceneEditor")
           (when-let [^GLAutoDrawable drawable (:gl-drawable self)]
             (.destroy drawable))
           ))

(def ^Integer min-pick-size 10)

(defn calc-picking-rect [start current]
  (let [ps [start current]
        min-fn (fn [^Integer v1 ^Integer v2] (Math/min v1 v2))
        max-fn (fn [^Integer v1 ^Integer v2] (Math/max v1 v2))
        min-p (Point2i. (reduce min-fn (map first ps)) (reduce min-fn (map second ps)))
        max-p (Point2i. (reduce max-fn (map first ps)) (reduce max-fn (map second ps)))
        dims (doto (Point2i. max-p) (.sub min-p))
        center (doto (Point2i. min-p) (.add (Point2i. (/ (.x dims) 2) (/ (.y dims) 2))))]
    (Rect. nil (.x center) (.y center) (Math/max (.x dims) min-pick-size) (Math/max (.y dims) min-pick-size))))

(defn- screen->world [camera viewport ^Vector3d screen-pos] ^Vector3d
  (let [w4 (c/camera-unproject camera viewport (.x screen-pos) (.y screen-pos) (.z screen-pos))]
    (Vector3d. (.x w4) (.y w4) (.z w4))))

(defn augment-action [view action]
  (let [x (:x action)
        y (:y action)
        screen-pos (Vector3d. x y 0)
        view-graph (g/node->graph-id view)
        camera (g/node-value (g/graph-value view-graph :camera) :camera)
        viewport (g/node-value view :viewport)
        world-pos (Point3d. ^Vector3d (screen->world camera viewport screen-pos))
        world-dir (doto ^Vector3d (screen->world camera viewport (doto (Vector3d. screen-pos) (.setZ 1)))
                    (.sub world-pos)
                    (.normalize))]
    (assoc action
           :screen-pos screen-pos
           :world-pos world-pos
           :world-dir world-dir)))

(defn- flip-y [^Node node height]
  (let [l (.getTransforms node)]
    (.clear l)
    (.add l (javafx.scene.transform.Rotate. 180 0 (/ height 2) 0 (javafx.geometry.Point3D. 1 0 0)))))

(when *fps-debug*
  (def fps-counter (agent (long-array 3 0)))

  (defn tick [^longs fps-counts now]
    (let [last-report-time (aget fps-counts 1)
          frame-count (inc (aget fps-counts 0))]
      (aset-long fps-counts 0 frame-count)
      (when (> now (+ last-report-time 1000000000))
        (do (println "FPS" frame-count))
        (aset-long fps-counts 1 now)
        (aset-long fps-counts 0 0)))
    fps-counts))

(defn- make-scene-view [scene-graph ^Parent parent opts]
  (let [image-view (ImageView.)]
    (.add (.getChildren ^Pane parent) image-view)
    (let [view (g/make-node! scene-graph SceneView :image-view image-view)]
      (let [node-id (g/node-id view)
            tool-user-data (atom [])
            event-handler (reify EventHandler (handle [this e]
                                                (let [self (g/node-by-id node-id)
                                                      action (augment-action self (i/action-from-jfx e))
                                                      x (:x action)
                                                      y (:y action)
                                                      pos [x y 0.0]
                                                      picking-rect (calc-picking-rect pos pos)]
                                                  ; Only look for tool selection when the mouse is moving with no button pressed
                                                  (when (and (= :mouse-moved (:type action)) (= 0 (:click-count action)))
                                                    (reset! tool-user-data (g/node-value self :selected-tool-renderables)))
                                                  (g/transact (g/set-property self :picking-rect picking-rect))
                                                  (dispatch-input (g/sources-of self :input-handlers) action @tool-user-data))))
            change-listener (reify ChangeListener (changed [this observable old-val new-val]
                                                    (Platform/runLater
                                                     (fn []
                                                       (let [bb ^BoundingBox (.getBoundsInParent (.getParent parent))
                                                             w (- (.getMaxX bb) (.getMinX bb))
                                                             h (- (.getMaxY bb) (.getMinY bb))]
                                                         (flip-y (:image-view (g/node-by-id node-id)) h)
                                                         (g/transact (g/set-property node-id :viewport (types/->Region 0 w 0 h))))))))]
        (.setOnMousePressed parent event-handler)
        (.setOnMouseReleased parent event-handler)
        (.setOnMouseClicked parent event-handler)
        (.setOnMouseMoved parent event-handler)
        (.setOnMouseDragged parent event-handler)
        (.setOnScroll parent event-handler)
        (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)

        (let [fps-counter (when *fps-debug* (agent (long-array 3 0)))
              ^Tab tab    (:tab opts)
              repainter   (proxy [AnimationTimer] []
                            (handle [now]
                              (when *fps-debug* (send-off fps-counter tick now))
                              (let [self                  (g/node-by-id node-id)
                                    image-view ^ImageView (:image-view self)
                                    visible               (.isSelected tab)]
                                (when (and visible)
                                  (try
                                    (let [image (g/node-value self :image)]
                                      (when (not= image (.getImage image-view)) (.setImage image-view image)))
                                    (catch Exception e
                                      (.setImage image-view nil)
                                      (.stop ^AnimationTimer this)))))))]
          (g/transact (g/set-property view :repainter repainter))
          (.start repainter)))
      (g/refresh view))))


(g/defnode PreviewView
  (inherits core/Scope)

  (property width g/Num)
  (property height g/Num)
  (property picking-rect Rect)

  (input selection g/Any)
  (input selected-tool-renderables g/Any)
  (input scene g/Any)
  (input frame BufferedImage)
  (input input-handlers Runnable :array)
  (input active-tool g/Keyword)
  (output active-tool g/Keyword (g/fnk [active-tool] active-tool))

  (output scene g/Any (g/fnk [scene] scene))
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output viewport Region (g/fnk [width height] (types/->Region 0 width 0 height)))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene (geom/null-aabb))))
  (output selection g/Any :cached (g/fnk [selection] selection))
  (output picking-rect Rect :cached (g/fnk [picking-rect] picking-rect)))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView :width width :height height))

(defn render-selection-box [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        start (:start user-data)
        current (:current user-data)]
    (when (and start current)
     (let [min-fn (fn [v1 v2] (map #(Math/min ^Double %1 ^Double %2) v1 v2))
           max-fn (fn [v1 v2] (map #(Math/max ^Double %1 ^Double %2) v1 v2))
           min-p (reduce min-fn [start current])
           min-x (nth min-p 0)
           min-y (nth min-p 1)
           max-p (reduce max-fn [start current])
           max-x (nth max-p 0)
           max-y (nth max-p 1)
           z 0.0
           c (double-array (map #(/ % 255.0) [131 188 212]))]
       (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
       (.glBegin gl GL2/GL_LINE_LOOP)
       (.glVertex3d gl min-x min-y z)
       (.glVertex3d gl min-x max-y z)
       (.glVertex3d gl max-x max-y z)
       (.glVertex3d gl max-x min-y z)
       (.glEnd gl)

       (.glBegin gl GL2/GL_QUADS)
       (.glColor4d gl (nth c 0) (nth c 1) (nth c 2) 0.2)
       (.glVertex3d gl min-x, min-y, z);
       (.glVertex3d gl min-x, max-y, z);
       (.glVertex3d gl max-x, max-y, z);
       (.glVertex3d gl max-x, min-y, z);
       (.glEnd gl)))))

(defn- select [controller op-seq mode]
  (let [controller (g/refresh controller)
        select-fn (g/node-value controller :select-fn)
        selection (g/node-value controller :picking-selection)
        sel-filter-fn (case mode
                        :direct (fn [selection] selection)
                        :toggle (fn [selection]
                                  (let [selection-set (set selection)
                                        prev-selection-set (g/node-value controller :prev-selection-set)]
                                    (seq (set/union (set/difference prev-selection-set selection-set) (set/difference selection-set prev-selection-set))))))
        selection (or (not-empty (sel-filter-fn (map :node-id selection))) (filter #(not (nil? %)) [(:node-id (g/node-value controller :scene))]))]
    (select-fn (map #(g/node-by-id %) selection) op-seq)))

(def mac-toggle-modifiers #{:shift :meta})
(def other-toggle-modifiers #{:control})
(def toggle-modifiers (if util/mac? mac-toggle-modifiers other-toggle-modifiers))

(defn handle-selection-input [self action user-data]
  (let [start (g/node-value self :start)
        current (g/node-value self :current)
        op-seq (g/node-value self :op-seq)
        mode (g/node-value self :mode)
        cursor-pos [(:x action) (:y action) 0]]
    (case (:type action)
      :mouse-pressed (let [op-seq (gensym)
                           toggle (reduce #(or %1 %2) (map #(% action) toggle-modifiers))
                           mode (if toggle :toggle :direct)]
                       (g/transact
                         (concat
                           (g/set-property self :op-seq op-seq)
                           (g/set-property self :start cursor-pos)
                           (g/set-property self :current cursor-pos)
                           (g/set-property self :mode mode)
                           (g/set-property self :prev-selection-set (set (g/node-value self :selection)))))
                       (select self op-seq mode)
                       nil)
      :mouse-released (do
                        (g/transact
                          (concat
                            (g/set-property self :start nil)
                            (g/set-property self :current nil)
                            (g/set-property self :op-seq nil)
                            (g/set-property self :mode nil)
                            (g/set-property self :prev-selection-set nil)))
                        nil)
      :mouse-moved (if start
                     (do
                       (g/transact (g/set-property self :current cursor-pos))
                       (select self op-seq mode)
                       nil)
                     action)
      action)))

(g/defnk produce-picking-rect [start current]
  (calc-picking-rect start current))

(g/defnode SelectionController
  (property select-fn Runnable)
  (property start (g/maybe types/Vec3))
  (property current (g/maybe types/Vec3))
  (property op-seq g/Any)
  (property mode (g/maybe (g/enum :direct :toggle)))
  (property prev-selection-set g/Any)

  (input selection g/Any)
  (input picking-selection g/Any)
  (input scene g/Any)

  (output picking-rect Rect :cached produce-picking-rect)
  (output renderable pass/RenderData :cached (g/fnk [start current] {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                                                                                    :render-fn render-selection-box
                                                                                    :user-data {:start start :current current}}]}))
  (output input-handler Runnable :cached (g/fnk [] handle-selection-input)))

(defn setup-view [view resource-node opts]
  (let [view-graph (g/node->graph-id view)
        app-view   (:app-view opts)
        project    (:project opts)]
    (g/make-nodes view-graph
                  [renderer   SceneRenderer
                   selection  [SelectionController :select-fn (fn [selection op-seq] (project/select! project selection op-seq))]
                   background background/Gradient
                   camera     [c/CameraController :camera (or (:camera opts) (c/make-camera :orthographic)) :reframe true]
                   grid       grid/Grid
                   tool-controller scene-tools/ToolController]
                  (g/update-property camera  :movements-enabled disj :tumble) ; TODO - pass in to constructor

                  (g/connect resource-node :scene view :scene)
                  (g/connect resource-node :scene selection :scene)
                  (g/set-graph-value view-graph :renderer renderer)
                  (g/set-graph-value view-graph :camera   camera)

                  (g/connect background      :renderable        renderer        :aux-renderables)
                  (g/connect camera          :camera            renderer        :camera)
                  (g/connect camera          :input-handler     view            :input-handlers)
                  (g/connect view            :aabb              camera          :aabb)
                  (g/connect view            :viewport          camera          :viewport)
                  (g/connect view            :viewport          renderer        :viewport)
                  (g/connect view            :scene             renderer        :scene)

                  (g/connect project         :selected-node-ids view            :selection)
                  (g/connect view            :selection         renderer        :selection)
                  (g/connect renderer        :frame             view            :frame)

                  (g/connect tool-controller :input-handler     view            :input-handlers)

                  (g/connect selection       :renderable        renderer        :tool-renderables)
                  (g/connect selection       :input-handler     view            :input-handlers)
                  (g/connect selection       :picking-rect      renderer        :picking-rect)
                  (g/connect renderer        :picking-selection selection       :picking-selection)
                  (g/connect view            :selection         selection       :selection)
                  (g/connect view            :picking-rect      renderer        :tool-picking-rect)
                  (g/connect renderer        :selected-tool-renderables view    :selected-tool-renderables)

                  (g/connect grid   :renderable renderer :aux-renderables)
                  (g/connect camera :camera     grid     :camera)


                  (g/connect tool-controller :renderables renderer :tool-renderables)

                  (g/connect app-view :active-tool view :active-tool)
                  (g/connect view :active-tool tool-controller :active-tool)
                  (g/connect view :viewport    tool-controller          :viewport)
                  (g/connect camera :camera tool-controller :camera)
                  (g/connect renderer :selected-renderables tool-controller :selected-renderables)
                  (when (not (:grid opts))
                    (g/delete-node grid)))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view (make-scene-view graph parent opts)]
    (g/transact
      (setup-view view resource-node opts))
    view))

(defn make-preview [graph resource-node opts width height]
  (let [view (make-preview-view graph width height)]
    (g/transact
      (setup-view view resource-node (dissoc opts :grid)))
    view))

(defn register-view-types [workspace]
                               (workspace/register-view-type workspace
                                                             :id :scene
                                                             :make-view-fn make-view
                                                             :make-preview-fn make-preview))

(g/defnode SceneNode
  (property position types/Vec3 (default [0 0 0]))
  (property rotation types/Vec3 (default [0 0 0]))

  (output position Vector3d :cached (g/fnk [^types/Vec3 position] (Vector3d. (double-array position))))
  (output rotation Quat4d :cached (g/fnk [^types/Vec3 rotation] (math/euler->quat rotation)))
  (output transform Matrix4d :cached (g/fnk [^Vector3d position ^Quat4d rotation] (Matrix4d. rotation position 1.0)))
  (output scene g/Any :cached (g/fnk [^g/NodeID node-id ^Matrix4d transform] {:node-id node-id :transform transform}))
  (output aabb AABB :cached (g/fnk [] (geom/null-aabb)))

  scene-tools/Movable
  (scene-tools/move [self delta] (let [p (doto (Vector3d. (double-array (:position self))) (.add delta))]
                                   (g/set-property self :position [(.x p) (.y p) (.z p)])))
  scene-tools/Rotatable
  (scene-tools/rotate [self delta] (let [new-rotation (doto (Quat4d. ^Quat4d (math/euler->quat (:rotation self))) (.mul delta))
                                         new-euler (math/quat->euler new-rotation)]
                                     (g/set-property self :rotation new-euler))))

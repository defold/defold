(ns editor.scene
  (:require [clojure.set :as set]
            [dynamo.background :as background]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.graph :as g]
            [dynamo.grid :as grid]
            [dynamo.types :as t]
            [dynamo.types :refer [IDisposable dispose]]
            [dynamo.util :as util]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.input :as i]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.scene-tools :as scene-tools]
            [internal.render.pass :as pass]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util.awt TextRenderer Screenshot]
           [dynamo.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
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
           [javax.media.opengl GL GL2 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]))

(set! *warn-on-reflection* true)

(def PASS_SHIFT        32)
(def INDEX_SHIFT       (+ PASS_SHIFT 4))
(def MANIPULATOR_SHIFT 62)

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
  (+ (z-distance camera viewport obj)
     (bit-shift-left (or (:index obj) 0)        INDEX_SHIFT)
     (bit-shift-left (or (:manipulator? obj) 0) MANIPULATOR_SHIFT)))

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
      (if (t/model-transform? pass)
        (gl/gl-mult-matrix-4d gl (c/camera-projection-matrix camera))
        (gl/glu-ortho glu viewport))
      (.glMatrixMode gl GL2/GL_MODELVIEW)
      (.glLoadIdentity gl)
      (when (t/model-transform? pass)
        (gl/gl-load-matrix-4d gl (c/camera-view-matrix camera)))
      (pass/prepare-gl pass gl glu)))

(defn render-node
  ([^GL2 gl pass renderable render-args]
    (render-node gl pass renderable render-args nil))
  ([^GL2 gl pass renderable render-args gl-name]
    (gl/gl-push-matrix
      gl
      (when gl-name
        (.glPushName gl gl-name))
      (when (t/model-transform? pass)
        (gl/gl-mult-matrix-4d gl (:world-transform renderable)))
      (try
        (when (:render-fn renderable)
          ((:render-fn renderable) render-args))
        (catch Exception e
          (log/error :exception e
                     :pass pass
                     :render-fn (:render-fn renderable)
                     :message "skipping renderable"))
        (finally
          (when gl-name
            (.glPopName gl)))))))

(defrecord TextRendererRef [^TextRenderer text-renderer ^GLContext context]
  clojure.lang.IDeref
  (deref [this] text-renderer)
  IDisposable
  (dispose [this]
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

(defn- render-sort [renderables camera viewport]
  (reverse (sort-by #(render-key camera viewport %) renderables)))

(g/defnk produce-frame [^Region viewport ^GLAutoDrawable drawable camera ^TextRendererRef text-renderer renderables selection]
  (when-let [^GLContext context (make-current viewport drawable)]
    (let [gl ^GL2 (.getGL context)
          glu ^GLU (GLU.)
          render-args {:gl gl :glu glu :camera camera :viewport viewport :text-renderer @text-renderer}
          selection-set (set (map g/node-id selection))]
      (.glClearColor gl 0.0 0.0 0.0 1.0)
      (gl/gl-clear gl 0.0 0.0 0.0 1)
      (.glColor4f gl 1.0 1.0 1.0 1.0)
      (gl-viewport gl viewport)
      (doseq [pass pass/render-passes
              :let [render-args (assoc render-args :pass pass)]]
        (setup-pass context gl glu pass camera viewport)
        (doseq [renderable (get renderables pass)
                :let [id (:id renderable)]]
          (render-node gl pass renderable (assoc render-args :selected (selection-set id)))))
      (let [[w h] (vp-dims viewport)
            buf-image (Screenshot/readToBufferedImage w h)]
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

(defn- end-select [^GL2 gl select-buffer renderables]
  (.glFlush gl)
  (let [hits (.glRenderMode gl GL2/GL_RENDER)
        selected-names (parse-select-buffer hits select-buffer)]
    (map #(nth renderables %) selected-names)))

(g/defnk produce-selection [renderables ^GLAutoDrawable drawable viewport camera ^Rect picking-rect ^IntBuffer select-buffer selection]
  (if-let [^GLContext context (and picking-rect (make-current viewport drawable))]
    (try
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)
            render-args {:gl gl :glu glu :camera camera :viewport viewport}
            selection-set (set (map g/node-id selection))]
        (flatten
          (for [pass pass/selection-passes
                :let [render-args (assoc render-args :pass pass)]]
            (do
              (begin-select gl select-buffer)
              (setup-pass context gl glu pass camera viewport picking-rect)
              (let [renderables (get renderables pass)]
                (doseq [[index renderable] (keep-indexed #(list %1 %2) renderables)]
                  (render-node gl pass renderable (assoc render-args :selected (selection-set (:id renderable))) index))
                (render-sort (end-select gl select-buffer renderables) camera viewport))))))
      (finally
        (.release context)))
    []))

(g/defnk produce-tool-selection [renderables ^GLAutoDrawable drawable viewport camera ^Rect tool-picking-rect ^IntBuffer select-buffer]
  (if-let [^GLContext context (and tool-picking-rect (make-current viewport drawable))]
    (try
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)
            render-args {:gl gl :glu glu :camera camera :viewport viewport}]
        (flatten
          (let [pass pass/manipulator-selection
                render-args (assoc render-args :pass pass)]
            (begin-select gl select-buffer)
            (setup-pass context gl glu pass camera viewport tool-picking-rect)
            (let [renderables (get renderables pass)]
              (doseq [[index renderable] (keep-indexed #(list %1 %2) renderables)]
                (render-node gl pass renderable (assoc render-args :selected false) index))
              (render-sort (end-select gl select-buffer renderables) camera viewport)))))
      (finally
        (.release context)))
    []))

(g/defnk produce-renderables [renderables camera viewport]
  (let [renderables (apply merge-with #(concat %1 %2) renderables)]
    (into {} (map (fn [[pass renderables]] [pass (render-sort renderables camera viewport)]) renderables))))

(g/defnk produce-selected-renderables [selection selection-renderables]
  (let [renderables (apply merge-with #(concat %1 %2) selection-renderables)
        renderable-by-id (into {} (mapcat (fn [[pass v]] (map #(do [(:id %) %]) v)) renderables))]
    (filter (comp not nil?) (map #(get renderable-by-id (g/node-id %)) selection))))

(g/defnk produce-selected-tool-renderables [tool-selection renderables]
  (apply merge-with concat {} (map #(do {(:id %) [(:user-data %)]}) tool-selection)))

(g/defnode SceneRenderer
  (property name t/Keyword (default :renderer))
  (property gl-drawable GLAutoDrawable)

  (input selection t/Any)
  (input viewport Region)
  (input camera Camera)
  (input renderables pass/RenderData :array)
  (input selection-renderables pass/RenderData :array)
  (input picking-rect Rect)
  (input tool-picking-rect Rect)

  (output renderables pass/RenderData :cached produce-renderables)
  (output select-buffer IntBuffer :cached (g/fnk [] (-> (ByteBuffer/allocateDirect (* 4 pick-buffer-size))
                                                      (.order (ByteOrder/nativeOrder))
                                                      (.asIntBuffer))))
  (output drawable (t/maybe GLAutoDrawable) :cached produce-drawable)
  (output text-renderer TextRendererRef :cached (g/fnk [^GLAutoDrawable drawable] (->TextRendererRef (gl/text-renderer Font/SANS_SERIF Font/BOLD 12) (if drawable (.getContext drawable) nil))))
  (output frame BufferedImage :cached produce-frame)
  (output picking-selection t/Any :cached produce-selection)
  (output tool-selection t/Any :cached produce-tool-selection)
  (output selected-renderables t/Any produce-selected-renderables)
  (output selected-tool-renderables t/Any produce-selected-tool-renderables))

(defn dispatch-input [input-handlers action user-data]
  (reduce (fn [action input-handler]
            (let [node (first input-handler)
                  label (second input-handler)]
              (when action
                ((g/node-value node label) node action user-data))))
          action input-handlers))

(defn- apply-transform [^Matrix4d transform renderables]
  (let [apply-tx (fn [renderable]
                   (let [^Matrix4d world-transform (or (:world-transform renderable) (doto (Matrix4d.) (.setIdentity)))]
                     (assoc renderable :world-transform (doto (Matrix4d. transform)
                                                          (.mul world-transform)))))]
    (into {} (map (fn [[pass lst]] [pass (map apply-tx lst)]) renderables))))

(defn- any-list? [v]
  (or (seq? v) (list? v) (vector? v)))

(defn scene->renderables [scene]
  (if (any-list? scene)
    (reduce #(merge-with concat %1 %2) {} (map scene->renderables scene))
    (let [{:keys [id transform aabb renderable children]} scene
          {:keys [render-fn passes]} renderable
          renderables (into {} (map (fn [pass] [pass [{:id id
                                                       :render-fn render-fn
                                                       :world-transform (doto (Matrix4d.) (.setIdentity))}]]) passes))
          renderables (reduce #(merge-with concat %1 %2) renderables (map scene->renderables children))]
      (if transform
        (apply-transform transform renderables)
        renderables))))

(defn- aabb [v]
  (:aabb v (geom/null-aabb)))

(g/defnk produce-aabb [scene]
  (if (any-list? scene)
    (reduce #(geom/aabb-union %1 (aabb %2)) (geom/null-aabb) scene)
    (aabb scene)))

(g/defnode SceneView
  (inherits core/Scope)

  (property image-view ImageView)
  (property viewport Region (default (t/->Region 0 0 0 0)))
  (property repainter AnimationTimer)
  (property visible t/Bool (default true))
  (property picking-rect Rect)

  (input frame BufferedImage)
  (input scene t/Any :array)
  (input input-handlers Runnable :array)
  (input selection t/Any)
  (input selected-tool-renderables t/Any)
  (input active-tool t/Keyword)
  (output active-tool t/Keyword (g/fnk [active-tool] active-tool))

  (output renderables pass/RenderData :cached (g/fnk [scene] (scene->renderables scene)))
  (output selection-renderables pass/RenderData :cached (g/fnk [renderables] (into {} (filter #(t/selection? (first %)) renderables))))
;;  (output viewport Region (g/fnk [viewport] viewport))
  (output image WritableImage :cached (g/fnk [frame ^ImageView image-view] (when frame (SwingFXUtils/toFXImage frame (.getImage image-view)))))
  (output aabb AABB :cached produce-aabb) ; TODO - base aabb on selection
  (output selection t/Any :cached (g/fnk [selection] selection))
  (output picking-rect Rect :cached (g/fnk [picking-rect] picking-rect))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil))
  t/IDisposable
  (dispose [self]
           (prn "Disposing SceneEditor")
           (when-let [^GLAutoDrawable drawable (:gl-drawable self)]
             (.destroy drawable))
           ))

(def ^Integer min-pick-size 10)

(defn- calc-picking-rect [start current]
  (let [ps [start current]
        min-fn (fn [^Integer v1 ^Integer v2] (Math/min v1 v2))
        max-fn (fn [^Integer v1 ^Integer v2] (Math/max v1 v2))
        min-p (Point2i. (reduce min-fn (map first ps)) (reduce min-fn (map second ps)))
        max-p (Point2i. (reduce max-fn (map first ps)) (reduce max-fn (map second ps)))
        dims (doto (Point2i. max-p) (.sub min-p))
        center (doto (Point2i. min-p) (.add (Point2i. (/ (.x dims) 2) (/ (.y dims) 2))))]
    (Rect. nil (.x center) (.y center) (Math/max (.x dims) min-pick-size) (Math/max (.y dims) min-pick-size))))

(defn- screen->world [view ^Vector3d screen-pos] ^Vector3d
  (let [view-graph (g/node->graph-id view)
        camera (g/node-value (g/graph-value view-graph :camera) :camera)
        viewport (g/node-value view :viewport)
        w4 (c/camera-unproject camera viewport (.x screen-pos) (.y screen-pos) (.z screen-pos))]
    (Vector3d. (.x w4) (.y w4) (.z w4))))

(defn make-scene-view [scene-graph ^Parent parent]
  (let [image-view (ImageView.)]
    (.add (.getChildren ^Pane parent) image-view)
    (let [view (g/make-node! scene-graph SceneView :image-view image-view)]
      (let [self-ref (g/node-id view)
            event-handler (reify EventHandler (handle [this e]
                                                (let [now (g/now)
                                                      self (g/node-by-id now self-ref)
                                                      action (i/action-from-jfx e)
                                                      x (:x action)
                                                      y (:y action)
                                                      screen-pos (Vector3d. x y 1)
                                                      world-pos (Point3d. ^Vector3d (screen->world self screen-pos))
                                                      world-dir (doto ^Vector3d (screen->world self (doto (Vector3d. screen-pos) (.setZ 0)))
                                                                  (.sub world-pos)
                                                                  (.normalize))
                                                      action (assoc action
                                                                    :screen-pos screen-pos
                                                                    :world-pos world-pos
                                                                    :world-dir world-dir)
                                                      pos [x y 0.0]
                                                      picking-rect (calc-picking-rect pos pos)]
                                                  (g/transact (g/set-property self :picking-rect picking-rect))
                                                  (dispatch-input (g/sources-of now self :input-handlers) action (g/node-value self :selected-tool-renderables)))))
            change-listener (reify ChangeListener (changed [this observable old-val new-val]
                                                    (let [bb ^BoundingBox new-val
                                                          w (- (.getMaxX bb) (.getMinX bb))
                                                          h (- (.getMaxY bb) (.getMinY bb))]
                                                      (g/transact (g/set-property self-ref :viewport (t/->Region 0 w 0 h))))))]
        (.setOnMousePressed parent event-handler)
        (.setOnMouseReleased parent event-handler)
        (.setOnMouseClicked parent event-handler)
        (.setOnMouseMoved parent event-handler)
        (.setOnMouseDragged parent event-handler)
        (.setOnScroll parent event-handler)
        (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)
        (let [repainter (proxy [AnimationTimer] []
                          (handle [now]
                            (let [self                  (g/node-by-id (g/now) self-ref)
                                  image-view ^ImageView (:image-view self)
                                  visible               (:visible self)]
                              (when (and visible)
                                (let [image (g/node-value self :image)]
                                  (when (not= image (.getImage image-view)) (.setImage image-view image)))))))]
          (g/transact (g/set-property view :repainter repainter))
          (.start repainter)))
      (g/refresh view))))

(g/defnode PreviewView
  (inherits core/Scope)

  (property width t/Num)
  (property height t/Num)

  (input selection t/Any)
  (input selected-tool-renderables t/Any)
  (input scene t/Any :array)
  (input frame BufferedImage)
  (input input-handlers Runnable :array)
  (input active-tool t/Keyword)
  (output active-tool t/Keyword (g/fnk [active-tool] active-tool))

  (output renderables pass/RenderData :cached (g/fnk [scene] (scene->renderables scene)))
  (output selection-renderables pass/RenderData :cached (g/fnk [renderables] (into {} (filter #(t/selection? (first %)) renderables))))
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output viewport Region (g/fnk [width height] (t/->Region 0 width 0 height)))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene)))
  (output selection t/Any :cached (g/fnk [selection] selection))
  (output picking-rect Rect :cached (g/fnk [] nil)))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView :width width :height height))

(defn render-selection-box [^GL2 gl start current]
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
      (.glEnd gl))))

(defn- select [controller op-seq mode]
  (let [controller (g/refresh controller)
        select-fn (g/node-value controller :select-fn)
        selection (g/node-value controller :picking-selection)
        sel-filter-fn (case mode
                        :direct (fn [sel] (map :id selection))
                        :toggle (fn [sel]
                                  (let [selection-set (set (map :id selection))
                                        prev-selection-set (g/node-value controller :prev-selection-set)]
                                    (seq (set/union (set/difference prev-selection-set selection-set) (set/difference selection-set prev-selection-set))))))
        selection (or (not-empty (sel-filter-fn selection)) (filter #(not (nil? %)) [(:id (g/node-value controller :scene))]))]
    (select-fn (map #(g/node-by-id (g/now) %) selection) op-seq)))

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
      :mouse-pressed (if (not (empty? user-data))
                       action
                       (let [op-seq (gensym)
                             toggle (reduce #(or %1 %2) (map #(% action) toggle-modifiers))
                             mode (if toggle :toggle :direct)]
                         (g/transact
                           (concat
                             (g/set-property self :op-seq op-seq)
                             (g/set-property self :start cursor-pos)
                             (g/set-property self :current cursor-pos)
                             (g/set-property self :mode mode)
                             (g/set-property self :prev-selection-set (set (map g/node-id (g/node-value self :selection))))))
                         (select self op-seq mode)
                         nil))
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
  (property start (t/maybe t/Vec3))
  (property current (t/maybe t/Vec3))
  (property op-seq t/Any)
  (property mode (t/maybe (t/enum :direct :toggle)))
  (property prev-selection-set t/Any)

  (input selection t/Any)
  (input picking-selection t/Any)
  (input scene t/Any)

  (output picking-rect Rect :cached produce-picking-rect)
  (output renderable pass/RenderData :cached (g/fnk [start current] {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d) :render-fn (g/fnk [gl] (render-selection-box gl start current))}]}))
  (output input-handler Runnable :cached (g/fnk [] handle-selection-input)))

(defn setup-view [view resource-node opts]
  (let [view-graph (g/node->graph-id view)]
    (g/make-nodes view-graph
                  [renderer   SceneRenderer
                   selection  [SelectionController :select-fn (:select-fn opts)]
                   background background/Gradient
                   camera     [c/CameraController :camera (or (:camera opts) (c/make-camera :orthographic)) :reframe true]
                   grid       grid/Grid
                   tool-controller [scene-tools/ToolController :active-tool :move]]
                  (g/update-property camera  :movements-enabled disj :tumble) ; TODO - pass in to constructor

                  (g/connect resource-node :scene view :scene)
                  (g/connect resource-node :scene selection :scene)
                  (g/set-graph-value view-graph :renderer renderer)
                  (g/set-graph-value view-graph :camera   camera)

                  (g/connect background      :renderable    renderer        :renderables)
                  (g/connect camera          :camera        renderer        :camera)
                  (g/connect camera          :input-handler view            :input-handlers)
                  (g/connect view            :aabb          camera          :aabb)
                  (g/connect view            :viewport      camera          :viewport)
                  (g/connect view            :viewport      renderer        :viewport)
                  (g/connect view            :renderables   renderer        :renderables)
                  (g/connect view            :selection-renderables   renderer        :selection-renderables)
                  (g/connect view            :selection     renderer        :selection)
                  (g/connect renderer        :frame         view            :frame)

                  (g/connect tool-controller :input-handler view :input-handlers)

                  (g/connect selection       :renderable        renderer        :renderables)
                  (g/connect selection       :input-handler     view            :input-handlers)
                  (g/connect selection       :picking-rect      renderer        :picking-rect)
                  (g/connect renderer        :picking-selection selection       :picking-selection)
                  (g/connect view            :selection         selection       :selection)
                  (g/connect view            :picking-rect      renderer        :tool-picking-rect)
                  (g/connect renderer        :selected-tool-renderables view    :selected-tool-renderables)

                  (g/connect grid   :renderable renderer :renderables)
                  (g/connect camera :camera     grid     :camera)

                  (g/connect tool-controller :renderables renderer :renderables)
                  (g/connect view :active-tool tool-controller :active-tool)
                  (g/connect view :viewport    tool-controller          :viewport)
                  (g/connect camera :camera tool-controller :camera)
                  (g/connect renderer :selected-renderables tool-controller :selected-renderables)
                  (when (not (:grid opts))
                    (g/delete-node grid)))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view (make-scene-view graph parent)]
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
  (property position t/Vec3 (default [0 0 0]))
  (property rotation t/Vec3 (default [0 0 0]))
  (property scale t/Num (default 1.0)) ; TODO - non-uniform scale

  (output position Vector3d :cached (g/fnk [position] (Vector3d. (double-array position))))
  (output rotation Quat4d :cached (g/fnk [rotation] (math/euler->quat rotation)))
  (output transform Matrix4d :cached (g/fnk [^Vector3d position ^Quat4d rotation ^double scale] (Matrix4d. rotation position scale)))
  (output scene t/Any :cached (g/fnk [self transform] {:id (g/node-id self) :transform transform}))
  (output aabb AABB :cached (g/fnk [] (geom/null-aabb)))
  
  scene-tools/Movable
  (scene-tools/move [self delta] (let [p (doto (Vector3d. (double-array (:position self))) (.add delta))]
                                   (g/set-property self :position [(.x p) (.y p) (.z p)])))
  scene-tools/Rotatable
  (scene-tools/rotate [self delta] (let [new-rotation (doto (Quat4d. ^Quat4d (math/euler->quat (:rotation self))) (.mul delta))
                                         new-euler (math/quat->euler new-rotation)]
                                     (g/set-property self :rotation new-euler))))

(ns editor.scene
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [dynamo.util :as util]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.input :as i]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene-cache :as scene-cache]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage DataBufferByte DataBufferInt]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox Pos]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Tab Button]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane StackPane]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]
           [sun.awt.image IntegerComponentRaster]))

(set! *warn-on-reflection* true)

(def ^:dynamic *fps-debug* nil)

(defn overlay-text [^GL2 gl ^String text x y]
  (let [^TextRenderer text-renderer (scene-cache/request-object! ::text-renderer ::overlay-text gl [Font/SANS_SERIF Font/BOLD 12])]
    (gl/overlay gl text-renderer text x y 1.0 1.0)))

(defonce ^:dynamic *controllers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defcontroller [controller & body]
  (let [qname (keyword (str *ns*) (name controller))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(g/fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *controllers* assoc ~qname {:fns ~fns})))

(defn substitute-scene [_]
  {:aabb (geom/null-aabb)
   :renderable {:render-fn (fn [gl render-args renderables count]
                             (let [pass (:pass render-args)]
                               (when (= pass pass/overlay)
                                 (overlay-text gl "An error prevents rendering from happening." 12.0 -22.0))))
                :passes [pass/overlay]}})

; Avoid recreating the image each frame
(def ^:private cached-buf-img-ref (atom nil))

; Replacement for Screenshot/readToBufferedImage but without expensive y-axis flip.
; We flip in JavaFX instead
(defn- read-to-buffered-image [^long w ^long h]
  (let [^BufferedImage image (let [^BufferedImage image @cached-buf-img-ref]
                               (when (or (nil? image) (not= (.getWidth image) w) (not= (.getHeight image) h))
                                 (reset! cached-buf-img-ref (BufferedImage. w h BufferedImage/TYPE_INT_ARGB_PRE)))
                               @cached-buf-img-ref)
        glc (GLContext/getCurrent)
        gl (.getGL glc)
        psm (GLPixelStorageModes.)]
   (.setPackAlignment psm gl 1)
   (.glReadPixels gl 0 0 w h GL2/GL_BGRA GL/GL_UNSIGNED_BYTE (IntBuffer/wrap (.getDataStorage ^IntegerComponentRaster (.getRaster image))))
   (.restore psm gl)
   image))

(def outline-color colors/bright-grey)
(def selected-outline-color colors/defold-turquoise)

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

(defn z-distance [camera viewport renderable ^Point3d tmp-p3d]
  (let [^Matrix4d t (or (:world-transform renderable) geom/Identity4d)
        _ (.transform t tmp-p3d)
        p (c/camera-project camera viewport tmp-p3d)]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera viewport renderable tmp-p3d]
  (:index renderable (- Long/MAX_VALUE (z-distance camera viewport renderable tmp-p3d))))

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

(g/defnk produce-drawable [_node-id ^Region viewport]
  (when (vp-not-empty? viewport)
    (let [[w h] (vp-dims viewport)
          ^GLOffscreenAutoDrawable drawable (g/node-value _node-id :gl-drawable)
          drawable (if drawable
                     (doto drawable (.setSize w h))
                     (let [profile (GLProfile/getDefault)
                           factory (GLDrawableFactory/getFactory profile)
                           caps    (doto (GLCapabilities. profile)
                                     (.setOnscreen false)
                                     (.setPBuffer true)
                                     (.setDoubleBuffered false))
                           ^GLOffscreenAutoDrawable drawable (.createOffscreenAutoDrawable factory nil caps nil w h nil)]
                       (g/transact (g/set-property _node-id :gl-drawable drawable))
                       drawable))]
      drawable)))

(defn- make-current [^Region viewport ^GLAutoDrawable drawable]
  (when (and drawable (vp-not-empty? viewport))
    (when-let [^GLContext context (.getContext drawable)]
      (let [result (.makeCurrent context)]
        (if (= result GLContext/CONTEXT_NOT_CURRENT)
          (do
            (prn "Failed to set gl context as current.")
            nil)
          context)))))

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
          (let [gl-name (if gl-names? batch-index nil)
                batch (subvec renderables 0 count)]
            (render-nodes gl render-args batch count gl-name))
          (let [end (+ offset count)]
            ; TODO - long conversion should not be necessary?
            (recur (subvec renderables count) (long end) (inc batch-index) (conj! batches [offset end])))))
      (persistent! batches))))

(defn- render-sort [renderables camera viewport]
  (sort-by :render-key renderables))

(defn- generic-render-args [glu viewport camera]
  (let [view (c/camera-view-matrix camera)
        proj (c/camera-projection-matrix camera)
        view-proj (doto (Matrix4d. proj) (.mul view))
        world (doto (Matrix4d.) (.setIdentity))
        world-view (doto (Matrix4d. view) (.mul world))
        texture (doto (Matrix4d.) (.setIdentity))
        normal (doto (math/affine-inverse world-view) (.transpose))]
    {:glu glu :camera camera :viewport viewport :view view :projection proj :view-proj view-proj :world world
     :world-view world-view :texture texture :normal normal}))

(g/defnk produce-frame [^Region viewport ^GLAutoDrawable drawable camera renderables tool-renderables]
  (when-let [^GLContext context (make-current viewport drawable)]
    (let [gl ^GL2 (.getGL context)
          glu ^GLU (GLU.)
          render-args (generic-render-args glu viewport camera)
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
        (scene-cache/prune-object-caches! gl)
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
            render-args (generic-render-args glu viewport camera)
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
            render-args (generic-render-args glu viewport camera)
            tool-renderables (apply merge-with into tool-renderables)
            passes [pass/manipulator-selection pass/overlay-selection]]
        (flatten
          (for [pass passes
                :let [render-args (assoc render-args :pass pass)]]
            (do
              (begin-select gl select-buffer)
              (setup-pass context gl glu pass camera viewport tool-picking-rect)
              (let [renderables (get tool-renderables pass)
                    batches (batch-render gl render-args renderables true :select-batch-key)]
                (render-sort (end-select gl select-buffer renderables batches) camera viewport))))))
      (finally
        (.release context)))
    []))

(g/defnk produce-selected-tool-renderables [tool-selection]
  (apply merge-with concat {} (map #(do {(:node-id %) [(:selection-data %)]}) tool-selection)))

(defn flatten-scene [scene selection-set ^Matrix4d world-transform out-renderables out-selected-renderables camera viewport tmp-p3d]
 (let [renderable (:renderable scene)
       ^Matrix4d trans (or (:transform scene) geom/Identity4d)
       parent-world world-transform
       world-transform (doto (Matrix4d. world-transform) (.mul trans))
       selected (contains? selection-set (:node-id scene))
       new-renderable (-> scene
                        (dissoc :renderable)
                        (assoc :render-fn (:render-fn renderable)
                               :world-transform world-transform
                               :selected selected
                               :user-data (:user-data renderable)
                               :batch-key (:batch-key renderable)
                               :aabb (geom/aabb-transform ^AABB (:aabb scene) parent-world))
                        (assoc :render-key (render-key camera viewport renderable tmp-p3d)))]
   (doseq [pass (:passes renderable)]
     (conj! (get out-renderables pass) new-renderable)
     (when (and selected (types/selection? pass))
       (conj! out-selected-renderables new-renderable)))
   (doseq [child-scene (:children scene)]
     (flatten-scene child-scene selection-set world-transform out-renderables out-selected-renderables camera viewport tmp-p3d))))

(defn produce-render-data [scene selection aux-renderables camera viewport]
  (let [selection-set (set selection)
        out-renderables (into {} (map #(do [% (transient [])]) pass/all-passes))
        out-selected-renderables (transient [])
        world-transform (doto (Matrix4d.) (.setIdentity))
        tmp-p3d (Point3d.)
        render-data (flatten-scene scene selection-set world-transform out-renderables out-selected-renderables camera viewport tmp-p3d)
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
  (output select-buffer IntBuffer :cached (g/always (-> (ByteBuffer/allocateDirect (* 4 pick-buffer-size))
                                                        (.order (ByteOrder/nativeOrder))
                                                        (.asIntBuffer))))
  (output drawable  GLAutoDrawable :cached produce-drawable)
  (output frame BufferedImage :cached produce-frame)
  (output picking-selection g/Any :cached produce-selection)
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-renderables g/Any :cached (g/fnk [render-data] (:selected-renderables render-data)))
  (output selected-aabb AABB :cached (g/fnk [selected-renderables] (reduce geom/aabb-union (geom/null-aabb) (map :aabb selected-renderables))))
  (output selected-updatables g/Any :cached (g/fnk [selected-renderables]
                                                   (into {} [(first (map (fn [r] [(:node-id r) r]) (filter :updatable selected-renderables)))])))
  (output updatables g/Any :cached (g/fnk [renderables]
                                          (let [flat-renderables (apply concat (map second renderables))]
                                            (into {} (map (fn [r] [(:node-id r) r]) (filter :updatable flat-renderables))))))
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables))

(defn dispatch-input [input-handlers action user-data]
  (reduce (fn [action [node-id label]]
            (when action
              ((g/node-value node-id label) node-id action user-data)))
          action input-handlers))

(g/defnode SceneView
  (inherits core/Scope)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property picking-rect Rect)
  (property active-updatable-ids g/Any)
  (property play-mode g/Keyword)

  (input frame BufferedImage)
  (input scene g/Any :substitute substitute-scene)
  (input input-handlers Runnable :array)
  (input selection g/Any)
  (input selected-tool-renderables g/Any)
  (input active-tool g/Keyword)
  (input updatables g/Any)
  (input selected-aabb AABB)
  (input selected-updatables g/Any)
  (output active-tool g/Keyword (g/fnk [active-tool] active-tool))
  (output active-updatables g/Any (g/fnk [updatables active-updatable-ids]
                                         (mapv updatables (filter #(contains? updatables %) active-updatable-ids))))

  (output scene g/Any (g/fnk [scene] scene))
  (output image WritableImage :cached (g/fnk [^BufferedImage frame ^ImageView image-view]
                                             (when frame (SwingFXUtils/toFXImage frame (.getImage image-view)))))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene (geom/null-aabb))))
  (output selection g/Any (g/fnk [selection] selection))
  (output selected-aabb AABB :cached (g/fnk [selected-aabb scene] (if (= selected-aabb (geom/null-aabb)) (:aabb scene) selected-aabb)))
  (output picking-rect Rect (g/fnk [picking-rect] picking-rect)))

(defn scene-view-dispose [node-id]
  (when-let [scene (g/node-by-id node-id)]
    (when-let [^GLAutoDrawable drawable (g/node-value node-id :gl-drawable)]
      (scene-cache/drop-context! (.getGL drawable) false)
      (.destroy drawable)
      (g/set-property! node-id :gl-drawable nil))))

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

(defn- ^Vector3d screen->world [camera viewport ^Vector3d screen-pos] ^Vector3d
  (let [w4 (c/camera-unproject camera viewport (.x screen-pos) (.y screen-pos) (.z screen-pos))]
    (Vector3d. (.x w4) (.y w4) (.z w4))))

(defn augment-action [view action]
  (let [x          (:x action)
        y          (:y action)
        screen-pos (Vector3d. x y 0)
        view-graph (g/node-id->graph-id view)
        camera     (g/node-value (g/graph-value view-graph :camera) :camera)
        viewport   (g/node-value view :viewport)
        world-pos  (Point3d. (screen->world camera viewport screen-pos))
        world-dir  (doto (screen->world camera viewport (doto (Vector3d. screen-pos) (.setZ 1)))
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

(defn- active-scene-view [app-view]
  (let [view (g/node-value app-view :active-view)]
    (when (and view (= (g/node-type* view) SceneView))
      view)))

(defn- play-handler [view-id]
  (let [play-mode (g/node-value view-id :play-mode)
        updatables (g/node-value view-id :selected-updatables)
        new-play-mode (if (= play-mode :playing) :idle :playing)
        active-updatable-ids (g/node-value view-id :active-updatable-ids)
        new-active-updatable-ids (if (empty? active-updatable-ids) (keys updatables) active-updatable-ids)]
    (g/transact
      (concat
        (g/set-property view-id :play-mode new-play-mode)
        (g/set-property view-id :active-updatable-ids new-active-updatable-ids)))))

(handler/defhandler :scene-play :global
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (let [selected (g/node-value view :selected-updatables)]
                           (not (empty? selected)))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (play-handler view))))

(defn- stop-handler [view-id]
  (g/transact
    (concat
      (g/set-property view-id :play-mode :idle)
      (g/set-property view-id :active-updatable-ids []))))

(handler/defhandler :scene-stop :global
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (let [active (g/node-value view :active-updatables)]
                           (not (empty? (g/node-value view :active-updatables))))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (stop-handler view))))

(defn- frame-selection [app-view]
  (when-let [view (active-scene-view app-view)]
    (let [graph (g/node-id->graph-id view)
          camera (g/graph-value graph :camera)
          aabb (g/node-value view :selected-aabb)
          viewport (g/node-value view :viewport)
          local-cam (g/node-value camera :local-camera)
          end-camera (c/camera-orthographic-frame-aabb local-cam viewport aabb)
          duration 0.5]
      (ui/anim! duration
                (fn [t] (let [t (- (* t t 3) (* t t t 2))
                              cam (c/interpolate local-cam end-camera t)]
                          (g/transact
                            (g/set-property camera :local-camera cam))))
                (fn [])))))

(handler/defhandler :frame-selection :global
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (let [selected (g/node-value view :selection)]
                           (not (empty? selected)))))
  (run [app-view] (frame-selection app-view)))

(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Scene"
                  :id ::scene
                  :children [{:label "Frame"
                              :acc "Shortcut+."
                              :command :frame-selection}
                             {:label "Play"
                              :acc "Shortcut+P"
                              :command :scene-play}
                             {:label "Stop"
                              :acc "Shortcut+T"
                              :command :scene-stop}]}])

(defn- make-scene-view [scene-graph ^Parent parent opts]
  (let [image-view (ImageView.)]
    (.add (.getChildren ^Pane parent) image-view)
    (let [view-id (g/make-node! scene-graph SceneView :image-view image-view)
          tool-user-data (atom [])
          event-handler   (ui/event-handler
                            e
                            (let [action (augment-action view-id (i/action-from-jfx e))
                                  x (:x action)
                                  y (:y action)
                                  pos [x y 0.0]
                                  picking-rect (calc-picking-rect pos pos)]
                              ; Only look for tool selection when the mouse is moving with no button pressed
                              (when (and (= :mouse-moved (:type action)) (= 0 (:click-count action)))
                                (reset! tool-user-data (g/node-value view-id :selected-tool-renderables)))
                              (g/transact (g/set-property view-id :picking-rect picking-rect))
                              (dispatch-input (g/sources-of view-id :input-handlers) action @tool-user-data)))
          change-listener (ui/change-listener
                            observable old-val new-val
                            (Platform/runLater
                              (fn []
                                (let [bb ^BoundingBox (.getBoundsInParent (.getParent parent))
                                      w (- (.getMaxX bb) (.getMinX bb))
                                      h (- (.getMaxY bb) (.getMinY bb))]
                                  (flip-y (g/node-value view-id :image-view) h)
                                  (g/transact (g/set-property view-id :viewport (types/->Region 0 w 0 h)))))))]
      (.setOnMousePressed parent event-handler)
      (.setOnMouseReleased parent event-handler)
      (.setOnMouseClicked parent event-handler)
      (.setOnMouseMoved parent event-handler)
      (.setOnMouseDragged parent event-handler)
      (.setOnScroll parent event-handler)
      (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)

      (let [fps-counter   (when *fps-debug* (agent (long-array 3 0)))
            ^Tab tab      (:tab opts)
            repainter     (ui/->timer
                            (fn [dt]
                              (when (.isSelected tab)
                                (when *fps-debug* (send-off fps-counter tick dt))
                                ; Fixed dt for deterministic playback
                                (let [dt 1/60]
                                  (let [updatables (g/node-value view-id :active-updatables)
                                        context {:dt (if (= (g/node-value view-id :play-mode) :playing) dt 0)}]
                                    (doseq [updatable updatables
                                            :let [node-path (:node-path updatable)
                                                  context (assoc context
                                                                 :world-transform (:world-transform updatable))]]
                                      ((get-in updatable [:updatable :update-fn]) context))
                                    (when (not (empty? updatables))
                                      (g/invalidate! (g/sources-of view-id :frame)))))
                                (scene-cache/prune-object-caches! nil)
                                (try
                                  (let [image-view ^ImageView (g/node-value view-id :image-view)
                                        image (g/node-value view-id :image)]
                                    (when (not= image (.getImage image-view))
                                      (.setImage image-view image)))
                                  (catch Exception e
                                    (.setImage image-view nil)
                                    (throw e))))))]
        (ui/on-close tab
                     (fn [e]
                       (ui/timer-stop! repainter)
                       (scene-view-dispose view-id)
                       (scene-cache/drop-context! nil true)))
        (ui/timer-start! repainter))
      view-id)))

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
  (input updatables g/Any)
  (input selected-aabb AABB)
  (input selected-updatables g/Any)
  (output active-tool g/Keyword (g/fnk [active-tool] active-tool))

  (output scene g/Any (g/fnk [scene] scene))
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output viewport Region (g/fnk [width height] (types/->Region 0 width 0 height)))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene (geom/null-aabb))))
  (output selection g/Any :cached (g/fnk [selection] selection))
  (output selected-aabb AABB :cached (g/fnk [selected-aabb scene] (if (= selected-aabb (geom/null-aabb)) (:aabb scene) selected-aabb)))
  (output picking-rect Rect :cached (g/fnk [picking-rect] picking-rect)))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView :width width :height height))

(defn- make-controllers [args]
  (for [[_ controller] @*controllers*
        :let [make-fn (get-in controller [:fns :make])]
        :when make-fn]
    (make-fn args)))

(defn setup-view [view-id resource-node opts]
  (let [view-graph  (g/node-id->graph-id view-id)
        app-view-id (:app-view opts)
        project     (:project opts)]
    (g/make-nodes view-graph
                  [renderer   SceneRenderer
                   background background/Gradient
                   camera     [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic)) :reframe true]
                   grid       grid/Grid
                   tool-controller scene-tools/ToolController]
                  (g/update-property camera  :movements-enabled disj :tumble) ; TODO - pass in to constructor

                  (g/connect resource-node        :scene                     view-id          :scene)
                  (g/set-graph-value view-graph   :renderer                  renderer)
                  (g/set-graph-value view-graph   :camera                    camera)

                  (g/connect background           :renderable                renderer         :aux-renderables)
                  (g/connect camera               :camera                    renderer         :camera)
                  (g/connect camera               :input-handler             view-id          :input-handlers)
                  (g/connect view-id              :aabb                      camera           :aabb)
                  (g/connect view-id              :viewport                  camera           :viewport)
                  (g/connect view-id              :viewport                  renderer         :viewport)
                  (g/connect view-id              :scene                     renderer         :scene)

                  (g/connect project              :selected-node-ids         view-id          :selection)
                  (g/connect view-id              :selection                 renderer         :selection)
                  (g/connect renderer             :frame                     view-id          :frame)

                  (g/connect tool-controller      :input-handler             view-id          :input-handlers)

                  (g/connect view-id              :picking-rect              renderer         :tool-picking-rect)
                  (g/connect renderer             :selected-tool-renderables view-id          :selected-tool-renderables)
                  (g/connect renderer             :updatables                view-id          :updatables)
                  (g/connect renderer             :selected-updatables       view-id          :selected-updatables)
                  (g/connect renderer             :selected-aabb             view-id          :selected-aabb)

                  (g/connect grid                 :renderable                renderer         :aux-renderables)
                  (g/connect camera               :camera                    grid             :camera)


                  (g/connect tool-controller      :renderables               renderer         :tool-renderables)

                  (g/connect app-view-id          :active-tool               view-id          :active-tool)
                  (g/connect view-id              :active-tool               tool-controller  :active-tool)
                  (g/connect view-id              :viewport                  tool-controller  :viewport)
                  (g/connect camera               :camera                    tool-controller  :camera)
                  (g/connect renderer             :selected-renderables      tool-controller  :selected-renderables)
                  (when (not (:grid opts))
                    (g/delete-node grid))

                  (make-controllers {:project project :resource-node resource-node
                                     :view view-id :renderer renderer}))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view-id (make-scene-view graph parent opts)]
    (g/transact
      (setup-view view-id resource-node opts))
    view-id))

(defn make-preview [graph resource-node opts width height]
  (let [view-id (make-preview-view graph width height)]
    (g/transact
      (setup-view view-id resource-node (dissoc opts :grid)))
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :scene
                                :make-view-fn make-view
                                :make-preview-fn make-preview))

(g/defnode SceneNode
  (property position types/Vec3 (default [0.0 0.0 0.0]))
  (property rotation types/Vec3 (default [0.0 0.0 0.0]))

  (output position Vector3d :cached (g/fnk [^types/Vec3 position] (Vector3d. (double-array position))))
  (output rotation Quat4d :cached (g/fnk [^types/Vec3 rotation] (math/euler->quat rotation)))
  (output transform Matrix4d :cached (g/fnk [^Vector3d position ^Quat4d rotation] (Matrix4d. rotation position 1.0)))
  (output scene g/Any :cached (g/fnk [^g/NodeID _node-id ^Matrix4d transform] {:node-id _node-id :transform transform}))
  (output aabb AABB :cached (g/always (geom/null-aabb)))

  scene-tools/Movable
  (scene-tools/move [self delta] (let [p (doto (Vector3d. (double-array (:position self))) (.add delta))]
                                   (g/set-property (g/node-id self) :position [(.x p) (.y p) (.z p)])))
  scene-tools/Rotatable
  (scene-tools/rotate [self delta] (let [new-rotation (doto (Quat4d. ^Quat4d (math/euler->quat (:rotation self))) (.mul delta))
                                         new-euler (math/quat->euler new-rotation)]
                                     (g/set-property (g/node-id self) :rotation new-euler))))

(g/defnk produce-transform [^Vector3d position ^Quat4d rotation ^Vector3d scale]
  (let [transform (Matrix4d. rotation position 1.0)
        s [(.x scale) (.y scale) (.z scale)]
        col (Vector4d.)]
    (doseq [^Integer i (range 3)
            :let [s (nth s i)]]
      (.getColumn transform i col)
      (.scale col s)
      (.setColumn transform i col))
    transform))

(g/defnode ScalableSceneNode
  (inherits SceneNode)

  (property scale types/Vec3 (default [1 1 1]))

  (display-order [SceneNode :scale])

  (output scale Vector3d :cached (g/fnk [^types/Vec3 scale] (Vector3d. (double-array scale))))
  (output transform Matrix4d :cached produce-transform)

  scene-tools/Scalable
  (scene-tools/scale [self delta] (let [s (Vector3d. (double-array (:scale self)))
                                        ^Vector3d d delta]
                                    (.setX s (* (.x s) (.x d)))
                                    (.setY s (* (.y s) (.y d)))
                                    (.setZ s (* (.z s) (.z d)))
                                    (g/set-property (g/node-id self) :scale [(.x s) (.y s) (.z s)]))))

(defn- make-text-renderer [^GL2 gl data]
  (let [[font-family font-style font-size] data]
    (gl/text-renderer font-family font-style font-size)))

(defn- destroy-text-renderers [^GL2 gl text-renderers _]
  (doseq [^TextRenderer text-renderer text-renderers]
    (.dispose text-renderer)))

(defn- update-text-renderer [^GL2 gl text-renderer data]
  (destroy-text-renderers gl [text-renderer])
  (make-text-renderer gl data))

(scene-cache/register-object-cache! ::text-renderer make-text-renderer update-text-renderer destroy-text-renderers)

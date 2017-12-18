(ns editor.scene
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.scene-selection :as selection]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.input :as i]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [util.profiler :as profiler]
            [editor.resource :as resource]
            [editor.scene-cache :as scene-cache]
            [editor.scene-text :as scene-text]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [editor.ui :as ui]
            [editor.rulers :as rulers]
            [service.log :as log]
            [editor.graph-util :as gu]
            [editor.properties :as properties]
            [editor.view :as view])
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
           [javafx.geometry BoundingBox Pos VPos HPos]
           [javafx.scene Scene Group Node Parent]
           [javafx.scene.control Tab Button]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout AnchorPane Pane StackPane]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [com.jogamp.opengl GL GL2 GL2GL3 GLContext GLException GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]
           [sun.awt.image IntegerComponentRaster]
           [com.defold.editor AsyncCopier]))

(set! *warn-on-reflection* true)

(defn overlay-text [^GL2 gl ^String text x y]
  (scene-text/overlay gl text x y))

(defn- get-resource-name [node-id]
  (let [{:keys [resource] :as resource-node} (and node-id (g/node-by-id node-id))]
    (and resource (resource/resource-name resource))))

(defn- root-causes
  [error]
  (->> (tree-seq :causes :causes error)
       (filter :message)
       (remove :causes)))

(defn- render-error
  [gl render-args renderables nrenderables]
  (when (= pass/overlay (:pass render-args))
    (let [errors (->> renderables
                      (map (comp :error :user-data))
                      (mapcat root-causes)
                      (map #(select-keys % [:_node-id :message]))
                      set)]
      (scene-text/overlay gl "Render error:" 24.0 -22.0)
      (doseq [[n error] (partition 2 (interleave (range) errors))]
        (let [message (format "- %s: %s"
                              (or (get-resource-name (:_node-id error))
                                  "unknown")
                              (:message error))]
          (scene-text/overlay gl message 24.0 (- -22.0 (* 14 (inc n)))))))))

(defn substitute-render-data
  [error]
  [{pass/overlay [{:render-fn render-error
                   :user-data {:error error}
                   :batch-key ::error}]}])

(defn substitute-scene [error]
  {:aabb       (geom/null-aabb)
   :renderable {:render-fn render-error
                :user-data {:error error}
                :batch-key ::error
                :passes    [pass/overlay]}})

;; Avoid recreating the image each frame
(defonce ^:private cached-buf-img-ref (atom nil))

;; Replacement for Screenshot/readToBufferedImage but without expensive y-axis flip.
;; We flip in JavaFX instead
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
  (types/dimensions viewport))

(defn vp-not-empty? [^Region viewport]
  (not (types/empty-space? viewport)))

(defn z-distance [^Matrix4d view-proj ^Matrix4d world-transform]
  (let [^Matrix4d t (or world-transform geom/Identity4d)
        tmp-v4d (Vector4d.)]
    (.getColumn t 3 tmp-v4d)
    (.transform view-proj tmp-v4d)
    (let [ndc-z (/ (.z tmp-v4d) (.w tmp-v4d))
          wz (min 1.0 (max 0.0 (* (+ ndc-z 1.0) 0.5)))]
      (long (* Integer/MAX_VALUE (max 0.0 wz))))))

(defn- render-key [^Matrix4d view-proj ^Matrix4d world-transform index topmost?]
  [(boolean topmost?)
   (- Long/MAX_VALUE (z-distance view-proj world-transform))
   (or index 0)])

(defn- outline-render-key [^Matrix4d view-proj ^Matrix4d world-transform index topmost? selected?]
  ;; Draw selection outlines on top of other outlines.
  [(boolean selected?)
   (boolean topmost?)
   (- Long/MAX_VALUE (z-distance view-proj world-transform))
   (or index 0)])

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn setup-pass
  ([context gl pass camera ^Region viewport]
    (setup-pass context gl pass camera viewport nil))
  ([context ^GL2 gl pass camera ^Region viewport pick-rect]
    (let [glu (GLU.)]
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
      (pass/prepare-gl pass gl glu))))

(defn make-current ^GLContext [^GLAutoDrawable drawable]
  (when-let [^GLContext context (.getContext drawable)]
    (try
      (let [result (.makeCurrent context)]
        (if (= result GLContext/CONTEXT_NOT_CURRENT)
          (do
            (log/warn :message "Failed to set gl context as current.")
            nil)
          context))
      (catch Exception e
        (log/error :exception e)
        nil))))

(defmacro with-drawable-as-current [^GLAutoDrawable drawable & forms]
  `(when-let [^GLContext ~'gl-context (make-current ~drawable)]
     (try
       (let [^GL2 ~'gl (.getGL ~'gl-context)]
         ~@forms)
       (finally (.release ~'gl-context)))))

(defn- gl-profile ^GLProfile []
  (try
    (GLProfile/getGL2ES1)
    (catch GLException e
      (log/warn :message "Failed to acquire GL profile for GL2/GLES1.")
      (GLProfile/getDefault))))

(defn make-drawable ^GLOffscreenAutoDrawable [w h]
  (let [profile (gl-profile)
        factory (GLDrawableFactory/getFactory profile)
        caps    (doto (GLCapabilities. profile)
                  (.setOnscreen false)
                  (.setFBO true)
                  (.setPBuffer true)
                  (.setDoubleBuffered false)
                  (.setStencilBits 8))
        drawable (.createOffscreenAutoDrawable factory nil caps nil w h)
        context (.createContext drawable nil)]
    (.setContext drawable context true)
    (with-drawable-as-current drawable
      (gl/gl-info! context))
    (let [info (gl/gl-info)]
      (when-let [missing (seq (:missing-functions info))]
        (.destroy drawable)
        (throw (GLException. (string/join "\n"
                               [(format "The graphics device does not support: %s" (string/join ", " missing))
                                (format "GPU: %s" (:renderer info))
                                (format "Driver: %s" (:version info))
                                "Please try to update your driver to the latest version and restart the application."])))))
    drawable))

(defn make-copier [^Region viewport]
  (let [[w h] (vp-dims viewport)]
    (AsyncCopier. w h)))

(defn render-nodes
  ([^GL2 gl render-args renderables count]
    (render-nodes gl render-args renderables count nil))
  ([^GL2 gl render-args renderables count gl-name]
    (when-let [render-fn (:render-fn (first renderables))]
      (try
        (when gl-name
          (.glPushName gl gl-name))
        (render-fn gl (assoc render-args :world (:world-transform (first renderables))) renderables count)
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
                          break? (or (not= first-render-fn render-fn)
                                     (nil? first-key)
                                     (nil? key)
                                     (not= first-key key))]
                      (if break?
                        count
                        (recur (rest renderables) (inc count)))))]
        (when (> count 0)
          (let [gl-name (if gl-names? batch-index nil)
                batch (subvec renderables 0 count)]
            (render-nodes gl render-args batch count gl-name))
          (let [end (+ offset count)]
            ;; TODO - long conversion should not be necessary?
            (recur (subvec renderables count) (long end) (inc batch-index) (conj! batches [offset end])))))
      (persistent! batches))))

(defn- render-sort [renderables]
  (sort-by :render-key renderables))

(defn generic-render-args [viewport camera view-id]
  (let [view (c/camera-view-matrix camera)
        proj (c/camera-projection-matrix camera)
        view-proj (doto (Matrix4d. proj) (.mul view))
        world (doto (Matrix4d.) (.setIdentity))
        world-view (doto (Matrix4d. view) (.mul world))
        texture (doto (Matrix4d.) (.setIdentity))
        normal (doto (math/affine-inverse world-view) (.transpose))]
    {:camera camera :viewport viewport :view view :projection proj :view-proj view-proj :world world
     :world-view world-view :texture texture :normal normal
     :view-id view-id}))

(defn- assoc-updatable-states
  [renderables updatable-states]
  (mapv (fn [renderable]
          (if-let [updatable-node-id (get-in renderable [:updatable :node-id])]
            (assoc-in renderable [:updatable :state] (get-in updatable-states [updatable-node-id]))
            renderable))
        renderables))

(defn render! [render-args ^GLContext context updatable-states]
  (let [^GL2 gl (.getGL context)
        {:keys [viewport camera renderables]} render-args]
    (gl/gl-clear gl 0.0 0.0 0.0 1)
    (.glColor4f gl 1.0 1.0 1.0 1.0)
    (gl-viewport gl viewport)
    (doseq [pass pass/render-passes
            :let [render-args (assoc render-args :pass pass)
                  pass-renderables (-> (get renderables pass)
                                       (assoc-updatable-states updatable-states))]]
      (setup-pass context gl pass camera viewport)
      (batch-render gl render-args pass-renderables false :batch-key))))

(defn- apply-pass-overrides
  [pass renderable]
  (let [overrides (get-in renderable [:pass-overrides pass])]
    (cond-> (dissoc renderable :pass-overrides)
      overrides
      (merge overrides))))

(defn- make-pass-renderables
  []
  (into {} (map #(vector % (transient []))) pass/all-passes))

(defn- persist-pass-renderables!
  [pass-renderables]
  (into {}
        (map (fn [[pass renderables]]
               [pass (persistent! renderables)]))
        pass-renderables))

(defn- update-pass-renderables!
  [pass-renderables scene flattened-renderable]
  (reduce (fn [pass-renderables pass]
            (update pass-renderables pass conj! (apply-pass-overrides pass flattened-renderable)))
          pass-renderables
          (-> scene :renderable :passes)))

(defn- flatten-scene-renderables! [pass-renderables scene selection-set view-proj node-path parent-world-transform]
  (let [renderable (:renderable scene)
        local-transform ^Matrix4d (:transform scene geom/Identity4d)
        world-transform (doto (Matrix4d. ^Matrix4d parent-world-transform) (.mul local-transform))
        appear-selected? (some? (some selection-set node-path)) ; Child nodes appear selected if parent is.
        new-renderable (-> scene
                           (dissoc :children :renderable)
                           (assoc :node-path node-path
                                  :picking-id (or (:picking-id scene) (peek node-path))
                                  :render-fn (:render-fn renderable)
                                  :world-transform world-transform
                                  :parent-world-transform parent-world-transform
                                  :selected appear-selected?
                                  :user-data (:user-data renderable)
                                  :batch-key (:batch-key renderable)
                                  :aabb (geom/aabb-transform ^AABB (:aabb scene (geom/null-aabb)) parent-world-transform)
                                  :render-key (render-key view-proj world-transform (:index renderable) (:topmost? renderable))
                                  :pass-overrides {pass/outline {:render-key (outline-render-key view-proj world-transform (:index renderable) (:topmost? renderable) appear-selected?)}}))
        pass-renderables (update-pass-renderables! pass-renderables scene new-renderable)]
    (reduce (fn [pass-renderables child-scene]
              (flatten-scene-renderables! pass-renderables child-scene selection-set view-proj (conj node-path (:node-id child-scene)) world-transform))
            pass-renderables
            (:children scene))))

(defn- flatten-scene [scene selection-set view-proj]
  (let [node-path []
        parent-world-transform (doto (Matrix4d.) (.setIdentity))]
    (-> (make-pass-renderables)
        (flatten-scene-renderables! scene selection-set view-proj node-path parent-world-transform)
        (persist-pass-renderables!))))

(defn- get-selection-pass-renderables-by-node-id
  "Returns a map of renderables that were in a selection pass by their node id.
  If a renderable appears in multiple selection passes, the one from the latter
  pass will be picked. This should be fine, since the new-renderable added
  by append-flattened-scene-renderables! will be the same for all passes."
  [renderables-by-pass]
  (into {}
        (comp (filter (comp types/selection? key))
              (mapcat (fn [[_pass renderables]]
                        (map (fn [renderable]
                               [(:node-id renderable) renderable])
                             renderables))))
        renderables-by-pass))

(defn produce-render-data [scene selection aux-renderables camera]
  (let [selection-set (set selection)
        view-proj (c/camera-view-proj-matrix camera)
        scene-renderables-by-pass (flatten-scene scene selection-set view-proj)
        selection-pass-renderables-by-node-id (get-selection-pass-renderables-by-node-id scene-renderables-by-pass)
        selected-renderables (into [] (keep selection-pass-renderables-by-node-id) selection)
        aux-renderables-by-pass (apply merge-with concat aux-renderables)
        all-renderables-by-pass (merge-with into scene-renderables-by-pass aux-renderables-by-pass)
        sorted-renderables-by-pass (into {} (map (fn [[pass renderables]] [pass (vec (render-sort renderables))]) all-renderables-by-pass))]
    {:renderables sorted-renderables-by-pass
     :selected-renderables selected-renderables}))

(g/defnk produce-render-args [_node-id ^Region viewport camera all-renderables frame-version]
  (let [current-frame-version (if frame-version (swap! frame-version inc) 0)]
    (-> (generic-render-args viewport camera _node-id)
        (assoc
          :renderables all-renderables
          :frame-version current-frame-version))))

(g/defnode SceneRenderer
  (property frame-version g/Any)

  (input scene g/Any :substitute substitute-scene)
  (input selection g/Any)
  (input camera Camera)
  (input aux-renderables pass/RenderData :array :substitute gu/array-subst-remove-errors)

  (output viewport Region :abstract)
  (output all-renderables g/Any :abstract)
  (output render-data g/Any :cached (g/fnk [scene selection aux-renderables camera] (produce-render-data scene selection aux-renderables camera)))
  (output renderables pass/RenderData :cached (g/fnk [render-data] (:renderables render-data)))
  (output selected-renderables g/Any :cached (g/fnk [render-data] (:selected-renderables render-data)))
  (output selected-aabb AABB :cached (g/fnk [selected-renderables scene] (if (empty? selected-renderables)
                                                                           (:aabb scene)
                                                                           (reduce geom/aabb-union (geom/null-aabb) (map :aabb selected-renderables)))))
  (output selected-updatables g/Any :cached (g/fnk [selected-renderables]
                                              (into {}
                                                    (comp (keep :updatable)
                                                          (map (juxt :node-id identity)))
                                                    selected-renderables)))
  (output updatables g/Any :cached (g/fnk [renderables]
                                     ;; Currently updatables are implemented as extra info on the renderables.
                                     ;; The renderable associates an updatable with itself, which contains info
                                     ;; about the updatable. The updatable is identified by a node-id, for example
                                     ;; the id of a ParticleFXNode. In the case of ParticleFX, the same updatable
                                     ;; is also associated with every sub-element of the ParticleFX scene, such
                                     ;; as every emitter and modifier below it. This makes it possible to start
                                     ;; playback of the owning ParticleFX scene while a modifier is selected.
                                     ;; In order to find the owning ParticleFX scene so we can position the
                                     ;; effect in the world, we find the renderable with the shortest node-path
                                     ;; for that particular updatable.
                                     ;;
                                     ;; TODO:
                                     ;; We probably want to change how this works to make it possible to have
                                     ;; multiple instances of the same updatable in a scene.
                                     (let [flat-renderables (apply concat (map second renderables))
                                           renderables-by-updatable-node-id (dissoc (group-by (comp :node-id :updatable) flat-renderables) nil)]
                                       (into {}
                                             (map (fn [[updatable-node-id renderables]]
                                                    (let [renderable (first (sort-by (comp count :node-path) renderables))
                                                          updatable (:updatable renderable)
                                                          world-transform (:world-transform renderable)
                                                          transformed-updatable (assoc updatable :world-transform world-transform)]
                                                      [updatable-node-id transformed-updatable])))
                                             renderables-by-updatable-node-id))))
  (output render-args g/Any :cached produce-render-args))

;; Scene selection

(def pick-buffer-size 4096)

(defn- make-select-buffer []
  (-> (ByteBuffer/allocateDirect (* 4 pick-buffer-size))
    (.order (ByteOrder/nativeOrder))
    (.asIntBuffer)))

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

(defn map-scene [f scene]
  (letfn [(scene-fn [scene]
            (let [children (:children scene)]
              (cond-> scene
                true (f)
                children (update :children (partial mapv scene-fn)))))]
    (scene-fn scene)))

(defn claim-child-scene [scene node-id]
  (assoc scene :picking-id node-id))

(defn claim-scene [scene node-id]
  ;; When scenes reference other resources in the project, we want to treat the
  ;; referenced scene as a group when picking in the scene view. To make this
  ;; happen, the referencing scene claims ownership of the referenced scene and
  ;; its children. Note that sub-elements can still be selected using the
  ;; Outline view should the need arise.
  (let [child-f #(claim-child-scene % node-id)
        children (:children scene)]
    (cond-> scene
      true (assoc :node-id node-id)
      children (update :children (partial mapv (partial map-scene child-f))))))

(g/defnk produce-selection [_node-id renderables ^GLAutoDrawable drawable viewport camera ^Rect picking-rect ^IntBuffer select-buffer selection]
  (or (and picking-rect
        (with-drawable-as-current drawable
          (let [render-args (generic-render-args viewport camera _node-id)]
            (into []
                  (comp (mapcat (fn [pass]
                                  (let [render-args (assoc render-args :pass pass)]
                                    (begin-select gl select-buffer)
                                    (setup-pass gl-context gl pass camera viewport picking-rect)
                                    (let [renderables (get renderables pass)
                                          batches (batch-render gl render-args renderables true :select-batch-key)]
                                      (reverse (render-sort (end-select gl select-buffer renderables batches)))))))
                        (keep :picking-id))
                  pass/selection-passes))))
      []))

(g/defnk produce-tool-selection [_node-id tool-renderables ^GLAutoDrawable drawable viewport camera ^Rect tool-picking-rect ^IntBuffer select-buffer]
  (or (and tool-picking-rect
        (with-drawable-as-current drawable
          (let [render-args (generic-render-args viewport camera _node-id)
                tool-renderables (apply merge-with into tool-renderables)
                passes [pass/manipulator-selection pass/overlay-selection]]
            (flatten
              (for [pass passes
                    :let [render-args (assoc render-args :pass pass)]]
                (do
                  (begin-select gl select-buffer)
                  (setup-pass gl-context gl pass camera viewport tool-picking-rect)
                  (let [renderables (get tool-renderables pass)
                        batches (batch-render gl render-args renderables true :select-batch-key)]
                    (render-sort (end-select gl select-buffer renderables batches)))))))))
    []))

(g/defnk produce-selected-tool-renderables [tool-selection]
  (apply merge-with concat {} (map #(do {(:node-id %) [(:selection-data %)]}) tool-selection)))

(g/defnode SceneView
  (inherits view/WorkbenchView)
  (inherits SceneRenderer)

  (property image-view ImageView)
  (property viewport Region (default (g/constantly (types/->Region 0 0 0 0))))
  (property active-updatable-ids g/Any)
  (property play-mode g/Keyword)
  (property drawable GLAutoDrawable)
  (property async-copier AsyncCopier)
  (property select-buffer IntBuffer)
  (property cursor-pos types/Vec2)
  (property tool-picking-rect Rect)
  (property tool-user-data g/Any (default (atom [])))
  (property input-action-queue g/Any (default []))
  (property updatable-states g/Any)

  (input input-handlers Runnable :array)
  (input picking-rect Rect)
  (input tool-renderables pass/RenderData :array :substitute substitute-render-data)
  (input active-tool g/Keyword)
  (input updatables g/Any)
  (input selected-updatables g/Any)
  (output active-tool g/Keyword (gu/passthrough active-tool))
  (output active-updatables g/Any :cached (g/fnk [updatables active-updatable-ids]
                                                 (into [] (keep updatables) active-updatable-ids)))

  (output selection g/Any (gu/passthrough selection))
  (output all-renderables pass/RenderData :cached (g/fnk [renderables tool-renderables]
                                                       (reduce (partial merge-with into) renderables tool-renderables)))
  (output picking-selection g/Any :cached produce-selection)
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables))

(defn scene-view-dispose [node-id]
  (when-let [scene (g/node-by-id node-id)]
    (when-let [^GLAutoDrawable drawable (g/node-value node-id :drawable)]
      (let [gl (.getGL drawable)]
        (when-let [^AsyncCopier copier (g/node-value node-id :async-copier)]
          (.dispose copier gl)
          (g/set-property! node-id :async-copier nil))
        (scene-cache/drop-context! node-id true)
        (scene-cache/drop-context! gl false)
        (.destroy drawable)
        (g/set-property! node-id :drawable nil)))))

(defn- ^Vector3d screen->world [camera viewport ^Vector3d screen-pos] ^Vector3d
  (let [w4 (c/camera-unproject camera viewport (.x screen-pos) (.y screen-pos) (.z screen-pos))]
    (Vector3d. (.x w4) (.y w4) (.z w4))))

(defn- view->camera [view]
  (g/node-feeding-into view :camera))

(defn augment-action [view action]
  (let [x          (:x action)
        y          (:y action)
        screen-pos (Vector3d. x y 0)
        view-graph (g/node-id->graph-id view)
        camera     (g/node-value (view->camera view) :camera)
        viewport   (g/node-value view :viewport)
        world-pos  (Point3d. (screen->world camera viewport screen-pos))
        world-dir  (doto (screen->world camera viewport (doto (Vector3d. screen-pos) (.setZ 1)))
                         (.sub world-pos)
                         (.normalize))]
    (assoc action
           :screen-pos screen-pos
           :world-pos world-pos
           :world-dir world-dir)))

(defn- active-scene-view [app-view]
  (let [view (g/node-value app-view :active-view)]
    (when (and view (g/node-instance? SceneView view))
      view)))

(defn- play-handler [view-id]
  (let [play-mode (g/node-value view-id :play-mode)
        selected-updatable-ids (set (keys (g/node-value view-id :selected-updatables)))
        active-updatable-ids (set (g/node-value view-id :active-updatable-ids))
        new-play-mode (if (= selected-updatable-ids active-updatable-ids)
                        (if (= play-mode :playing) :idle :playing)
                        :playing)]
    (g/transact
      (concat
        (g/set-property view-id :play-mode new-play-mode)
        (g/set-property view-id :active-updatable-ids selected-updatable-ids)))))

(handler/defhandler :scene-play :global
  (active? [app-view] (when-let [view (active-scene-view app-view)]
                        (seq (g/node-value view :updatables))))
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (let [selected (g/node-value view :selected-updatables)]
                           (not (empty? selected)))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (play-handler view))))

(defn- stop-handler [view-id]
  (g/transact
    (concat
      (g/set-property view-id :play-mode :idle)
      (g/set-property view-id :active-updatable-ids [])
      (g/set-property view-id :updatable-states {}))))

(handler/defhandler :scene-stop :global
  (active? [app-view] (when-let [view (active-scene-view app-view)]
                        (seq (g/node-value view :updatables))))
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (seq (g/node-value view :active-updatables))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (stop-handler view))))

(defn set-camera! [camera-node start-camera end-camera animate?]
  (if animate?
    (let [duration 0.5]
      (ui/anim! duration
                (fn [t] (let [t (- (* t t 3) (* t t t 2))
                              cam (c/interpolate start-camera end-camera t)]
                          (g/transact
                            (g/set-property camera-node :local-camera cam))))
                (fn []
                  (g/transact
                    (g/set-property camera-node :local-camera end-camera)))))
    (g/transact (g/set-property camera-node :local-camera end-camera))))

(defn frame-selection [view animate?]
  (when-let [aabb (g/node-value view :selected-aabb)]
    (let [graph (g/node-id->graph-id view)
          camera (view->camera view)
          viewport (g/node-value view :viewport)
          local-cam (g/node-value camera :local-camera)
          end-camera (c/camera-orthographic-frame-aabb local-cam viewport aabb)]
      (set-camera! camera local-cam end-camera animate?))))

(defn realign-camera [view animate?]
  (when-let [aabb (g/node-value view :selected-aabb)]
    (let [graph (g/node-id->graph-id view)
          camera (view->camera view)
          viewport (g/node-value view :viewport)
          local-cam (g/node-value camera :local-camera)
          end-camera (c/camera-orthographic-realign local-cam viewport aabb)]
      (set-camera! camera local-cam end-camera animate?))))

(handler/defhandler :frame-selection :global
  (enabled? [app-view] (when-let [view (active-scene-view app-view)]
                         (let [selected (g/node-value view :selection)]
                           (not (empty? selected)))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (frame-selection view true))))

(handler/defhandler :realign-camera :global
  (enabled? [app-view] (active-scene-view app-view))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (realign-camera view true))))

(handler/defhandler :move-whole-pixels :global
  (active? [app-view] (active-scene-view app-view))
  (state [prefs] (scene-tools/move-whole-pixels? prefs))
  (run [prefs] (scene-tools/set-move-whole-pixels! prefs (not (scene-tools/move-whole-pixels? prefs)))))

(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Scene"
                  :id ::scene
                  :children [{:label "Play"
                              :command :scene-play}
                             {:label "Stop"
                              :command :scene-stop}
                             {:label :separator}
                             {:label "Frame Selection"
                              :command :frame-selection}
                             {:label "Realign Camera"
                              :command :realign-camera}
                             {:label :separator}
                             {:label "Move Whole Pixels"
                              :command :move-whole-pixels
                              :check true}
                             {:label :separator
                              :id ::scene-end}]}])

(defn dispatch-input [input-handlers action user-data]
  (reduce (fn [action [node-id label]]
            (when action
              ((g/node-value node-id label) node-id action user-data)))
          action input-handlers))

(defn- update-updatables
  [updatable-states view-id play-mode active-updatables]
  (let [dt 1/60 ; fixed dt for deterministic playback
        context {:dt (if (= play-mode :playing) dt 0)}]
    (reduce (fn [ret {:keys [update-fn node-id world-transform initial-state]}]
              (let [context (assoc context
                                   :world-transform world-transform
                                   :node-id node-id
                                   :view-id view-id)
                    state (get-in updatable-states [node-id] initial-state)]
                (assoc ret node-id (update-fn state context))))
            {}
            active-updatables)))

(defn update-image-view! [^ImageView image-view ^GLAutoDrawable drawable ^AsyncCopier async-copier main-frame?]
  (when main-frame?
    (profiler/begin-frame))
  (when-let [view-id (ui/user-data image-view ::view-id)]
    (let [play-mode (g/node-value view-id :play-mode)
          tool-user-data (g/node-value view-id :tool-user-data)
          action-queue (g/node-value view-id :input-action-queue)
          active-updatables (g/node-value view-id :active-updatables)
          {:keys [frame-version] :as render-args} (g/node-value view-id :render-args)]
      (when (seq action-queue)
        (g/set-property! view-id :input-action-queue []))
      (when (seq active-updatables)
        (g/invalidate-outputs! [[view-id :render-args]]))
      (when main-frame?
        (scene-cache/prune-object-caches! nil))
      (profiler/profile "input-dispatch" -1
        (let [input-handlers (g/sources-of view-id :input-handlers)]
          (doseq [action action-queue]
            (dispatch-input input-handlers action @tool-user-data))))
      (profiler/profile "updatables" -1
        (when (seq active-updatables)
          (g/update-property! view-id :updatable-states update-updatables view-id play-mode active-updatables)))
      (profiler/profile "render" -1
        (let [current-frame-version (ui/user-data image-view ::current-frame-version)]
          (with-drawable-as-current drawable
            (when (not= current-frame-version frame-version)
              (render! render-args gl-context (g/node-value view-id :updatable-states))
              (ui/user-data! image-view ::current-frame-version frame-version)
              (scene-cache/prune-object-caches! gl)
              (scene-cache/prune-object-caches! view-id))
            (when-let [^WritableImage image (.flip async-copier gl frame-version)]
              (.setImage image-view image))))))))

(defn- nudge! [scene-node-ids ^double dx ^double dy ^double dz]
  (g/transact
    (for [node-id scene-node-ids
          :let [[^double x ^double y ^double z] (g/node-value node-id :position)]]
      (g/set-property node-id :position [(+ x dx) (+ y dy) (+ z dz)]))))

(declare selection->movable)

(handler/defhandler :up :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 0.0 1.0 0.0)))

(handler/defhandler :down :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 0.0 -1.0 0.0)))

(handler/defhandler :left :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) -1.0 0.0 0.0)))

(handler/defhandler :right :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 1.0 0.0 0.0)))

(handler/defhandler :up-major :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 0.0 10.0 0.0)))

(handler/defhandler :down-major :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 0.0 -10.0 0.0)))

(handler/defhandler :left-major :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) -10.0 0.0 0.0)))

(handler/defhandler :right-major :workbench
  (active? [selection] (selection->movable selection))
  (run [selection] (nudge! (selection->movable selection) 10.0 0.0 0.0)))

(defn- handle-key-pressed! [^KeyEvent event]
  ;; Only handle bare key events that cannot be bound to handlers here.
  (when (not= ::unhandled
              (if (or (.isAltDown event) (.isMetaDown event) (.isShiftDown event) (.isShortcutDown event))
                ::unhandled
                (condp = (.getCode event)
                  KeyCode/UP (ui/run-command (.getSource event) :up)
                  KeyCode/DOWN (ui/run-command (.getSource event) :down)
                  KeyCode/LEFT (ui/run-command (.getSource event) :left)
                  KeyCode/RIGHT (ui/run-command (.getSource event) :right)
                  ::unhandled)))
    (.consume event)))

(defn register-event-handler! [^Parent parent view-id]
  (let [process-events? (atom true)
        event-handler   (ui/event-handler e
                          (when @process-events?
                            (try
                              (profiler/profile "input-event" -1
                                (let [action (augment-action view-id (i/action-from-jfx e))
                                      x (:x action)
                                      y (:y action)
                                      pos [x y 0.0]
                                      picking-rect (selection/calc-picking-rect pos pos)]
                                  (when (= :mouse-pressed (:type action))
                                    ;; Request focus and consume event to prevent someone else from stealing focus
                                    (.requestFocus parent)
                                    (.consume e))
                                  ;; Only look for tool selection when the mouse is moving with no button pressed
                                  (when (and (= :mouse-moved (:type action)) (= 0 (:click-count action)))
                                    (let [s (g/node-value view-id :selected-tool-renderables)
                                          tool-user-data (g/node-value view-id :tool-user-data)]
                                      (reset! tool-user-data s)))
                                  (g/transact
                                    (concat
                                      (g/set-property view-id :cursor-pos [x y])
                                      (g/set-property view-id :tool-picking-rect picking-rect)
                                      (g/update-property view-id :input-action-queue conj action)))))
                              (catch Throwable error
                                (reset! process-events? false)
                                (error-reporting/report-exception! error)))))]
    (ui/on-mouse! parent (fn [type e] (cond
                                        (= type :exit)
                                        (g/set-property! view-id :cursor-pos nil))))
    (.setOnMousePressed parent event-handler)
    (.setOnMouseReleased parent event-handler)
    (.setOnMouseClicked parent event-handler)
    (.setOnMouseMoved parent event-handler)
    (.setOnMouseDragged parent event-handler)
    (.setOnScroll parent event-handler)
    (.setOnKeyPressed parent (ui/event-handler e
                               (when @process-events?
                                 (handle-key-pressed! e))))))

(defn make-gl-pane! [view-id parent opts timer-name main-frame?]
  (let [image-view (doto (ImageView.)
                     (.setScaleY -1.0)
                     (.setFocusTraversable true)
                     (.setPreserveRatio false)
                     (.setSmooth false))
        pane (proxy [com.defold.control.Region] []
               (layoutChildren []
                 (let [this ^com.defold.control.Region this
                       w (.getWidth this)
                       h (.getHeight this)]
                   (try
                     (.setFitWidth image-view w)
                     (.setFitHeight image-view h)
                     (proxy-super layoutInArea ^Node image-view 0.0 0.0 w h 0.0 HPos/CENTER VPos/CENTER)
                     (when (and (> w 0) (> h 0))
                       (let [viewport (types/->Region 0 w 0 h)]
                         (g/transact (g/set-property view-id :viewport viewport))
                         (if-let [view-id (ui/user-data image-view ::view-id)]
                           (let [drawable ^GLOffscreenAutoDrawable (g/node-value view-id :drawable)]
                             (doto drawable
                               (.setSurfaceSize w h))
                             (doto ^AsyncCopier (g/node-value view-id :async-copier)
                               (.setSize w h)))
                           (let [drawable (make-drawable w h)]
                             (ui/user-data! image-view ::view-id view-id)
                             (register-event-handler! this view-id)
                             (let [async-copier (make-copier viewport)
                                   ^Tab tab      (:tab opts)
                                   repainter     (ui/->timer timer-name
                                                             (fn [^AnimationTimer timer _]
                                                               (when (.isSelected tab)
                                                                 (try
                                                                   (update-image-view! image-view drawable async-copier main-frame?)
                                                                   (catch Throwable error
                                                                     (.stop timer)
                                                                     (error-reporting/report-exception! error))))))]
                               (ui/on-closed! tab
                                              (fn [e]
                                                (ui/timer-stop! repainter)
                                                (scene-view-dispose view-id)
                                                (scene-cache/drop-context! nil true)))
                               (ui/timer-start! repainter)
                               (g/set-property! view-id :drawable drawable :async-copier async-copier))
                             (frame-selection view-id false)))))
                     (catch Throwable error
                       (error-reporting/report-exception! error)))
                   (proxy-super layoutChildren))))]
    (.setFocusTraversable pane true)
    (.add (.getChildren pane) image-view)
    (g/set-property! view-id :image-view image-view)
    pane))

(defn- make-scene-view-pane [view-id opts]
  (let [scene-view-pane ^Pane (ui/load-fxml "scene-view.fxml")]
    (ui/fill-control scene-view-pane)
    (ui/with-controls scene-view-pane [^AnchorPane gl-view-anchor-pane]
      (let [gl-pane (make-gl-pane! view-id gl-view-anchor-pane opts "update-scene-view" true)]
        (ui/fill-control gl-pane)
        (.add (.getChildren scene-view-pane) 0 gl-pane)))
    scene-view-pane))

(defn- make-scene-view [scene-graph ^Parent parent opts]
  (let [view-id (g/make-node! scene-graph SceneView :select-buffer (make-select-buffer) :frame-version (atom 0) :updatable-states {})
        scene-view-pane (make-scene-view-pane view-id opts)]
    (ui/children! parent [scene-view-pane])
    view-id))

(g/defnk produce-frame [render-args ^GLAutoDrawable drawable]
  (with-drawable-as-current drawable
    (render! render-args gl-context nil)
    (let [[w h] (vp-dims (:viewport render-args))
          buf-image (read-to-buffered-image w h)]
      (scene-cache/prune-object-caches! gl)
      buf-image)))

(g/defnode PreviewView
  (inherits view/WorkbenchView)
  (inherits SceneRenderer)

  (property width g/Num)
  (property height g/Num)
  (property tool-picking-rect Rect)
  (property cursor-pos types/Vec2)
  (property select-buffer IntBuffer)
  (property image-view ImageView)
  (property drawable GLAutoDrawable)

  (input input-handlers Runnable :array)
  (input active-tool g/Keyword)
  (input updatables g/Any)
  (input selected-updatables g/Any)
  (input picking-rect Rect)
  (input tool-renderables pass/RenderData :array)

  (output active-tool g/Keyword (gu/passthrough active-tool))
  (output viewport Region (g/fnk [width height] (types/->Region 0 width 0 height)))
  (output selection g/Any (gu/passthrough selection))
  (output picking-selection g/Any :cached produce-selection)
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables)
  (output frame BufferedImage :cached produce-frame)
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output all-renderables g/Any (gu/passthrough renderables)))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView :width width :height height :drawable (make-drawable width height) :select-buffer (make-select-buffer)))


(defmulti attach-grid
  (fn [grid-node-type grid-node-id view-id resource-node camera]
    (:key @grid-node-type)))

(defmethod attach-grid :editor.grid/Grid
  [_ grid-node-id view-id resource-node camera]
  (concat
    (g/connect grid-node-id :renderable view-id      :aux-renderables)
    (g/connect camera       :camera     grid-node-id :camera)))

(defmulti attach-tool-controller
  (fn [tool-node-type tool-node-id view-id resource-node]
    (:key @tool-node-type)))

(defmethod attach-tool-controller :default
  [_ tool-node view-id resource-node])

(defn setup-view [view-id resource-node opts]
  (let [view-graph  (g/node-id->graph-id view-id)
        app-view-id (:app-view opts)
        select-fn   (:select-fn opts)
        prefs       (:prefs opts)
        project     (:project opts)
        grid-type   (cond
                      (true? (:grid opts)) grid/Grid
                      (:grid opts) (:grid opts)
                      :else grid/Grid)
        tool-controller-type (get opts :tool-controller scene-tools/ToolController)]
    (concat
      (g/make-nodes view-graph
                    [background      background/Background
                     selection       [selection/SelectionController :select-fn (fn [selection op-seq]
                                                                                 (g/transact
                                                                                   (concat
                                                                                     (g/operation-sequence op-seq)
                                                                                     (g/operation-label "Select")
                                                                                     (select-fn selection))))]
                     camera          [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic identity {:fov-x 1000 :fov-y 1000}))]
                     grid            grid-type
                     tool-controller [tool-controller-type :prefs prefs]
                     rulers          [rulers/Rulers]]

                    (g/connect resource-node        :scene                     view-id          :scene)

                    (g/connect background           :renderable                view-id          :aux-renderables)

                    (g/connect camera               :camera                    view-id          :camera)
                    (g/connect camera               :input-handler             view-id          :input-handlers)
                    (g/connect view-id              :viewport                  camera           :viewport)

                    (g/connect app-view-id          :selected-node-ids         view-id          :selection)

                    (g/connect app-view-id          :active-tool               view-id          :active-tool)

                    (g/connect tool-controller      :input-handler             view-id          :input-handlers)
                    (g/connect tool-controller      :renderables               view-id          :tool-renderables)
                    (g/connect view-id              :active-tool               tool-controller  :active-tool)
                    (g/connect view-id              :viewport                  tool-controller  :viewport)
                    (g/connect camera               :camera                    tool-controller  :camera)
                    (g/connect view-id              :selected-renderables      tool-controller  :selected-renderables)
                    (attach-tool-controller tool-controller-type tool-controller view-id resource-node)

                    (if (:grid opts)
                      (attach-grid grid-type grid view-id resource-node camera)
                      (g/delete-node grid))

                    (g/connect resource-node        :_node-id                  selection        :root-id)
                    (g/connect selection            :renderable                view-id          :tool-renderables)
                    (g/connect selection            :input-handler             view-id          :input-handlers)
                    (g/connect selection            :picking-rect              view-id          :picking-rect)
                    (g/connect view-id              :picking-selection         selection        :picking-selection)
                    (g/connect view-id              :selection                 selection        :selection)

                    (g/connect camera :camera rulers :camera)
                    (g/connect rulers :renderables view-id :aux-renderables)
                    (g/connect view-id :viewport rulers :viewport)
                    (g/connect view-id :cursor-pos rulers :cursor-pos))
      (when-let [node-id (:select-node opts)]
        (select-fn [node-id])))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view-id (make-scene-view graph parent opts)]
    (g/transact
      (setup-view view-id resource-node opts))
    view-id))

(defn make-preview [graph resource-node opts width height]
  (let [view-id (make-preview-view graph width height)]
    (g/transact
      (setup-view view-id resource-node (dissoc opts :grid)))
    (frame-selection view-id false)
    view-id))

(defn- focus-view [view-id opts]
  (when-let [image-view ^ImageView (g/node-value view-id :image-view)]
    (.requestFocus image-view)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :scene
                                :label "Scene"
                                :make-view-fn make-view
                                :make-preview-fn make-preview
                                :focus-fn focus-view))

(g/defnk produce-transform [^Vector3d position-v3 ^Quat4d rotation-q4 ^Vector3d scale-v3]
  (math/->mat4-non-uniform position-v3 rotation-q4 scale-v3))

(def produce-no-transform-properties (g/constantly #{}))
(def produce-scalable-transform-properties (g/constantly #{:position :rotation :scale}))
(def produce-unscalable-transform-properties (g/constantly #{:position :rotation}))

(g/defnode SceneNode
  (property position types/Vec3 (default [0.0 0.0 0.0])
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :position))))
  (property rotation types/Vec4 (default [0.0 0.0 0.0 1.0])
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :rotation)))
            (dynamic edit-type (g/constantly (properties/quat->euler))))
  (property scale types/Vec3 (default [1.0 1.0 1.0])
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :scale))))

  (output transform-properties g/Any :abstract)
  (output position-v3 Vector3d :cached (g/fnk [^types/Vec3 position] (doto (Vector3d.) (math/clj->vecmath position))))
  (output rotation-q4 Quat4d :cached (g/fnk [^types/Vec4 rotation] (doto (Quat4d.) (math/clj->vecmath rotation))))
  (output scale-v3 Vector3d :cached (g/fnk [^types/Vec3 scale] (Vector3d. (double-array scale))))
  (output transform Matrix4d :cached produce-transform)
  (output scene g/Any :cached (g/fnk [^g/NodeID _node-id ^Matrix4d transform] {:node-id _node-id :transform transform}))
  (output aabb AABB :cached (g/constantly (geom/null-aabb))))

(defmethod scene-tools/manip-movable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :position))

(defmethod scene-tools/manip-rotatable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :rotation))

(defmethod scene-tools/manip-scalable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :scale))

(defmethod scene-tools/manip-move ::SceneNode [evaluation-context node-id delta]
  (let [orig-p ^Vector3d (doto (Vector3d.) (math/clj->vecmath (g/node-value node-id :position evaluation-context)))
        p (doto (Vector3d. orig-p) (.add delta))]
    (g/set-property node-id :position (properties/round-vec [(.x p) (.y p) (.z p)]))))

(defmethod scene-tools/manip-rotate ::SceneNode [evaluation-context node-id delta]
  (let [new-rotation (math/vecmath->clj
                       (doto (Quat4d.)
                         (math/clj->vecmath (g/node-value node-id :rotation evaluation-context))
                         (.mul delta)))]
    ;; Note! The rotation is not rounded here like manip-move and manip-scale.
    ;; As the user-facing property is the euler angles, they are rounded in properties/quat->euler.
    (g/set-property node-id :rotation new-rotation)))

(defmethod scene-tools/manip-scale ::SceneNode [evaluation-context node-id delta]
  (let [s (Vector3d. (double-array (g/node-value node-id :scale evaluation-context)))
        ^Vector3d d delta]
    (.setX s (* (.x s) (.x d)))
    (.setY s (* (.y s) (.y d)))
    (.setZ s (* (.z s) (.z d)))
    (g/set-property node-id :scale (properties/round-vec [(.x s) (.y s) (.z s)]))))

(defn selection->movable [selection]
  (handler/selection->node-ids selection scene-tools/manip-movable?))

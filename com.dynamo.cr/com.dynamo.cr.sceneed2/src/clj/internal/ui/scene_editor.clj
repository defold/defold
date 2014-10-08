(ns internal.ui.scene-editor
  (:require [clojure.core.async :refer [<! >! go-loop]]
            [plumbing.core :refer [defnk]]
            [dynamo.editors :as e]
            [dynamo.types :as t]
            [dynamo.geom :as g]
            [dynamo.camera :as c]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.ui :as ui]
            [schema.core :as s]
            [dynamo.gl :refer :all]
            [service.log :as log]
            [internal.render.pass :as pass]
            [internal.ui.background :as back]
            [internal.fps :refer [new-fps-tracker]]
            [internal.ui.grid :as grid])
  (:import [javax.media.opengl GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [java.nio IntBuffer]
           [java.awt Font]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
           [org.eclipse.swt.opengl GLData GLCanvas]
           [dynamo.types Camera Region]))

(set! *warn-on-reflection* true)

(def PASS_SHIFT        32)
(def INDEX_SHIFT       (+ PASS_SHIFT 4))
(def MANIPULATOR_SHIFT 62)

(defn z-distance [camera obj]
  (let [p (->> (Point3d.)
            (g/world-space obj)
            (c/camera-project camera (:viewport camera)))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera obj]
  (+ (z-distance camera obj)
     (bit-shift-left (or (:index obj) 0)        INDEX_SHIFT)
     (bit-shift-left (or (:manipulator? obj) 0) MANIPULATOR_SHIFT)))

(defnk produce-render-data :- t/RenderData
  [this g renderables :- [t/RenderData] camera]
  (assert camera (str "No camera is available as an input to Scene Renderer (node:" (:_id this) "."))
  (apply merge-with (fn [a b] (reverse (sort-by #(render-key camera %) (concat a b)))) renderables))

(n/defnode SceneRenderer
  (input camera      Camera)
  (input renderables [t/RenderData])
  (input controller  t/Node)

  (output render-data t/RenderData produce-render-data))

(defn on-resize
  [evt editor state]
  (let [{:keys [^GLCanvas canvas camera-node-id project-state]} @state
        camera-node (p/resource-by-id project-state camera-node-id)
        client      (.getClientArea canvas)
        viewport    (t/->Region 0 (.width client) 0 (.height client))
        aspect      (/ (double (.width client)) (.height client))
        camera      (:camera camera-node)
        new-camera  (-> camera
                      (c/set-orthographic (:fov camera)
                                        aspect
                                        -100000
                                        100000)
                      (assoc :viewport viewport))]
    (p/transact project-state [(p/update-resource camera-node assoc :camera new-camera)])
    (ui/request-repaint editor)))

(defn setup-pass
  [context ^GL2 gl ^GLU glu pass camera ^Region viewport]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glLoadIdentity gl)
  (if (t/model-transform? pass)
    (gl-mult-matrix-4d gl (c/camera-projection-matrix camera))
    (glu-ortho glu viewport))
  (.glMatrixMode gl GL2/GL_MODELVIEW)
  (.glLoadIdentity gl)
  (when (t/model-transform? pass)
    (gl-load-matrix-4d gl (c/camera-view-matrix camera)))
  (pass/prepare-gl pass gl glu))

(defn do-paint
  [{:keys [project-state ^GLContext context ^GLCanvas canvas camera-node-id render-node-id small-text-renderer] :as state}]
  (when (not (.isDisposed canvas))
    (.setCurrent canvas)
    (with-context context [gl glu]
      (try
        (gl-clear gl 0.0 0.0 0.0 1)

        (let [camera      (p/get-resource-value project-state (p/resource-by-id project-state camera-node-id) :camera)
              render-data (p/get-resource-value project-state (p/resource-by-id project-state render-node-id) :render-data)
              viewport    (:viewport camera)]
          (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport)))
          (when render-data
            (doseq [pass pass/passes]
              (setup-pass context gl glu pass camera viewport)
              (doseq [node (get render-data pass)]
                (gl-push-matrix gl
                    (when (t/model-transform? pass)
                      (gl-mult-matrix-4d gl (:world-transform node)))
                    (try
                      (when (:render-fn node)
                        ((:render-fn node) context gl glu small-text-renderer))
                      (catch Exception e
                        (log/error :exception e
                                   :pass pass
                                   :message (str (.getMessage e) "skipping node " (class node) (:_id node) "\n ** trace: " (clojure.stacktrace/print-stack-trace e 30))))))))))
        (finally
          (.swapBuffers canvas))))))

(defn batch-repaint
  [state]
  (when-not (:paint-pending @state)
    (swap! state assoc :paint-pending true)
    (ui/after 1
             (swap! state dissoc :paint-pending)
             (do-paint @state)
             ;; uncomment the following for an FPS test
             ;; (batch-repaint state)
             )))

(defn- start-event-pump
  [editor canvas {:keys [render-node-id project-state]}]
  (let [event-chan (ui/make-event-channel)]
    (doseq [evt [:resize :mouse-down :mouse-up :mouse-double-click :mouse-enter :mouse-exit :mouse-hover :mouse-move :mouse-wheel :key-down :key-up]]
      (ui/pipe-events-to-channel canvas evt event-chan))
    (go-loop []
       (when-let [e (<! event-chan)]
         (try
           (p/publish (p/resource-feeding-into (p/resource-by-id project-state render-node-id) :controller) e)
           (catch Exception ex (.printStackTrace ex)))
         (recur)))
    event-chan))

(deftype SceneEditor [state scene-node]
  e/Editor
  (init [this site]
    (binding [ui/*view* this]
      (let [render-node       (make-scene-renderer :_id -1 :editor this)
            background-node   (back/make-background :_id -2)
            grid-node         (grid/make-grid :_id -3)
            fps-node          (new-fps-tracker)
            camera-node       (c/make-camera-node :camera (c/make-camera :orthographic) :_id -4)
            camera-controller (c/make-camera-controller)
            tx-result         (p/transact (:project-state @state)
                                          [(p/new-resource render-node)
                                           (p/new-resource background-node)
                                           (p/new-resource grid-node)
                                           (p/new-resource camera-node)
                                           (p/new-resource camera-controller)
                                           (p/new-resource fps-node)
                                           (p/connect fps-node          :renderable render-node :renderables)
                                           (p/connect scene-node        :renderable render-node :renderables)
                                           (p/connect background-node   :renderable render-node :renderables)
                                           (p/connect grid-node         :renderable render-node :renderables)
                                           (p/connect camera-node       :camera     render-node :camera)
                                           (p/connect camera-node       :camera     grid-node   :camera)
                                           (p/connect camera-node       :camera     camera-controller :camera)
                                           (p/connect camera-controller :self       render-node :controller)])]
       (swap! state assoc
              :camera-node-id (p/resolve-tempid tx-result -4)
              :render-node-id (p/resolve-tempid tx-result -1)))))

  (create-controls [this parent]
    (let [canvas        (glcanvas parent)
          factory       (glfactory)
          _             (.setCurrent canvas)
          context       (.createExternalGLContext factory)
          _             (.makeCurrent context)
          gl            (.. context getGL getGL2)
          vertex-arr    (IntBuffer/allocate 1)
          _             (.glGenVertexArrays gl 1 vertex-arr)
          vertex-arr-id (.get vertex-arr 0)
          _             (.glBindVertexArray gl vertex-arr-id)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (ui/listen canvas :resize on-resize this state)
      (swap! state assoc
             :context context
             :canvas  canvas
             :event-channel (start-event-pump this canvas @state)
             :small-text-renderer (text-renderer Font/SANS_SERIF Font/BOLD 12))))

  (save [this file monitor])
  (dirty? [this] false)
  (save-as-allowed? [this] false)
  (set-focus [this])

  ui/Repaintable
  (request-repaint [this] (batch-repaint state)))

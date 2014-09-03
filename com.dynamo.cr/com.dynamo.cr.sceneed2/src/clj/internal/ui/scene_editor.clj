(ns internal.ui.scene-editor
  (:require [plumbing.core :refer [defnk]]
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
            [internal.ui.grid :as grid])
  (:import [javax.media.opengl GL2]
           [java.nio IntBuffer]
           [java.awt Font]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
           [org.eclipse.swt SWT]
           [dynamo.types Camera Region]))

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
  {:inputs     {:renderables [t/RenderData]
                :camera      Camera}
   :cached     #{:render-data}
   :on-update  #{:render-data}
   :transforms {:render-data #'produce-render-data}})

(defn setup-pass
  [context gl glu pass camera viewport]
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
  [{:keys [project-state context canvas camera-node-id render-node-id] :as state}]
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
                          ((:render-fn node) context gl glu))
                        (catch Exception e
                          (log/error :exception e
                                     :pass pass
                                     :message (str (.getMessage e) "skipping node " (class node) (:_id node) "\n ** trace: " (clojure.stacktrace/print-stack-trace e 30))))))))))
          (finally
            (.swapBuffers canvas))))))


(defn on-resize
  [evt editor state]
  (let [{:keys [canvas camera-node-id project-state]} @state
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

(deftype SceneEditor [state scene-node]
  e/Editor
  (init [this site]
    (let [render-node     (make-scene-renderer :_id -1)
          background-node (back/make-background :_id -2)
          grid-node       (grid/make-grid :_id -3)
          camera-node     (c/make-camera-node :camera (c/make-camera :orthographic) :_id -4)
          tx-result       (p/transact (:project-state @state)
                                      [(p/new-resource render-node)
                                       (p/new-resource background-node)
                                       (p/new-resource grid-node)
                                       (p/new-resource camera-node)
                                       (p/connect scene-node      :renderable render-node :renderables)
                                       (p/connect background-node :renderable render-node :renderables)
                                       (p/connect grid-node       :renderable render-node :renderables)
                                       (p/connect camera-node     :camera     render-node :camera)
                                       (p/connect camera-node     :camera     grid-node   :camera)])]
      (swap! state assoc
             :camera-node-id (p/resolve-tempid tx-result -4)
             :render-node-id (p/resolve-tempid tx-result -1))))

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
          _             (.glBindVertexArray gl vertex-arr-id)
          ]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (e/listen canvas SWT/Resize on-resize this state)
      (swap! state assoc
             :context context
             :canvas  canvas
             :small-text-renderer (text-renderer Font/SANS_SERIF Font/BOLD 12))))

  (save [this file monitor])
  (dirty? [this] false)
  (save-as-allowed? [this] false)
  (set-focus [this])

  ui/Repaintable
  (request-repaint [this]
    (when-not (:paint-pending @state)
      (e/after 15
             (swap! state dissoc :paint-pending)
             (do-paint @state)))))


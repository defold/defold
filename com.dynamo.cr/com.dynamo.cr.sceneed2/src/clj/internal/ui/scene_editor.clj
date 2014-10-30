(ns internal.ui.scene-editor
  (:require [clojure.core.async :refer [<! >! go-loop]]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.camera :as c]
            [dynamo.editors :as e]
            [dynamo.geom :as g]
            [dynamo.gl :refer :all]
            [dynamo.node :as n :refer [Scope]]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [service.log :as log]
            [internal.render.pass :as pass]
            [internal.ui.background :as back]
            [internal.fps :refer [new-fps-tracker]]
            [internal.ui.grid :as grid]
            [internal.query :as iq])
  (:import [javax.media.opengl GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [java.nio IntBuffer]
           [java.awt Font]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
           [org.eclipse.swt.opengl GLData GLCanvas]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Camera Region AABB]))

(set! *warn-on-reflection* true)

(def CONTEXT_ID "com.dynamo.cr.clojure.contexts.scene-editor")

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
  [this g renderables :- [t/RenderData] view-camera :- Camera]
  (assert view-camera (str "No camera is available as an input to Scene Renderer (node:" (:_id this) "."))
  (apply merge-with (fn [a b] (reverse (sort-by #(render-key view-camera %) (concat a b)))) renderables))

(defn setup-pass
  [context ^GL2 gl ^GLU glu pass camera]
  (let [viewport ^Region (:viewport camera)]
    (.glMatrixMode gl GL2/GL_PROJECTION)
    (.glLoadIdentity gl)
    (if (t/model-transform? pass)
      (gl-mult-matrix-4d gl (c/camera-projection-matrix camera))
      (glu-ortho glu viewport))
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (.glLoadIdentity gl)
    (when (t/model-transform? pass)
      (gl-load-matrix-4d gl (c/camera-view-matrix camera)))
    (pass/prepare-gl pass gl glu)))

(defn gl-viewport [^GL2 gl camera]
  (let [viewport ^Region (:viewport camera)]
    (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport)))))

(defnk paint-renderer
  [^GLContext context ^GLCanvas canvas this ^Camera view-camera text-renderer]
  (ui/swt-safe
    (when (and canvas (not (.isDisposed canvas)))
      (.setCurrent canvas)
      (with-context context [gl glu]
        (try
          (gl-clear gl 0.0 0.0 0.0 1)
          (gl-viewport gl view-camera)
          (when-let [renderables (n/get-node-value this :render-data)]
            (doseq [pass pass/passes]
              (setup-pass context gl glu pass view-camera)
              (doseq [node (get renderables pass)]
                (gl-push-matrix gl
                    (when (t/model-transform? pass)
                      (gl-mult-matrix-4d gl (:world-transform node)))
                    (try
                      (when (:render-fn node)
                        ((:render-fn node) context gl glu text-renderer))
                      (catch Exception e
                        (log/error :exception e
                                   :pass pass
                                   :message (str (.getMessage e) "skipping node " (class node) (:_id node) "\n ** trace: " (clojure.stacktrace/print-stack-trace e 30)))))))))
          (finally
            (.swapBuffers canvas)))))))

(defnk passthrough-aabb
  [aabb]
  aabb)

(n/defnode Renderer
  (input view-camera Camera)
  (input renderables [t/RenderData])
  (input controller  t/Node)
  (input aabb        AABB)

  (property context GLContext)
  (property canvas  GLCanvas)
  (property text-renderer TextRenderer)

  (output render-data t/RenderData produce-render-data)
  (output paint s/Keyword :on-update paint-renderer)
  (output aabb AABB passthrough-aabb)

  (on :resize
    (let [canvas      (:canvas self)
          client      (.getClientArea ^GLCanvas canvas)
          viewport    (t/->Region 0 (.width client) 0 (.height client))
          aspect      (/ (double (.width client)) (.height client))
          camera-node (iq/node-feeding-into self :view-camera)
          camera      (n/get-node-value camera-node :camera)
          new-camera  (-> camera
                        (c/set-orthographic (:fov camera)
                                            aspect
                                            -100000
                                            100000)
                        (assoc :viewport viewport))]
      (ds/set-property camera-node :camera new-camera)))

  (on :reframe
    (let [camera-node (iq/node-feeding-into self :view-camera)
          camera      (n/get-node-value camera-node :camera)
          aabb        (n/get-node-value self :aabb)]
      (when aabb ;; there exists an aabb to center on
        (ds/set-property camera-node :camera (c/camera-orthographic-frame-aabb camera aabb))))))

(defn- dispatch-to-controller-of [evt self]
  (t/process-one-event (iq/node-feeding-into self :controller) evt))

(defn start-event-pump
  [canvas self]
  (doseq [evt-type [:mouse-down :mouse-up :mouse-double-click :mouse-enter :mouse-exit :mouse-hover :mouse-move :mouse-wheel :key-down :key-up]]
    (ui/listen canvas evt-type dispatch-to-controller-of self)))

(defn pipe-events-to-node
  [component type {:keys [_id world-ref] :as node}]
  (ui/listen component type #(t/process-one-event (iq/node-by-id world-ref _id) %)))

(defn- send-view-scope-message
  [self txn]
  (doseq [n (:nodes-added txn)]
    (ds/send-after n {:type :view-scope :scope self})))

(n/defnode SceneEditor
  (inherits Scope)
  (inherits Renderer)

  (input controller  s/Any)

  (property triggers {:schema s/Any :default [#'send-view-scope-message]})

  (on :create
    (let [canvas        (glcanvas (:parent event))
          factory       (glfactory)
          _             (.setCurrent canvas)
          context       (.createExternalGLContext factory)
          _             (.makeCurrent context)
          gl            (.. context getGL getGL2)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (pipe-events-to-node canvas :resize self)
      (start-event-pump canvas self)
      (ds/set-property self
        :context context
        :canvas canvas
        :text-renderer (text-renderer Font/SANS_SERIF Font/BOLD 12)))))

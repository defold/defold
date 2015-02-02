(ns internal.ui.scene-editor
  (:require [clojure.core.async :refer [<! >! go-loop]]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.camera :as c]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.node :as n :refer [Scope]]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [service.log :as log]
            [internal.metrics :as metrics]
            [internal.disposal :as disp]
            [internal.node :as in]
            [internal.render.pass :as pass]
            [internal.repaint :as repaint])
  (:import [javax.media.opengl GL2 GLContext]
           [javax.media.opengl.glu GLU]
           [java.nio ByteOrder ByteBuffer IntBuffer]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
           [org.eclipse.swt.opengl GLCanvas]
           [dynamo.types Camera Region ]
           [internal.ui IDirtyable]))

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
  ([context gl glu pass camera]
    (setup-pass context gl glu pass camera nil))
  ([context ^GL2 gl ^GLU glu pass camera pick-rect]
    (let [viewport ^Region (:viewport camera)]
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
      (pass/prepare-gl pass gl glu))))

(defn gl-viewport [^GL2 gl camera]
  (let [viewport ^Region (:viewport camera)]
    (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport)))))

(defn render
  [context gl glu text-renderer pass renderable]
  (gl/gl-push-matrix
    gl
    (when (t/model-transform? pass)
      (gl/gl-mult-matrix-4d gl (:world-transform renderable)))
    (try
      (when (:render-fn renderable)
        ((:render-fn renderable) context gl glu text-renderer))
      (catch Exception e
        (log/error :exception e
                   :pass pass
                   :renderable renderable
                   :message "skipping renderable")))))

(defn paint-renderer
  [^GLContext context ^GLCanvas canvas this ^Camera view-camera text-renderer]
  (metrics/paint)
  (ui/swt-safe
   (when (and canvas (not (.isDisposed canvas)))
     (.setCurrent canvas)
     (gl/with-context context [gl glu]
       (try
         (gl/gl-clear gl 0.0 0.0 0.0 1)
         (gl-viewport gl view-camera)
         (when-let [renderables (n/get-node-value this :render-data)]
           (doseq [pass pass/render-passes]
             (setup-pass context gl glu pass view-camera)
             (doseq [node (get renderables pass)]
               (render context gl glu text-renderer pass node))))
         (finally
           (.swapBuffers canvas)
           (metrics/paint-complete)))))))

(def ^:private pick-buffer-size 4096)

(defn selection-renderer
  "Renders the renderables, each of which should be a map containing
  :select-name (an integer) and :render-fn. Returns a collection of
  names (integers) found within the pick-rect (map of :x :y :width :height)."
  [^GLContext context renderables ^Camera view-camera pick-rect]
  (let [select-buffer (.. ByteBuffer
                        (allocateDirect (* 4 pick-buffer-size))
                        (order (ByteOrder/nativeOrder))
                        asIntBuffer)]
    (gl/with-context context [gl glu]
     (.glSelectBuffer gl pick-buffer-size select-buffer)
     (.glRenderMode gl GL2/GL_SELECT)
     (.glInitNames gl)
     (doseq [pass pass/selection-passes]
       (setup-pass context gl glu pass view-camera pick-rect)
       (doseq [renderable (get renderables pass)]
         (.glPushName gl (:select-name renderable))
         (render context gl glu nil pass renderable)
         (.glPopName gl)))
     (.glFlush gl)  ; is this necessary?
     (let [hit-count (.glRenderMode gl GL2/GL_RENDER)]
       (gl/select-buffer-names hit-count select-buffer)))))

(defn dispatch-to-controller-of [evt self]
  (t/process-one-event (ds/node-feeding-into self :controller) evt))

(defn start-event-pump
  [canvas self]
  (doseq [evt-type [:mouse-down :mouse-up :mouse-double-click :mouse-enter :mouse-exit :mouse-hover :mouse-move :mouse-wheel :key-down :key-up]]
    (ui/listen canvas evt-type dispatch-to-controller-of self)))

(defn pipe-events-to-node
  [component type {:keys [_id world-ref] :as node}]
  (ui/listen component type #(t/process-one-event (ds/node world-ref _id) %)))

(defn send-view-scope-message
  [transaction graph self label kind]
  (when (ds/is-modified? transaction self)
    (doseq [id (:nodes-added transaction)]
      (ds/send-after {:_id id} {:type :view-scope :scope self}))))

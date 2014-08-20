(ns internal.ui.scene-editor
  (:require [plumbing.core :refer [defnk]]
            [dynamo.editors :as e]
            [dynamo.types :as t]
            [dynamo.geom :as g]
            [dynamo.camera :as c]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.ui :as ui]
            [internal.ui.gl :refer :all]
            [internal.ui.editors :refer :all]
            [internal.render.pass :as pass])
  (:import [javax.media.opengl GL2]
           [java.awt Font]
           [javax.vecmath Point3d Matrix4d]
           [dynamo.types Camera Region]))

(def PASS_SHIFT        32)
(def INDEX_SHIFT       (+ PASS_SHIFT 4))
(def MANIPULATOR_SHIFT 62)

(defn z-distance [camera viewport obj]
  (let [p (->> (Point3d.)
            (g/world-space obj)
            (c/camera-project camera viewport))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera viewport obj]
  (+ (z-distance camera obj)
     (bit-shift-left (or (:index obj) 0)        INDEX_SHIFT)
     (bit-shift-left (or (:manipulator? obj) 0) MANIPULATOR_SHIFT)))

(defnk produce-render-data :- t/RenderData
  [this g renderables :- [t/RenderData]]
  (let [camera   (:camera this)
        viewport (:viewport this)]
    (apply merge-with (fn [a b] (sort-by #(render-key camera viewport %) (concat a b))) renderables)))

(n/defnode SceneRenderer
  {:properties {:camera      {:scbema Camera}
                :viewport    {:schema Region}}
   :inputs     {:renderables [t/RenderData]}
   :cached     #{:render-data}
   :transforms {:render-data #'produce-render-data}})

(defnk background-renderable :- t/RenderData
  [this g]
  {pass/background
   [{:world-transform g/Identity4d
     :render-fn
     (fn [context gl glu]
       (let [top-color    (float-array [0.4765 0.5508 0.6445])
             bottom-color (float-array [0 0 0])
             x0           (float -1.0)
             x1           (float 1.0)
             y0           (float -1.0)
             y1           (float 1.0)]
         (gl-quads gl
                   (gl-color-3fv top-color 0)
                   (gl-vertex-2f x0 y1)
                   (gl-vertex-2f x1 y1)
                   (gl-color-3fv bottom-color 0)
                   (gl-vertex-2f x1 y0)
                   (gl-vertex-2f x0 y0))))}]})

(n/defnode Background
  {:transforms {:renderable #'background-renderable}})

(defn do-paint
  [{:keys [project-state context canvas render-node-id viewport]}]
  (when (not (.isDisposed canvas))
      (.setCurrent canvas)
      (with-context context [gl glu]
        (try
          (gl-clear gl 0.0 0.0 0.0 1)

          ;; (.glViewport gl (.left viewport) (.top viewport) (.right viewport) (.bottom viewport))

          (when-let [render-data (p/get-resource-value project-state
                                                       (p/resource-by-id project-state render-node-id)
                                                       :render-data)]
            (doseq [pass pass/passes]
              (doseq [node (get render-data pass)]
                (when (:render-fn node)
                  ((:render-fn node) context gl glu)))))
          (finally
            (.swapBuffers canvas))))))

(deftype SceneEditor [state scene-node]
  e/Editor
  (init [this site]
    (let [render-node     (make-scene-renderer :_id -1)
          background-node (make-background :_id -2)
          tx-result       (p/transact (:project-state @state)
                                      [(p/new-resource render-node)
                                       (p/new-resource background-node)
                                       (p/connect scene-node :renderable render-node :renderables)
                                       (p/connect background-node :renderable render-node :renderables)])]
      (swap! state assoc
             :render-node-id (p/resolve-tempid tx-result -1))))

  (create-controls [this parent]
    (let [canvas        (glcanvas parent)
          factory       (glfactory)
          _             (.setCurrent canvas)
          context       (.createExternalGLContext factory)
          _             (.makeCurrent context)
          gl            (.. context getGL getGL2)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
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
      (swap! state assoc :paint-pending do-paint)
      (swt-timed-exec 15
                      (do-paint @state)))))


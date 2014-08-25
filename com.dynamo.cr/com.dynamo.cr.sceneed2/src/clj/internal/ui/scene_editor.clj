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
            [internal.ui.gl :refer :all]
            [internal.ui.editors :refer :all]
            [internal.render.pass :as pass])
  (:import [javax.media.opengl GL2]
           [java.awt Font]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
           [org.eclipse.swt.widgets Listener]
           [org.eclipse.swt SWT]
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
  {:properties {:camera      {:schema Camera}
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

(def min-align (/ (Math/sqrt 2.0) 2.0))
(def grid-color [0.44705 0.44314 0.5098])

(defmulti as-array class)

(defmethod as-array Vector3d
  [v]
  (let [vals (double-array 3)]
    (.get v vals)
    vals))

(defmethod as-array Vector4d
  [v]
  (let [vals (double-array 4)]
    (.get v vals)
    vals))

(defn render-grid-axis
  [gl vx uidx start stop size vidx min max]
  (doseq [u (range start stop size)]
      (aset vx uidx u)
      (aset vx vidx min)
      (gl-vertex-3dv gl vx 0)
      (aset vx vidx max)
      (gl-vertex-3dv gl vx 0)))

(defn render-grid
  [gl fixed-axis size aabb]
  ;; draw across
  (let [min-values (as-array (.min aabb))
       max-values (as-array (.max aabb))
       u-axis     (mod (inc fixed-axis) 3)
       u-min      (nth min-values u-axis)
       u-max      (nth max-values u-axis)
       v-axis     (mod (inc u-axis) 3)
       v-min      (nth min-values v-axis)
       v-max      (nth max-values v-axis)
       vertex     (double-array 3)]
    (aset vertex fixed-axis 0.0)
    (render-grid-axis gl vertex u-axis u-min u-max size v-axis v-min v-max)
    (render-grid-axis gl vertex v-axis v-min v-max size u-axis u-min u-max)))

(defnk grid-renderable :- t/RenderData
  [this g project camera]
  {pass/transparent
   [{:world-transform g/Identity4d
     :render-fn
     (fn [context gl glu]
       (let [grids       (p/get-resource-value project this :grids)
             view-matrix (c/camera-view-matrix camera)
             dir         (double-array 4)
             _           (.getRow view-matrix 2 dir)]
         (doall
           (for [grid-index (range 2)
                  axis      (range 3)
                 :when      (> (aget dir axis) min-align)
                 :let       [ratio (nth (:ratios grids) grid-index)
                             alpha (* (aget dir axis) ratio)]
                 :when      (> ratio 0.0)]
             (gl-lines gl
                       (gl-color-3dv+a grid-color alpha)
                       (render-grid axis
                                    (nth (:sizes grids) grid-index)
                                    (nth (:aabbs grids) grid-index)))))))}]})

(def axis-vectors
  [(Vector4d. 1.0 0.0 0.0 0.0)
   (Vector4d. 0.0 1.0 0.0 0.0)
   (Vector4d. 0.0 0.0 1.0 0.0)])

(defn as-unit-vector
  [v]
  (let [v-unit (Vector4d. v)]
    (set! (. v-unit w) 0)
    (.normalize v-unit)
    v-unit))


(defn dot [x y] (.dot x y))

(defn most-aligned-axis
  [normal]
  (let [[xdot ydot zdot] (map #(Math/abs (dot normal %)) axis-vectors)]
    (cond
      (and (>= xdot ydot) (>= xdot zdot)) 0
      (and (>= ydot xdot) (>= ydot zdot)) 1
      (and (>= zdot xdot) (>= zdot ydot)) 2)))

(defn frustum-plane-projection
  [plane1 plane2]
  (let [m (Matrix3d. 0.0         0.0         1.0
                     (.x plane1) (.y plane1) (.z plane1)
                     (.x plane2) (.y plane2) (.z plane2))
        v (Vector3d. 0.0 (- (.w plane1)) (- (.w plane2)))]
    (.invert m)
    (.transform m v)
    v))

(defn frustum-projection-aabb
  [planes]
  (-> (g/null-aabb)
    (g/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 2)))
    (g/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 3)))
    (g/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 2)))
    (g/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 3)))))

(defn grid-ratio [extent]
  (let [exp (Math/log10 extent)]
    (- 1.0 (- exp (Math/floor exp)))))

(defn small-grid-size [extent]  (Math/pow 10.0 (dec (Math/floor (Math/log10 extent)))))
(defn large-grid-size [extent]  (Math/pow 10.0      (Math/floor (Math/log10 extent))))

(defn grid-snap-down [a sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [a sz] (* sz (Math/ceil  (/ a sz))))

(defn snap-out-to-grid
  [aabb grid-size]
  (t/->AABB (Vector3d. (grid-snap-down (.. aabb min x) grid-size)
                       (grid-snap-down (.. aabb min y) grid-size)
                       (grid-snap-down (.. aabb min z) grid-size))
            (Vector3d. (grid-snap-up   (.. aabb max x) grid-size)
                       (grid-snap-up   (.. aabb max y) grid-size)
                       (grid-snap-up   (.. aabb max z) grid-size))))

(defnk update-grids :- s/Any
  [this g camera]
  (let [frustum-planes   (c/viewproj-frustum-planes camera)
        far-z-plane      (nth frustum-planes 5)
        normal           (as-unit-vector far-z-plane)
        perp-axis        (most-aligned-axis normal)
        aabb             (frustum-projection-aabb frustum-planes)
        extent           (as-array (g/aabb-extent aabb))
        _                (aset-double extent perp-axis Double/POSITIVE_INFINITY)
        smallest-extent  (reduce min extent)
        first-grid-ratio (grid-ratio smallest-extent)
        grid-size-small  (small-grid-size smallest-extent)
        grid-size-large  (large-grid-size smallest-extent)]
    {:ratios [first-grid-ratio                        (- 1.0 first-grid-ratio)]
     :sizes  [grid-size-small                         grid-size-large]
     :aabbs  [(snap-out-to-grid aabb grid-size-small) (snap-out-to-grid aabb grid-size-large)]}))

(def GridProperties
  {:inputs     {:camera          Camera}
   :properties {:grid-color      (t/color)
                :auto-grid       (t/bool)
                :fixed-grid-size (t/non-negative-integer)}
   :cached     #{:grids}
   :transforms {:grids      #'update-grids
                :renderable #'grid-renderable}})

(n/defnode Grid
  GridProperties)

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
  (pass/prepare-gl pass gl glu)
  )

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
                    (when (:render-fn node)
                      ((:render-fn node) context gl glu)))))))
          (finally
            (.swapBuffers canvas))))))

(defn listen
  [c t f & args]
  (.addListener c t
    (proxy [Listener] []
      (handleEvent [evt]
        (apply f evt args)))))

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
          background-node (make-background :_id -2)
          grid-node       (make-grid :_id -3)
          camera-node     (c/make-camera-node :camera (c/make-camera :orthographic) :_id -4)
          tx-result       (p/transact (:project-state @state)
                                      [(p/new-resource render-node)
                                       (p/new-resource background-node)
                                       (p/new-resource grid-node)
                                       (p/new-resource camera-node)
                                       (p/connect scene-node      :renderable render-node :renderables)
                                       (p/connect background-node :renderable render-node :renderables)
                                       (p/connect grid-node       :renderable render-node :renderables)
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
          gl            (.. context getGL getGL2)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (listen canvas SWT/Resize on-resize this state)
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
      (swt-timed-exec 15
                      (swap! state dissoc :paint-pending)
                      (do-paint @state)))))


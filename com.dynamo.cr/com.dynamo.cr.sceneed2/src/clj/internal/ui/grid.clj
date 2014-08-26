(ns internal.ui.grid
  (:require [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [dynamo.camera :as c]
            [dynamo.geom :as g]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.types :as t]
            [dynamo.gl :refer :all]
            [internal.render.pass :as pass])
  (:import [javax.vecmath Vector3d Vector4d Matrix3d Matrix4d]
           [dynamo.types Camera]))


(def min-align (/ (Math/sqrt 2.0) 2.0))
(def grid-color [0.44705 0.44314 0.5098])
(def x-axis-color (color 200   0   0))
(def y-axis-color (color   0 200   0))
(def z-axis-color (color   0   0 200))

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

(defn render-primary-axes
  [gl aabb]
  (gl-color-3fv gl x-axis-color 0)
  (gl-vertex-3d gl (.. aabb min x) 0.0 0.0)
  (gl-vertex-3d gl (.. aabb max x) 0.0 0.0)
  (gl-color-3fv gl y-axis-color 0)
  (gl-vertex-3d gl 0.0 (.. aabb min y) 0.0)
  (gl-vertex-3d gl 0.0 (.. aabb max y) 0.0)
  (gl-color-3fv gl z-axis-color 0)
  (gl-vertex-3d gl 0.0 0.0 (.. aabb min z))
  (gl-vertex-3d gl 0.0 0.0 (.. aabb max z)))

(defn render-grid-sizes
  [gl dir grids]
  (doall
    (for [grid-index (range 2)
         axis      (range 3)
         :when      (> (aget dir axis) min-align)
         :let       [ratio (nth (:ratios grids) grid-index)
                     alpha (* (aget dir axis) ratio)]
         :when      (> ratio 0.0)]
     (do
       (gl-color-3dv+a gl grid-color alpha)
       (render-grid gl axis
                    (nth (:sizes grids) grid-index)
                    (nth (:aabbs grids) grid-index))))))

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
         (gl-lines gl
           (render-grid-sizes dir grids)
           (render-primary-axes (apply g/aabb-union (:aabbs grids))))))}]})

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

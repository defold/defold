(ns editor.grid
  (:require [editor.gl :as gl]
            [dynamo.graph :as g]
            [dynamo.property :as dp]
            [dynamo.types :as t :refer [min-p max-p]]
            [editor.camera :as c]
            [editor.geom :as geom]
            [internal.render.pass :as pass])
  (:import [dynamo.types AABB Camera]
           [javax.media.opengl GL GL2]
           [javax.vecmath Vector3d Vector4d Matrix3d Matrix4d Point3d]))

(def min-align (/ (Math/sqrt 2.0) 2.0))
(def grid-color [0.44705 0.44314 0.5098])
(def x-axis-color (gl/color 200   0   0))
(def y-axis-color (gl/color   0 200   0))
(def z-axis-color (gl/color   0   0 200))

(defn render-grid-axis
  [^GL2 gl ^doubles vx uidx start stop size vidx min max]
  (doseq [u (range start stop size)]
      (aset vx uidx ^double u)
      (aset vx vidx ^double min)
      (gl/gl-vertex-3dv gl vx 0)
      (aset vx vidx ^double max)
      (gl/gl-vertex-3dv gl vx 0)))

(defn render-grid
  [gl fixed-axis size aabb]
  ;; draw across
  (let [min-values (geom/as-array (min-p aabb))
        max-values  (geom/as-array (max-p aabb))
        u-axis      (mod (inc fixed-axis) 3)
        u-min       (nth min-values u-axis)
        u-max       (nth max-values u-axis)
        v-axis      (mod (inc u-axis) 3)
        v-min       (nth min-values v-axis)
        v-max       (nth max-values v-axis)
        vertex      (double-array 3)]
    (aset vertex fixed-axis 0.0)
    (render-grid-axis gl vertex u-axis u-min u-max size v-axis v-min v-max)
    (render-grid-axis gl vertex v-axis v-min v-max size u-axis u-min u-max)))

(defn render-primary-axes
  [^GL2 gl ^AABB aabb]
  (gl/gl-color-3fv gl x-axis-color 0)
  (gl/gl-vertex-3d gl (-> aabb min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb max-p .x) 0.0 0.0)
  (gl/gl-color-3fv gl y-axis-color 0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb max-p .y) 0.0)
  (gl/gl-color-3fv gl z-axis-color 0)
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb min-p .z))
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb max-p .z)))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids]
  (doall
    (for [grid-index (range 2) ; 0 1
          axis       (range 3) ; 0 1 2
          :let       [ratio (nth (:ratios grids) grid-index)
                      alpha (Math/abs (* (aget dir axis) ratio))]]
     (do
       (gl/gl-color-3dv+a gl grid-color alpha)
       (render-grid gl axis
                    (nth (:sizes grids) grid-index)
                    (nth (:aabbs grids) grid-index))))))

(defn render-scaled-grids
  [^GL2 gl pass renderables count]
  (let [renderable (first renderables)
        user-render-data (:user-render-data renderable)
        camera (:camera user-render-data)
        grids (:grids user-render-data)
        view-matrix (c/camera-view-matrix camera)
        dir         (double-array 4)
        _           (.getRow view-matrix 2 dir)]
    (gl/gl-lines gl
      (render-grid-sizes dir grids)
      (render-primary-axes (apply geom/aabb-union (:aabbs grids))))))

(g/defnk grid-renderable :- pass/RenderData
  [camera grids]
  {pass/transparent
   [{:world-transform geom/Identity4d
     :render-fn       render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids}}]})

(def axis-vectors
  [(Vector4d. 1.0 0.0 0.0 0.0)
   (Vector4d. 0.0 1.0 0.0 0.0)
   (Vector4d. 0.0 0.0 1.0 0.0)])

(defn as-unit-vector
  [^Vector4d v]
  (let [v-unit (Vector4d. v)]
    (set! (. v-unit w) 0)
    (.normalize v-unit)
    v-unit))


(defn dot
  ^double [^Vector4d x ^Vector4d y]
  (.dot x y))

(defn frustum-plane-projection
  [^Vector4d plane1 ^Vector4d plane2]
  (let [m (Matrix3d. 0.0         0.0         1.0
                     (.x plane1) (.y plane1) (.z plane1)
                     (.x plane2) (.y plane2) (.z plane2))
        v (Point3d. 0.0 (- (.w plane1)) (- (.w plane2)))]
    (.invert m)
    (.transform m v)
    v))

(defn frustum-projection-aabb
  [planes]
  (-> (geom/null-aabb)
    (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 2)))
    (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 3)))
    (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 2)))
    (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 3)))))

(defn grid-ratio [extent]
  (let [exp (Math/log10 extent)]
    (- 1.0 (- exp (Math/floor exp)))))

(defn small-grid-size [extent]  (Math/pow 10.0 (dec (Math/floor (Math/log10 extent)))))
(defn large-grid-size [extent]  (Math/pow 10.0      (Math/floor (Math/log10 extent))))

(defn grid-snap-down [a sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [a sz] (* sz (Math/ceil  (/ a sz))))

(defn snap-out-to-grid
  [^AABB aabb grid-size]
  (t/->AABB (Point3d. (grid-snap-down (-> aabb min-p .x) grid-size)
                      (grid-snap-down (-> aabb min-p .y) grid-size)
                      (grid-snap-down (-> aabb min-p .z) grid-size))
            (Point3d. (grid-snap-up   (-> aabb max-p .x) grid-size)
                      (grid-snap-up   (-> aabb max-p .y) grid-size)
                      (grid-snap-up   (-> aabb max-p .z) grid-size))))

(g/defnk update-grids :- t/Any
  [camera]
  (let [frustum-planes   (c/viewproj-frustum-planes camera)
        far-z-plane      (nth frustum-planes 5)
        normal           (as-unit-vector far-z-plane)
        aabb             (frustum-projection-aabb frustum-planes)
        extent           (geom/as-array (geom/aabb-extent aabb))
        perp-axis        2
        _                (aset-double extent perp-axis Double/POSITIVE_INFINITY)
        smallest-extent  (reduce min extent)
        first-grid-ratio (grid-ratio smallest-extent)
        grid-size-small  (small-grid-size smallest-extent)
        grid-size-large  (large-grid-size smallest-extent)]
    {:ratios [first-grid-ratio                        (- 1.0 first-grid-ratio)]
     :sizes  [grid-size-small                         grid-size-large]
     :aabbs  [(snap-out-to-grid aabb grid-size-small) (snap-out-to-grid aabb grid-size-large)]}))

(g/defnode Grid
  (input camera Camera)
  (property grid-color t/Color)
  (property auto-grid  t/Bool)
  (property fixed-grid-size dp/NonNegativeInt (default 0))

  (output grids      t/Any :cached update-grids)
  (output renderable pass/RenderData  grid-renderable))

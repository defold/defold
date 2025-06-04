;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.grid
  (:require [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.scene-cache :as scene-cache]
            [editor.types :as types])
  (:import com.jogamp.opengl.GL2
           [editor.types AABB Camera]
           [java.nio ByteBuffer ByteOrder DoubleBuffer]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)

(def min-align (/ (Math/sqrt 2.0) 2.0))
(def grid-color colors/scene-grid)
(def x-axis-color colors/scene-grid-x-axis)
(def y-axis-color colors/scene-grid-y-axis)
(def z-axis-color colors/scene-grid-z-axis)

(defn- make-grid-vertex-buffer [_1 _2]
  (-> (ByteBuffer/allocateDirect (* 3 8))
    (.order (ByteOrder/nativeOrder))
    (.asDoubleBuffer)))

(defn- ignore-grid-vertex-buffer [_1 _2 _3]
  ;; Nothing.
  nil)

(scene-cache/register-object-cache! ::grid-vertex make-grid-vertex-buffer ignore-grid-vertex-buffer ignore-grid-vertex-buffer)

(defn render-grid-axis
  [^GL2 gl ^DoubleBuffer vx uidx start stop size vidx min max]
  (doseq [u (range start stop size)]
      (.put vx ^int uidx ^double u)
      (.put vx ^int vidx ^double min)
      (gl/gl-vertex-3dv gl vx)
      (.put vx ^int vidx ^double max)
      (gl/gl-vertex-3dv gl vx)))

(defn render-grid
  [gl fixed-axis size aabb]
  (let [min-values (geom/as-array (types/min-p aabb))
        max-values (geom/as-array (types/max-p aabb))
        u-axis (mod (inc fixed-axis) 3)
        u-min (nth min-values u-axis)
        u-max (nth max-values u-axis)
        v-axis (mod (inc u-axis) 3)
        v-min (nth min-values v-axis)
        v-max (nth max-values v-axis)
        vertex ^DoubleBuffer (scene-cache/request-object! ::grid-vertex :grid-vertex {} nil)]
    (.put vertex ^int fixed-axis 0.0)
    (render-grid-axis gl vertex u-axis u-min u-max size v-axis v-min v-max)
    (render-grid-axis gl vertex v-axis v-min v-max size u-axis u-min u-max)))

(defn render-primary-axes
  [^GL2 gl ^AABB aabb]
  (gl/gl-color gl x-axis-color)
  (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0)
  (gl/gl-color gl y-axis-color)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0)
  (gl/gl-color gl z-axis-color)
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/min-p .z))
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/max-p .z)))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids]
  (doall
    (for [grid-index (range 2) ; 0 1
          axis       (range 3) ; 0 1 2
          :let       [ratio (nth (:ratios grids) grid-index)
                      alpha (Math/abs (* (aget dir axis) ratio))]]
     (do
       (gl/gl-color gl (colors/alpha grid-color alpha))
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

(g/defnk grid-renderable
  [camera grids]
  {pass/infinity-grid ; Grid lines stretching to infinity. Not depth-clipped to frustum.
   [{:world-transform geom/Identity4d
     :tags #{:grid}
     :render-fn render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids}}]
   pass/transparent ; Grid lines potentially intersecting scene geometry.
   [{:world-transform geom/Identity4d
     :tags #{:grid}
     :render-fn render-scaled-grids
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
  (-> geom/null-aabb
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
  (types/->AABB (Point3d. (grid-snap-down (-> aabb types/min-p .x) grid-size)
                      (grid-snap-down (-> aabb types/min-p .y) grid-size)
                      (grid-snap-down (-> aabb types/min-p .z) grid-size))
            (Point3d. (grid-snap-up   (-> aabb types/max-p .x) grid-size)
                      (grid-snap-up   (-> aabb types/max-p .y) grid-size)
                      (grid-snap-up   (-> aabb types/max-p .z) grid-size))))

(g/defnk update-grids
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
  (property grid-color types/Color)
  (property auto-grid  g/Bool)
  (property fixed-grid-size g/Int
            (default 0))
  (output grids      g/Any :cached update-grids)
  (output renderable pass/RenderData :cached grid-renderable))


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

(ns editor.curve-grid
  (:require [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.types :as types]
            [editor.camera :as c]
            [editor.gl.pass :as pass])
  (:import [editor.types AABB Camera]
           [com.jogamp.opengl GL2]
           [javax.vecmath Point3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def grid-color colors/mid-grey)
(def x-axis-color colors/defold-white)
(def y-axis-color colors/defold-white)

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
  (let [min-values (geom/as-array (types/min-p aabb))
        max-values (geom/as-array (types/max-p aabb))
        u-axis ^double (mod (inc ^int fixed-axis) 3)
        u-min (nth min-values u-axis)
        u-max (nth max-values u-axis)
        v-axis ^double (mod (inc ^int u-axis) 3)
        v-min (nth min-values v-axis)
        v-max (nth max-values v-axis)
        vertex (double-array 3)]
    (aset vertex fixed-axis 0.0)
    (render-grid-axis gl vertex v-axis v-min v-max size u-axis u-min u-max)))

(defn render-primary-axes
  [^GL2 gl ^AABB aabb]
  (gl/gl-color gl x-axis-color)
  (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0)
  (gl/gl-color gl y-axis-color)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0)
  (gl/gl-vertex-3d gl 1.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 1.0 (-> aabb types/max-p .y) 0.0)
  (gl/gl-color gl grid-color)
  (doseq [i (range 4)]
    (let [x (/ (inc ^int i) 5.0)]
      (gl/gl-vertex-3d gl x (-> aabb types/min-p .y) 0.0)
      (gl/gl-vertex-3d gl x (-> aabb types/max-p .y) 0.0))))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids]
  (doall
   (for [grid-index (range 2)
         axis [2]
         :let [ratio ^double (nth (:ratios grids) grid-index)
               alpha (Math/abs (* ^double (aget dir axis) ratio))]]
     (do
       (gl/gl-color gl (colors/alpha grid-color alpha))
       (render-grid gl axis
                    (nth (:sizes grids) grid-index)
                    (nth (:aabbs grids) grid-index))))))

(defn render-scaled-grids
  [^GL2 gl _pass renderables _count]
  (let [renderable (first renderables)
        user-render-data (:user-render-data renderable)
        camera (:camera user-render-data)
        grids (:grids user-render-data)
        view-matrix (c/camera-view-matrix camera)
        dir (double-array 4)
        _ (.getRow view-matrix 2 dir)]
    (gl/gl-lines gl
      (render-grid-sizes dir grids)
      (render-primary-axes (apply geom/aabb-union (:aabbs grids))))))

(g/defnk grid-renderable
  [camera grids]
  {pass/transparent
   [{:world-transform geom/Identity4d
     :render-fn render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids}}]})

(g/defnk update-grids
  [camera]
  (let [frustum-planes (c/viewproj-frustum-planes camera)
        aabb (grid/frustum-projection-aabb frustum-planes)
        extent (.y (geom/aabb-extent aabb))
        first-grid-ratio (grid/grid-ratio extent)
        grid-size-small (grid/small-grid-size extent)
        grid-size-large (grid/large-grid-size extent)]
    {:ratios [first-grid-ratio (- 1.0 ^double first-grid-ratio)]
     :sizes [grid-size-small grid-size-large]
     :aabbs [(grid/snap-out-to-grid aabb grid-size-small)
             (grid/snap-out-to-grid aabb grid-size-large)]}))

(g/defnode Grid
  (property size g/Any)
  (property alpha g/Num)

  (input camera Camera)

  (output grids g/Any :cached update-grids)
  (output renderable pass/RenderData :cached grid-renderable))

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

(ns editor.tile-map-grid
  (:require
   [dynamo.graph :as g]
   [editor.camera :as c]
   [editor.colors :as colors]
   [editor.geom :as geom]
   [editor.gl :as gl]
   [editor.gl.pass :as pass]
   [editor.scene :as scene]
   [editor.types :as types])
  (:import
   (editor.types AABB Camera)
   (com.jogamp.opengl GL GL2)
   (javax.vecmath Vector3d Vector4d Matrix3d Matrix4d Point3d)))

(set! *warn-on-reflection* true)

(def grid-color colors/scene-grid)
(def x-axis-color colors/scene-grid-x-axis)
(def y-axis-color colors/scene-grid-y-axis)

(defn render-xy-axes
  [^GL2 gl ^AABB aabb]
  (gl/gl-color gl x-axis-color)
  (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0)
  (gl/gl-color gl y-axis-color)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0))

(defn render-xy-grid
  [^GL2 gl aabb [w h]]
  (gl/gl-color gl grid-color)
  (let [min   (types/min-p aabb)
        max   (types/max-p aabb)
        x-min (.x min)
        x-max (.x max)
        y-min (.y min)
        y-max (.y max)]
    (doseq [x (range x-min x-max w)]
      (gl/gl-vertex-3d gl x y-min 0)
      (gl/gl-vertex-3d gl x y-max 0))
    (doseq [y (range y-min y-max h)]
      (gl/gl-vertex-3d gl x-min y 0)
      (gl/gl-vertex-3d gl x-max y 0))))

(defn render-grid
  [^GL2 gl pass renderables count]
  (let [renderable (first renderables)
        user-render-data (:user-render-data renderable)
        camera (:camera user-render-data)
        grid (:grid user-render-data)]
    (gl/gl-lines gl
                 (render-xy-grid (:aabb grid) (:grid-size grid))
                 (render-xy-axes (:aabb grid)))))

(g/defnk grid-renderable
  [camera grid]
  (when grid
    {pass/infinity-grid ; Grid lines stretching to infinity. Not depth-clipped to frustum.
     [{:world-transform geom/Identity4d
       :tags #{:grid}
       :render-fn render-grid
       :user-render-data {:camera camera
                          :grid grid}}]
     pass/transparent ; Grid lines potentially intersecting scene geometry.
     [{:world-transform geom/Identity4d
       :tags #{:grid}
       :render-fn render-grid
       :user-render-data {:camera camera
                          :grid grid}}]}))

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

(defn grid-snap-down [a sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [a sz] (* sz (Math/ceil  (/ a sz))))

(defn snap-out-to-grid
  [^AABB aabb [w h]]
  (types/->AABB (Point3d. (grid-snap-down (-> aabb types/min-p .x) w)
                          (grid-snap-down (-> aabb types/min-p .y) h)
                          0.0)
                (Point3d. (grid-snap-up   (-> aabb types/max-p .x) w)
                          (grid-snap-up   (-> aabb types/max-p .y) h)
                          0.0)))

(g/defnk update-grid
  [camera grid-size]
  (when grid-size
    (let [frustum-planes (c/viewproj-frustum-planes camera)
          far-z-plane    (nth frustum-planes 5)
          aabb           (-> (frustum-projection-aabb frustum-planes)
                             (snap-out-to-grid grid-size))]
      {:aabb      aabb
       :grid-size grid-size})))

(g/defnode TileMapGrid
  (input camera Camera)
  (input grid-size g/Any :substitute nil)

  (output grid       g/Any :cached update-grid)
  (output renderable pass/RenderData :cached grid-renderable))


(defmethod scene/attach-grid ::TileMapGrid
  [_ grid-node-id view-id resource-node camera]
  (concat
   (g/connect grid-node-id  :renderable       view-id      :aux-renderables)
   (g/connect camera        :camera           grid-node-id :camera)
   (g/connect resource-node :tile-dimensions  grid-node-id :grid-size)))

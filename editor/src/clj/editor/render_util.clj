;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.render-util
  (:require [editor.colors :as colors]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graphics.types :as graphics.types]
            [editor.math :as math]
            [editor.shaders :as shaders]
            [editor.types :as types]
            [util.array :as array]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; aabb-outline
;; -----------------------------------------------------------------------------

(def ^:private aabb-outline-shader shaders/basic-color-world-space)

(def ^:private aabb-outline-line-order
  [[5 4]
   [5 0]
   [4 7]
   [7 0]
   [6 5]
   [1 0]
   [3 4]
   [2 7]
   [6 3]
   [6 1]
   [3 2]
   [2 1]])

(def ^{:private true :tag 'long} aabb-outline-vertex-count (transduce (map count) + aabb-outline-line-order))

(defn- make-aabb-outline-vertex-buffer
  [vertex-description renderables]
  ;; Note: This is called after scene flattening, so the AABBs in the
  ;; renderables are all in world-space.
  (let [vertex-buffer-capacity (* (count renderables) aabb-outline-vertex-count)
        vertex-buffer (vtx/make-vertex-buffer vertex-description :stream vertex-buffer-capacity)
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)

        put-renderable-vertices!
        (fn put-renderable-vertices! [renderable]
          (let [aabb (:aabb renderable)
                min (types/min-p aabb)
                max (types/max-p aabb)
                x0 (float (.x min))
                y0 (float (.y min))
                z0 (float (.z min))
                x1 (float (.x max))
                y1 (float (.y max))
                z1 (float (.z max))
                color-floats (float-array (colors/renderable-outline-color renderable))
                positions [(array/of-floats x1 y1 z1)
                           (array/of-floats x1 y1 z0)
                           (array/of-floats x1 y0 z0)
                           (array/of-floats x0 y0 z0)
                           (array/of-floats x0 y0 z1)
                           (array/of-floats x0 y1 z1)
                           (array/of-floats x0 y1 z0)
                           (array/of-floats x1 y0 z1)]]
            (doseq [[^long start-index ^long end-index] aabb-outline-line-order]
              (let [^floats start-position-floats (positions start-index)
                    ^floats end-position-floats (positions end-index)]
                (when (< math/epsilon-sq (math/float-array-distance-sq start-position-floats end-position-floats))
                  (.put float-buffer start-position-floats)
                  (.put float-buffer color-floats)
                  (.put float-buffer end-position-floats)
                  (.put float-buffer color-floats))))))]

    (run! put-renderable-vertices! renderables)
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(defn- render-aabb-outline
  [^GL2 gl render-args renderables _renderable-count]
  (let [vertex-description (shaders/vertex-description aabb-outline-shader)
        vertex-buffer (make-aabb-outline-vertex-buffer vertex-description renderables)
        vertex-binding (vtx/use-with ::aabb-outline vertex-buffer aabb-outline-shader)]
    (gl/with-gl-bindings gl render-args [aabb-outline-shader vertex-binding]
      (gl/gl-draw-arrays gl GL2/GL_LINES 0 (count vertex-buffer)))))

(defn- make-aabb-outline-renderable-raw
  [tags]
  {:pre [(set? tags)
         (every? keyword? tags)]}
  {:render-fn render-aabb-outline
   :tags (coll/merge #{:outline} tags)
   :batch-key ::aabb-outline
   :select-batch-key :not-rendered
   :passes [pass/outline]})

;; SDK api
(def ^{:arglists '([tags])} make-aabb-outline-renderable
  (fn/memoize make-aabb-outline-renderable-raw))

;; -----------------------------------------------------------------------------
;; textured-quad
;; -----------------------------------------------------------------------------

(def ^:private textured-quad-shader shaders/basic-texture-paged-world-space)

(defn- make-textured-quad-vertex-buffer
  [vertex-description renderables]
  (let [vertex-buffer-capacity (* (count renderables) 6)
        vertex-buffer (vtx/make-vertex-buffer vertex-description :stream vertex-buffer-capacity)
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)

        put-renderable-vertices!
        (fn put-renderable-vertices! [renderable]
          (let [user-data (:user-data renderable)
                ^Matrix4d transform (:world-transform renderable)
                x (double (:x user-data))
                y (double (:y user-data))
                width (double (:width user-data))
                height (double (:height user-data))
                min (Point3d. x y 0.0)
                max (Point3d. (+ x width) (+ y height) 0.0)
                _ (.transform transform min)
                _ (.transform transform max)
                page-index (float (:page-index user-data))
                v0 ^floats (array/of-floats (.x min) (.y min) 0.0 0.0 0.0 page-index)
                v1 ^floats (array/of-floats (.x min) (.y max) 0.0 0.0 1.0 page-index)
                v2 ^floats (array/of-floats (.x max) (.y max) 0.0 1.0 1.0 page-index)
                v3 ^floats (array/of-floats (.x max) (.y min) 0.0 1.0 0.0 page-index)]
            (.put float-buffer v0)
            (.put float-buffer v1)
            (.put float-buffer v2)
            (.put float-buffer v2)
            (.put float-buffer v3)
            (.put float-buffer v0)))]

    (run! put-renderable-vertices! renderables)
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(defn- render-textured-quad
  [^GL2 gl render-args renderables _renderable-count]
  (let [first-renderable (first renderables)
        user-data (:user-data first-renderable)
        gpu-texture (:gpu-texture user-data @texture/white-pixel)
        vertex-description (shaders/vertex-description textured-quad-shader)
        vertex-buffer (make-textured-quad-vertex-buffer vertex-description renderables)
        vertex-binding (vtx/use-with ::textured-quad vertex-buffer textured-quad-shader)]
    (gl/with-gl-bindings gl render-args [textured-quad-shader vertex-binding gpu-texture]
      (shader/set-samplers-by-index textured-quad-shader gl 0 (:texture-units gpu-texture))
      (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (count vertex-buffer)))))

(defn- make-textured-quad-renderable
  [tags x y width height gpu-texture page-index]
  {:pre [(graphics.types/renderable-tags? tags)
         (number? x)
         (number? y)
         (number? width)
         (number? height)
         (texture/texture-lifecycle? gpu-texture)
         (nat-int? page-index)]}
  {:render-fn render-textured-quad
   :tags tags
   :batch-key gpu-texture
   :passes [pass/transparent]
   :user-data {:x x
               :y y
               :width width
               :height height
               :page-index page-index
               :gpu-texture gpu-texture}})

;; SDK api
(defn make-outlined-textured-quad-scene
  [tags transform width height gpu-texture page-index]
  {:pre [(instance? Matrix4d transform)]}
  (let [min (Point3d. 0.0 0.0 0.0)
        max (Point3d. (double width) (double height) 0.0)
        aabb (types/->AABB min max)]
    {:aabb aabb
     :transform transform
     :renderable (make-textured-quad-renderable tags 0.0 0.0 width height gpu-texture page-index)
     :children [{:aabb aabb
                 :renderable (make-aabb-outline-renderable tags)}]}))

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

(ns editor.render-util
  (:require [editor.colors :as colors]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.shaders :as shaders]
            [editor.types :as types]
            [util.array :as array]
            [util.fn :as fn])
  (:import [com.jogamp.opengl GL GL2]
           [java.nio FloatBuffer]
           [javax.vecmath Point3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private aabb-line-order
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

(def ^{:private true :tag 'long} aabb-vertex-count (transduce (map count) + aabb-line-order))

(defn- put-aabb-outline-vertices!
  [^FloatBuffer float-buffer ^Point3d min ^Point3d max ^floats color-floats]
  (let [x0 (float (.x min))
        y0 (float (.y min))
        z0 (float (.z min))
        x1 (float (.x max))
        y1 (float (.y max))
        z1 (float (.z max))
        positions [(array/of-floats x1 y1 z1)
                   (array/of-floats x1 y1 z0)
                   (array/of-floats x1 y0 z0)
                   (array/of-floats x0 y0 z0)
                   (array/of-floats x0 y0 z1)
                   (array/of-floats x0 y1 z1)
                   (array/of-floats x0 y1 z0)
                   (array/of-floats x1 y0 z1)]]
    (doseq [[^long start-index ^long end-index] aabb-line-order]
      (let [^floats start-position-floats (positions start-index)
            ^floats end-position-floats (positions end-index)]
        (when (< math/epsilon-sq (math/float-array-distance-sq start-position-floats end-position-floats))
          (.put float-buffer start-position-floats)
          (.put float-buffer color-floats)
          (.put float-buffer end-position-floats)
          (.put float-buffer color-floats))))))

(defn- make-aabb-outline-vertex-buffer
  [renderables]
  ;; The AABBs in the renderables are in world-space.
  (let [vertex-description (shaders/vertex-description shaders/basic-color-world-space)
        vertex-buffer-capacity (* (count renderables) aabb-vertex-count)
        vertex-buffer (vtx/make-vertex-buffer vertex-description :stream vertex-buffer-capacity)
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)

        put-renderable-aabb-vertices!
        (fn put-local-space-aabb-vertices! [renderable]
          (let [aabb (:aabb renderable)
                min (types/min-p aabb)
                max (types/max-p aabb)
                color-floats (float-array (colors/renderable-outline-color renderable))]
            (put-aabb-outline-vertices! float-buffer min max color-floats)))]

    (run! put-renderable-aabb-vertices! renderables)
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(defn- render-world-space-aabb-outline
  [^GL2 gl render-args renderables _renderable-count]
  (let [vertex-buffer (make-aabb-outline-vertex-buffer renderables)
        outline-vertex-binding (vtx/use-with ::aabb-outline vertex-buffer shaders/basic-color-world-space)]
    (gl/with-gl-bindings gl render-args [shaders/basic-color-world-space outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vertex-buffer)))))

(defn- make-aabb-outline-renderable-raw
  [tag]
  {:pre [(keyword? tag)]}
  {:render-fn render-world-space-aabb-outline
   :tags (conj #{:outline} tag)
   :batch-key ::aabb-outline
   :select-batch-key :not-rendered
   :passes [pass/outline]})

(def make-aabb-outline-renderable (fn/memoize make-aabb-outline-renderable-raw))

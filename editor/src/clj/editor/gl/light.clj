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

(ns editor.gl.light
  (:require [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.math :as math]
            [editor.scene-cache :as scene-cache]
            [editor.types :as types])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:const default-max-preview-lights 8)

(defn- engine-light-type-index
  ^double [light-type-kw]
  (case light-type-kw
    :directional 0.0
    :point 1.0
    :spot 2.0
    0.0))

(defn- world-space-light-direction
  ^Vector3d [^Matrix4d world-transform]
  ;; Extract only rotation from the world transform so that scale
  ;; (including negative scale) does not affect the direction.
  ;; For direction (0,0,-1), the result is the negated third column of
  ;; the upper-left 3x3, normalized to remove scale.
  (doto (Vector3d. (- (.m02 world-transform))
                   (- (.m12 world-transform))
                   (- (.m22 world-transform)))
    (.normalize)))

(defn- preview-renderable-min-scale
  ^double [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (Math/abs (.x ws))
         (Math/abs (.y ws))
         (Math/abs (.z ws)))
    1.0))

(defn renderable->std140-light [renderable]
  (let [light-data (get-in renderable [:user-data :editor-preview-light])
        ^Vector3d translation (:world-translation renderable math/zero-v3)
        ^Matrix4d transform (:world-transform renderable math/identity-mat4)
        light-type (:light-type light-data)
        light-color (:color light-data)
        light-intensity (:intensity light-data)
        light-range (double (:range light-data))
        scaled-range (max 0.01 (* light-range (preview-renderable-min-scale renderable)))
        translation-v4 (Vector4d. (.x translation) (.y translation) (.z translation) 1.0)
        color-v4 (doto (Vector4d.) (math/clj->vecmath light-color))
        type-index (engine-light-type-index light-type)]
    (case light-type
      :directional
      (let [d (world-space-light-direction transform)]
        {:position translation-v4
         :color color-v4
         :direction-range (Vector4d. (.x d) (.y d) (.z d) 0.0)
         :params (Vector4d. type-index light-intensity 0.0 0.0)})

      :point
      {:position translation-v4
       :color color-v4
       :direction-range (Vector4d. 0.0 0.0 0.0 scaled-range)
       :params (Vector4d. type-index light-intensity 0.0 0.0)}

      :spot
      (let [d (world-space-light-direction transform)
            inner-deg (:inner-cone-angle light-data)
            outer-deg (:outer-cone-angle light-data)
            inner-rad (Math/toRadians inner-deg)
            outer-rad (Math/toRadians outer-deg)]
        {:position translation-v4
         :color color-v4
         :direction-range (Vector4d. (.x d) (.y d) (.z d) scaled-range)
         :params (Vector4d. type-index light-intensity inner-rad outer-rad)})

      ;; default
      {:position translation-v4
       :color color-v4
       :direction-range (Vector4d. 0.0 0.0 0.0 0.0)
       :params (Vector4d. type-index light-intensity 0.0 0.0)})))

(defn- light-camera-distance-squared
  ^double [camera renderable]
  (let [^Point3d camera-position (types/position camera)
        ^Vector3d world-translation (:world-translation renderable math/zero-v3)
        dx (- (.x camera-position) (.x world-translation))
        dy (- (.y camera-position) (.y world-translation))
        dz (- (.z camera-position) (.z world-translation))]
    (+ (* dx dx) (* dy dy) (* dz dz))))

(defn- directional-preview-light? [renderable]
  (= :directional (get-in renderable [:user-data :editor-preview-light :light-type])))

(defn packed-lights-from-scene
  "Turns the editor scene's light renderables into a small, stable, engine-shaped list so
  the viewport shaders can produce light data consistently with the runtime layout."
  [renderables-by-pass camera]
  (let [visible-with-preview (filterv #(get-in % [:user-data :editor-preview-light])
                                      (get renderables-by-pass pass/transparent []))
        deduped-visible-preview (vals (reduce (fn [by-node-id-path renderable]
                                                (update by-node-id-path (:node-id-path renderable) #(or % renderable)))
                                              {}
                                              visible-with-preview))
        sorted-preview-lights (sort-by (comp vec :node-id-path) deduped-visible-preview)
        directional-lights (filter directional-preview-light? sorted-preview-lights)
        local-lights (remove directional-preview-light? sorted-preview-lights)
        directional-lights (take default-max-preview-lights directional-lights)
        local-light-budget (- default-max-preview-lights (count directional-lights))]
    (mapv renderable->std140-light
          (concat directional-lights
                  (take local-light-budget
                        (sort-by (juxt #(light-camera-distance-squared camera %)
                                       (comp vec :node-id-path))
                                 local-lights))))))

(defn- gl-light-uniform-name [^long i field]
  (str "lights[" i "]." (case field
                          :position "position"
                          :color "color"
                          :direction-range "direction_range"
                          :params "params")))

(def ^:private lights-count-uniform-name "lights_count")

(defn bind-preview-lights-for-shader! [^GL2 gl shader-lifecycle render-args]
  (when (shader/uses-preview-light-buffer? shader-lifecycle)
    (let [packed-lights (or (:editor/preview-lights render-args) [])
          shader-light-capacity (shader/preview-light-capacity shader-lifecycle)]
      (when-let [{:keys [^int program uniform-infos]}
                 (scene-cache/request-object! ::shader/shader
                                              (:request-id shader-lifecycle)
                                              gl
                                              (:request-data shader-lifecycle))]
        (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
          (let [lights (take shader-light-capacity packed-lights)
                light-count (count lights)
                count-v4 (Vector4d. (double light-count) 0.0 0.0 0.0)]
            (when-some [uniform-info (uniform-infos lights-count-uniform-name)]
              (shader/set-uniform-at-index gl program (:location uniform-info) count-v4))

            (when (pos? light-count)
              (doseq [^long i (range light-count)
                      :let [light (nth lights i)]
                      field [:position :color :direction-range :params]
                      :let [uniform-name (gl-light-uniform-name i field)
                            uniform-value (field light)
                            uniform-info (uniform-infos uniform-name)]
                      :when uniform-info]
                (shader/set-uniform-at-index gl program (:location uniform-info) uniform-value)))))))))

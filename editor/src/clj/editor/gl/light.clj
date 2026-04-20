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
            [editor.scene-cache :as scene-cache])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:const default-max-preview-lights 8)

(defn- engine-light-type-index ^double [light-type-kw]
  (case light-type-kw
    :directional 0.0
    :point 1.0
    :spot 2.0
    0.0))

(defn- preview-normalize-dir! ^Vector3d [^Vector3d v]
  (let [len (Math/sqrt (+ (* (.x v) (.x v)) (* (.y v) (.y v)) (* (.z v) (.z v))))]
    (if (> len 1e-10)
      (doto v (.scale (/ 1.0 len)))
      (doto v (.set 0.0 0.0 -1.0)))))

(defn- world-space-light-direction [^Matrix4d world-transform]
  (preview-normalize-dir! (math/transform-vector world-transform (Vector3d. 0.0 0.0 -1.0))))

(defn- preview-renderable-min-scale ^double [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (.x ws) (.y ws) (.z ws))
    1.0))

(defn renderable->std140-light [renderable]
  (let [light-data (get-in renderable [:user-data :editor-preview-light])
        ^Vector3d translation (or (:world-translation renderable)
                                     (Vector3d. 0.0 0.0 0.0))
        ^Matrix4d transform (or (:world-transform renderable)
                                      (doto (Matrix4d.) (.setIdentity)))
        light-type (:light-type light-data)
        light-color (:color light-data)
        light-intensity (or (:intensity light-data) 1.0)
        light-range (double (or (:range light-data) 10.0))
        scaled-range (max 0.01 (* light-range (preview-renderable-min-scale renderable)))
        translation-v4 (Vector4d. (.x translation) (.y translation) (.z translation) 1.0)
        color-v4 (Vector4d. (nth light-color  0) (nth light-color  1) (nth light-color  2) (nth light-color  3))
        type-index (engine-light-type-index light-type)]
    (case light-type
      :directional
      (let [^Vector3d d (world-space-light-direction transform)]
        {:position translation-v4
         :color color-v4
         :direction_range (Vector4d. (.x d) (.y d) (.z d) 0.0)
         :params (Vector4d. type-index light-intensity 0.0 0.0)})
      :point
      {:position translation-v4
       :color color-v4
       :direction_range (Vector4d. 0.0 0.0 0.0 scaled-range)
       :params (Vector4d. type-index light-intensity 0.0 0.0)}
      :spot
      (let [^Vector3d d (world-space-light-direction transform)
            inner-deg (or (:inner-cone-angle light-data) 0.0)
            outer-deg (or (:outer-cone-angle light-data) 45.0)
            inner-rad (Math/toRadians inner-deg)
            outer-rad (Math/toRadians outer-deg)]
        {:position translation-v4
         :color color-v4
         :direction_range (Vector4d. (.x d) (.y d) (.z d) scaled-range)
         :params (Vector4d. type-index light-intensity inner-rad outer-rad)})
      {:position translation-v4
       :color color-v4
       :direction_range (Vector4d. 0.0 0.0 0.0 0.0)
       :params (Vector4d. type-index light-intensity 0.0 0.0)})))

(defn packed-lights-from-scene [renderables-by-pass]
  "Turns the editor scene's light renderables into a small, stable, engine-shaped list so
  the viewport shaders can produce light data consistently with the runtime layout."
  (let [visible-with-preview (filterv #(get-in % [:user-data :editor-preview-light])
                                      (get renderables-by-pass pass/transparent []))
        deduped-visible-preview (vals (reduce (fn [by-node-id-path renderable]
                                                (update by-node-id-path (:node-id-path renderable) #(or % renderable)))
                                              {}
                                              visible-with-preview))]
    (mapv renderable->std140-light
          (take default-max-preview-lights
                (sort-by (comp vec :node-id-path) deduped-visible-preview)))))

(defn- gl-light-uniform-name [^long i field]
  (str "lights[" i "]." (case field
                          :position "position"
                          :color "color"
                          :direction_range "direction_range"
                          :params "params")))

(defn bind-preview-lights-for-shader! [^GL2 gl shader-lifecycle render-args]
  (let [packed-lights (or (:editor/preview-lights render-args) [])
        lights (take default-max-preview-lights packed-lights)
        light-count (long (count lights))
        count-v4 (Vector4d. (double light-count) 0.0 0.0 0.0)
        pairs (into [["lights_count" count-v4]]
                    (mapcat
                      (fn [^long i]
                        (let [light (nth lights i)]
                          (map (fn [field]
                                 [(gl-light-uniform-name i field) (field light)])
                               [:position :color :direction_range :params])))
                      (range light-count)))]
    (when-let [{:keys [^int program uniform-infos]}
               (scene-cache/request-object! ::shader/shader
                                            (:request-id shader-lifecycle)
                                            gl
                                            (:request-data shader-lifecycle))]
      (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
        (doseq [[name val] pairs
                :when (and (string? (not-empty name)) (some? val) (uniform-infos name))]
          (let [uniform-info (uniform-infos name)]
            (try
              (shader/set-uniform-at-index gl program (:location uniform-info) val)
              (catch IllegalArgumentException e
                (throw (IllegalArgumentException. (format "Failed setting uniform '%s'." name) e))))))))))

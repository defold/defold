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
           [editor.gl.shader ShaderLifecycle]
           [javax.vecmath Matrix4d Vector3d Vector4d]))

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

(defn- preview-normalize-dir!
  ^Vector3d [^Vector3d v]
  (let [len (Math/sqrt (+ (* (.x v) (.x v)) (* (.y v) (.y v)) (* (.z v) (.z v))))]
    (if (> len 1e-10)
      (doto v (.scale (/ 1.0 len)))
      (doto v (.set 0.0 0.0 -1.0)))))

(defn- world-space-light-direction
  ^Vector3d [^Matrix4d world-transform]
  (let [d (Vector3d. 0.0 0.0 -1.0)]
    (preview-normalize-dir! (math/transform-vector world-transform d))))

(defn- preview-renderable-min-scale
  ^double [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (.x ws) (.y ws) (.z ws))
    1.0))

(defn renderable->std140-light
  [renderable]
  (let [prev (get-in renderable [:user-data :editor-preview-light])
        ^Vector3d p (or (:world-translation renderable) (Vector3d. 0.0 0.0 0.0))
        ^Matrix4d wt (or (:world-transform renderable)
                         (doto (Matrix4d.) (.setIdentity)))
        lt (:light-type prev)
        c (:color prev)
        intensity (double (or (:intensity prev) 1.0))
        range (double (or (:range prev) 10.0))
        scaled-range (max 0.01 (* range (double (preview-renderable-min-scale renderable))))
        inner-deg (double (or (:inner-cone-angle prev) 0.0))
        outer-deg (double (or (:outer-cone-angle prev) 45.0))
        inner-rad (Math/toRadians inner-deg)
        outer-rad (Math/toRadians outer-deg)
        r (double (nth c 0 1.0))
        g (double (nth c 1 1.0))
        b (double (nth c 2 1.0))
        a (double (nth c 3 1.0))
        pos (Vector4d. (.x p) (.y p) (.z p) 1.0)
        col (Vector4d. r g b a)
        tidx (engine-light-type-index lt)]
    (case lt
      :directional
      (let [^Vector3d d (world-space-light-direction wt)]
        {:position pos
         :color col
         :direction_range (Vector4d. (.x d) (.y d) (.z d) 0.0)
         :params (Vector4d. tidx intensity 0.0 0.0)})
      :point
      {:position pos
       :color col
       :direction_range (Vector4d. 0.0 0.0 0.0 scaled-range)
       :params (Vector4d. tidx intensity 0.0 0.0)}
      :spot
      (let [^Vector3d d (world-space-light-direction wt)]
        {:position pos
         :color col
         :direction_range (Vector4d. (.x d) (.y d) (.z d) scaled-range)
         :params (Vector4d. tidx intensity inner-rad outer-rad)})
      {:position pos
       :color col
       :direction_range (Vector4d. 0.0 0.0 0.0 0.0)
       :params (Vector4d. tidx intensity 0.0 0.0)})))

;; Turns the editor scene's light renderables into a small, stable, engine-shaped list so
;; the viewport shaders can produce light data consistently with the runtime layout.
(defn packed-lights-from-scene
  [renderables-by-pass]
  (let [visible-with-preview (filterv #(get-in % [:user-data :editor-preview-light])
                                      (get renderables-by-pass pass/transparent []))
        deduped-visible-preview (vals (reduce (fn [by-node-id-path renderable]
                                                (update by-node-id-path (:node-id-path renderable) #(or % renderable)))
                                              {}
                                              visible-with-preview))]
    (if (seq deduped-visible-preview)
      (mapv renderable->std140-light
            (take default-max-preview-lights
                  (sort-by (comp vec :node-id-path) deduped-visible-preview)))
      [])))

(defn- light-uniform-name [^long i field]
  (str "lights[" i "]." (case field
                          :position "position"
                          :color "color"
                          :direction_range "direction_range"
                          :params "params")))

(defn bind-engine-style-lights!
  [^GL2 gl ^ShaderLifecycle shader-lifecycle packed-lights]
  (let [lights (vec (take default-max-preview-lights packed-lights))
        n (long (count lights))
        count-v4 (Vector4d. (double n) 0.0 0.0 0.0)
        pairs (into [["lights_count" count-v4]]
                    (mapcat
                      (fn [^long i]
                        (let [light (nth lights i)]
                          (map (fn [field]
                                 [(light-uniform-name i field) (field light)])
                               [:position :color :direction_range :params])))
                      (range n)))]
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

(defn bind-preview-lights-for-shader!
  [^GL2 gl shader-lifecycle render-args]
  (bind-engine-style-lights! gl shader-lifecycle (or (:editor/preview-lights render-args) [])))

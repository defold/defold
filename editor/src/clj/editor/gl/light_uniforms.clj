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

(ns editor.gl.light-uniforms
  "Packing and upload of engine-compatible light uniforms for editor material preview.

  Layout matches `LightSTD140` in `engine/render/src/render/render_private.h`:
  `position`, `color`, `direction_range`, `params` as four `vec4` per light, plus
  `lights_count` as a `vec4` with active count in `.x` (see `render/light.cpp` ES2 path)."
  (:require [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.math :as math])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Vector3d Vector4d]))

(def ^:const default-max-preview-lights 8)

(defn- vec4d
  ^Vector4d [^double x ^double y ^double z ^double w]
  (Vector4d. x y z w))

(defn- engine-light-type-index
  ^double [light-type-kw]
  (case light-type-kw
    :directional 0.0
    :point 1.0
    :spot 2.0
    0.0))

(defn- normalize-dir!
  ^Vector3d [^Vector3d v]
  (let [len (Math/sqrt (+ (* (.x v) (.x v)) (* (.y v) (.y v)) (* (.z v) (.z v))))]
    (if (> len 1e-10)
      (doto v (.scale (/ 1.0 len)))
      (doto v (.set 0.0 0.0 -1.0)))))

(defn- world-space-light-direction
  ^Vector3d [^Matrix4d world-transform]
  (let [d (Vector3d. 0.0 0.0 -1.0)]
    (normalize-dir! (math/transform-vector world-transform d))))

(defn- renderable-min-scale
  ^double [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (.x ws) (.y ws) (.z ws))
    1.0))

(defn renderable->std140-light
  "Build one engine `LightSTD140` row from a flattened scene renderable that has
  `:editor-preview-light` in `:user-data` (see `editor.light/produce-light-scene`)."
  [renderable]
  (let [prev (get-in renderable [:user-data :editor-preview-light])
        ^Vector3d p (or (:world-translation renderable) (Vector3d. 0.0 0.0 0.0))
        ^Matrix4d wt (or (:world-transform renderable)
                         (doto (Matrix4d.) (.setIdentity)))
        lt (:light-type prev)
        c (:color prev)
        intensity (double (or (:intensity prev) 1.0))
        range (double (or (:range prev) 10.0))
        scaled-range (max 0.01 (* range (double (renderable-min-scale renderable))))
        inner-deg (double (or (:inner-cone-angle prev) 0.0))
        outer-deg (double (or (:outer-cone-angle prev) 45.0))
        inner-rad (Math/toRadians inner-deg)
        outer-rad (Math/toRadians outer-deg)
        r (double (nth c 0 1.0))
        g (double (nth c 1 1.0))
        b (double (nth c 2 1.0))
        a (double (nth c 3 1.0))
        pos (vec4d (.x p) (.y p) (.z p) 1.0)
        col (vec4d r g b a)
        tidx (engine-light-type-index lt)]
    (case lt
      :directional
      (let [^Vector3d d (world-space-light-direction wt)]
        {:position pos
         :color col
         :direction_range (vec4d (.x d) (.y d) (.z d) 0.0)
         :params (vec4d tidx intensity 0.0 0.0)})

      :point
      {:position pos
       :color col
       :direction_range (vec4d 0.0 0.0 0.0 scaled-range)
       :params (vec4d tidx intensity 0.0 0.0)}

      :spot
      (let [^Vector3d d (world-space-light-direction wt)]
        {:position pos
         :color col
         :direction_range (vec4d (.x d) (.y d) (.z d) scaled-range)
         :params (vec4d tidx intensity inner-rad outer-rad)})

      ;; Unknown
      {:position pos
       :color col
       :direction_range (vec4d 0.0 0.0 0.0 0.0)
       :params (vec4d tidx intensity 0.0 0.0)})))

(defn packed-lights-from-scene
  "Collect lights from scene renderables. Returns an empty vector when there are none."
  [renderables-by-pass]
  (let [;; Only transparent pass renderables are actually visible in scene view.
        ;; Hidden lights can still appear in outline/selection for selection UX.
        visible-with-preview (filterv #(get-in % [:user-data :editor-preview-light])
                                      (get renderables-by-pass pass/transparent []))
        deduped-visible-preview (vals (reduce (fn [by-node-id-path renderable]
                                                (update by-node-id-path (:node-id-path renderable) #(or % renderable)))
                                              {}
                                              visible-with-preview))]
    (if (seq deduped-visible-preview)
      (mapv renderable->std140-light
            (take default-max-preview-lights
                  (sort-by (comp vec :node-id-path) deduped-visible-preview)))
      ;; No visible lights -> empty light buffer.
      [])))

(defn pack-lights
  "Returns a vector of light maps in engine order. Each map has keys
  `:position`, `:color`, `:direction_range`, `:params` with `Vector4d` values."
  [lights]
  (vec lights))

(defn lights-std140->float-array
  "Contiguous `float-array` of length `(* 16 (count lights))`: for each light, four
  `vec4` in order position, color, direction_range, params (row-major components)."
  [packed-lights]
  (let [packed-lights (vec (take default-max-preview-lights packed-lights))
        n (count packed-lights)
        ^floats a (float-array (* 16 n))]
    (dotimes [li n]
      (let [light (nth packed-lights li)
            o (* li 16)
            ^Vector4d p (:position light)
            ^Vector4d c (:color light)
            ^Vector4d d (:direction_range light)
            ^Vector4d pr (:params light)]
        (aset a (+ o 0) (float (.x p)))
        (aset a (+ o 1) (float (.y p)))
        (aset a (+ o 2) (float (.z p)))
        (aset a (+ o 3) (float (.w p)))
        (aset a (+ o 4) (float (.x c)))
        (aset a (+ o 5) (float (.y c)))
        (aset a (+ o 6) (float (.z c)))
        (aset a (+ o 7) (float (.w c)))
        (aset a (+ o 8) (float (.x d)))
        (aset a (+ o 9) (float (.y d)))
        (aset a (+ o 10) (float (.z d)))
        (aset a (+ o 11) (float (.w d)))
        (aset a (+ o 12) (float (.x pr)))
        (aset a (+ o 13) (float (.y pr)))
        (aset a (+ o 14) (float (.z pr)))
        (aset a (+ o 15) (float (.w pr)))))
    a))

(defn- light-uniform-name
  [^long i field]
  (str "lights[" i "]." (case field
                         :position "position"
                         :color "color"
                         :direction_range "direction_range"
                         :params "params")))

(defn bind-engine-style-lights!
  "Upload `lights_count` and `lights[i].{position,color,direction_range,params}` for
  each active light when those uniforms exist (after UBO namespace strip for
  `lights_count`). Uses one scene-cache lookup via `shader/set-uniforms-for-current-program!`."
  [^GL2 gl shader-lifecycle packed-lights]
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
    (shader/set-uniforms-for-current-program! gl shader-lifecycle pairs)))

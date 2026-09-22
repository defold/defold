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
            [editor.gl.shader :as shader]
            [editor.math :as math]
            [editor.scene-cache :as scene-cache]
            [editor.types :as types]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:const default-max-preview-lights 8)

(def default-preview-ambient-light
  (Vector3d. 0.2 0.2 0.2))

(def default-preview-directional-light-entry
  {:node-id-path [::default-preview-directional-light]
   :world-translation math/zero-v3
   :light-type :directional
   :packed-light {:position (Vector4d. 0.0 0.0 0.0 1.0)
                  :color (Vector4d. 1.0 1.0 1.0 1.0)
                  :direction-range (Vector4d. 0.0 -1.0 0.0 0.0)
                  :params (Vector4d. 0.0 1.0 0.0 0.0)}})

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

(defn- color->vector4d
  ^Vector4d [color]
  (Vector4d. (double (nth color 0 1.0))
             (double (nth color 1 1.0))
             (double (nth color 2 1.0))
             (double (nth color 3 1.0))))

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
        color-v4 (color->vector4d light-color)
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

(defn- preview-light-renderable? [renderable]
  (some? (get-in renderable [:user-data :editor-preview-light])))

(defn- renderable-node-id-path-comparator [a b]
  (compare (vec (:node-id-path a))
           (vec (:node-id-path b))))

(defn- renderable->preview-light-entry [renderable]
  {:node-id-path (vec (:node-id-path renderable))
   :world-translation (:world-translation renderable math/zero-v3)
   :light-data (get-in renderable [:user-data :editor-preview-light])
   :light-type (get-in renderable [:user-data :editor-preview-light :light-type])
   :packed-light (renderable->std140-light renderable)})

(defn- directional-preview-light-entry? [preview-light-entry]
  (= :directional (:light-type preview-light-entry)))

(defn- ambient-preview-light-entry? [preview-light-entry]
  (= :ambient (:light-type preview-light-entry)))

(defn- preview-light-entry->ambient-light
  [preview-light-entry]
  (let [light-data (:light-data preview-light-entry)
        light-color (:color light-data)
        light-intensity (double (:intensity light-data))]
    (Vector3d. (* (double (nth light-color 0 1.0)) light-intensity)
               (* (double (nth light-color 1 1.0)) light-intensity)
               (* (double (nth light-color 2 1.0)) light-intensity))))

(defn preview-light-data-from-renderables
  "Prepares the camera-independent parts of scene preview light packing."
  [renderables]
  (let [preview-renderables (filterv preview-light-renderable? renderables)]
    (if (coll/empty? preview-renderables)
      {:ambient-light math/zero-v3
       :directional-light-entries []
       :local-light-entries []
       :local-light-budget default-max-preview-lights}
      (let [deduped-preview-renderables
            (into (sorted-set-by renderable-node-id-path-comparator)
                  preview-renderables)

            preview-light-entries
            (mapv renderable->preview-light-entry deduped-preview-renderables)

            [ambient-light-entries non-ambient-light-entries]
            (coll/separate-by ambient-preview-light-entry? preview-light-entries)

            ambient-light
            (transduce (map preview-light-entry->ambient-light)
                       (completing math/add-vector)
                       math/zero-v3
                       ambient-light-entries)

            [directional-light-entries local-light-entries]
            (coll/separate-by directional-preview-light-entry? non-ambient-light-entries)

            directional-light-entries
            (subvec directional-light-entries
                    0
                    (min (count directional-light-entries)
                         default-max-preview-lights))

            local-light-budget (- default-max-preview-lights (count directional-light-entries))]
        {:ambient-light ambient-light
         :directional-light-entries directional-light-entries
         :local-light-entries local-light-entries
         :local-light-budget local-light-budget}))))

(defn with-default-preview-lights [preview-light-data]
  (assoc preview-light-data
    :ambient-light default-preview-ambient-light
    :directional-light-entries [default-preview-directional-light-entry]
    :local-light-budget (dec default-max-preview-lights)))

(defn- preview-light-entry-distance-squared
  ^double [preview-light-entry ^Point3d position]
  (let [^Vector3d world-translation (:world-translation preview-light-entry)
        dx (- (.x position) (.x world-translation))
        dy (- (.y position) (.y world-translation))
        dz (- (.z position) (.z world-translation))]
    (+ (* dx dx) (* dy dy) (* dz dz))))

(defn packed-lights-from-preview-light-data
  "Turns prepared scene preview light data into the camera-specific std140 light list."
  [preview-light-data camera]
  (let [{:keys [directional-light-entries local-light-entries local-light-budget]} preview-light-data
        camera-position (types/position camera)]
    (into (mapv :packed-light directional-light-entries)
          (comp (take local-light-budget)
                (map :packed-light))
          (sort-by #(preview-light-entry-distance-squared % camera-position)
                   local-light-entries))))

(defn- gl-light-uniform-name [^long i field]
  (str "lights[" i "]." (case field
                          :position "position"
                          :color "color"
                          :direction-range "direction_range"
                          :params "params")))

(def ^:private light-info-uniform-name "light_info")

(defn bind-preview-lights-for-shader! [^GL2 gl shader-lifecycle render-args]
  (when (shader/uses-preview-light-buffer? shader-lifecycle)
    (let [packed-lights (or (:preview-lights render-args) [])
          ^Vector3d ambient-light (or (:preview-ambient-light render-args) math/zero-v3)
          shader-light-capacity (shader/preview-light-capacity shader-lifecycle)]
      (when-let [{:keys [^int program uniform-infos]}
                 (scene-cache/request-object! ::shader/shader
                                              (:request-id shader-lifecycle)
                                              gl
                                              (:request-data shader-lifecycle))]
        (when (and (not (zero? program)) (= program (gl/gl-current-program gl)))
          (let [lights (take shader-light-capacity packed-lights)
                light-count (count lights)
                light-info (Vector4d. (.x ambient-light) (.y ambient-light) (.z ambient-light) (double light-count))]
            (when-some [uniform-info (uniform-infos light-info-uniform-name)]
              (shader/set-uniform-at-index gl program (:location uniform-info) light-info))

            (when (pos? light-count)
              (doseq [^long i (range light-count)
                      :let [light (nth lights i)]
                      field [:position :color :direction-range :params]
                      :let [uniform-name (gl-light-uniform-name i field)
                            uniform-value (field light)
                            uniform-info (uniform-infos uniform-name)]
                      :when uniform-info]
                (shader/set-uniform-at-index gl program (:location uniform-info) uniform-value)))))))))

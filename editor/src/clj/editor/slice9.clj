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

(ns editor.slice9
  (:require [editor.geom :as geom]
            [util.coll :as coll])
  (:import [com.defold.engine Geometry Geometry$Slice9Params GeometryJni]
           [com.defold.libs ResourceUnpacker]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private indices
  (delay
    (ResourceUnpacker/unpackResources)
    (vec (GeometryJni/GetSlice9Indices))))

(defn sliced? [slice9]
  (coll/any? pos? slice9))

(defn vertex-data
  [{:keys [texture-width texture-height tex-coords-raw flip-horizontal flip-vertical]
    image-pivot :pivot}
   [^double box-width ^double box-height] slice9 pivot]
  (let [indices @indices
        params (Geometry$Slice9Params.)]
    (set! (.width params) (float box-width))
    (set! (.height params) (float box-height))
    (set! (.slice9 params) (float-array slice9))
    (set! (.pivotX params) (float (+ (case (geom/gui-pivot->h-align pivot)
                                       :left 0.0
                                       :center 0.5
                                       :right 1.0)
                                     (double (get image-pivot 0 0.0)))))
    (set! (.pivotY params) (float (+ (case (geom/gui-pivot->v-align pivot)
                                       :bottom 0.0
                                       :middle 0.5
                                       :top 1.0)
                                     (double (get image-pivot 1 0.0)))))
    (set! (.flipHorizontal params) (byte (if flip-horizontal 1 0)))
    (set! (.flipVertical params) (byte (if flip-vertical 1 0)))

    (let [xy (GeometryJni/GenerateSlice9Positions params)
          uv (GeometryJni/GenerateSlice9TexCoords params
                                                  (float texture-width)
                                                  (float texture-height)
                                                  (float-array (coll/into-> tex-coords-raw [] cat)))

          positions (mapv (fn [^long vertex]
                            (let [offset (* vertex 2)]
                              (vector-of :double
                                (float (* (.width params) (aget xy offset)))
                                (float (* (.height params) (aget xy (inc offset))))
                                0.0)))
                          (range Geometry/SLICE9_VERTEX_COUNT))

          uvs (mapv (fn [^long vertex]
                      (let [offset (* vertex 2)]
                        (vector-of :double (aget uv offset) (aget uv (inc offset)))))
                    (range Geometry/SLICE9_VERTEX_COUNT))]
      {:position-data (mapv positions indices)
       :uv-data (mapv uvs indices)
       ;; Trace each cell's perimeter using its native triangle indices.
       :line-data (coll/into-> (range 9) []
                    (mapcat (fn [^long cell]
                              (let [offset (* cell 6)]
                                (mapv (fn [^long corner]
                                        (positions (indices (+ offset corner))))
                                      [0 1 1 2 2 5 5 0])))))})))

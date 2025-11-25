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

(ns editor.graphics-test
  (:require [clojure.test :refer :all]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [util.coll :refer [pair]]))

(set! *warn-on-reflection* true)

(defn- semantic-type->label
  ^String [semantic-type]
  (subs (name semantic-type)
        (count "semantic-type-")))

(defn- vector-type->label
  ^String [vector-type]
  (if vector-type
    (subs (name vector-type)
          (count "vector-type-"))
    "no-values"))

(def ^:private convert-double-values-rule-declaration
  ;; This map describes the expected behavior of the convert-double-values
  ;; function under various semantic-types.
  ;;
  ;; Format: semantic-type->old-vector-type->new-vector-type->expected-result.
  ;;
  ;; The nil entry for the old-vector-type details the expected result when
  ;; resizing an empty vector to a specified new-vector-type, i.e. constructing
  ;; a vector-type instance from nothing.
  ;;
  ;; We will look up the old-vector-type in the new-vector-type->expected-result
  ;; map for each semantic-type, invoke graphics/convert-double-values, and
  ;; verify that the result matches the value for the new-vector-type in the
  ;; new-vector-type->expected-result map. If a particular semantic-type does
  ;; not declare a behavior for a particular new-vector-type, we will expect the
  ;; behavior to match :semantic-type-none.
  ;;
  ;; Formally, the conversion rules are:
  ;;
  ;; * Converting from a scalar or vector to a smaller scalar or vector will
  ;;   truncate values from the end of the source value.
  ;; * Converting from a scalar or vector to a larger vector will zero-fill at
  ;;   the end, unless the semantic-type is position, color or tangent, in which
  ;;   case the W component will be one.
  ;; * Converting from a scalar or vector to a matrix will zero-fill at the end.
  ;; * Converting from a matrix to a smaller matrix will use the top-left
  ;;   portion of the source matrix.
  ;; * Converting from a matrix to a larger matrix will write into the top-left
  ;;   portion of the destination matrix, and fill the remaining space with the
  ;;   identity matrix.
  ;; * Converting from a matrix to a vector will truncate values from the end of
  ;;   the source matrix.
  ;;
  ;; In addition to conversions, we have a unique set of rules for the case
  ;; where we construct a default value from no value at all:
  ;;
  ;; * The default for a scalar or vector is a double vector of the appropriate
  ;;   length. The contents are typically zeroes, but there are some special
  ;;   cases based on the semantic-type. Colors will be initialized to opaque
  ;;   white, and positions and tangents will have a one in the W component.
  ;; * The default for a matrix is the identity matrix.
  {:semantic-type-none
   {nil
    {:vector-type-scalar [0.0]
     :vector-type-vec2 [0.0 0.0]
     :vector-type-vec3 [0.0 0.0 0.0]
     :vector-type-vec4 [0.0 0.0 0.0 0.0]
     :vector-type-mat2 [1.0 0.0
                        0.0 1.0]
     :vector-type-mat3 [1.0 0.0 0.0
                        0.0 1.0 0.0
                        0.0 0.0 1.0]
     :vector-type-mat4 [1.0 0.0 0.0 0.0
                        0.0 1.0 0.0 0.0
                        0.0 0.0 1.0 0.0
                        0.0 0.0 0.0 1.0]}

    :vector-type-scalar
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.1]
     :vector-type-vec3 [1.1 1.1 1.1]
     :vector-type-vec4 [1.1 1.1 1.1 1.1]
     :vector-type-mat2 [1.1 0.0
                        0.0 1.1]
     :vector-type-mat3 [1.1 0.0 0.0
                        0.0 1.1 0.0
                        0.0 0.0 1.1]
     :vector-type-mat4 [1.1 0.0 0.0 0.0
                        0.0 1.1 0.0 0.0
                        0.0 0.0 1.1 0.0
                        0.0 0.0 0.0 1.1]}

    :vector-type-vec2
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 0.0]
     :vector-type-vec4 [1.1 1.2 0.0 0.0]
     :vector-type-mat2 [1.1 1.2
                        0.0 0.0]
     :vector-type-mat3 [1.1 1.2 0.0
                        0.0 0.0 0.0
                        0.0 0.0 0.0]
     :vector-type-mat4 [1.1 1.2 0.0 0.0
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0]}

    :vector-type-vec3
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 1.3]
     :vector-type-vec4 [1.1 1.2 1.3 0.0]
     :vector-type-mat2 [1.1 1.2
                        1.3 0.0]
     :vector-type-mat3 [1.1 1.2 1.3
                        0.0 0.0 0.0
                        0.0 0.0 0.0]
     :vector-type-mat4 [1.1 1.2 1.3 0.0
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0]}

    :vector-type-vec4
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 1.3]
     :vector-type-vec4 [1.1 1.2 1.3 4.4]
     :vector-type-mat2 [1.1 1.2
                        1.3 4.4]
     :vector-type-mat3 [1.1 1.2 1.3
                        4.4 0.0 0.0
                        0.0 0.0 0.0]
     :vector-type-mat4 [1.1 1.2 1.3 4.4
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0
                        0.0 0.0 0.0 0.0]}

    :vector-type-mat2
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 1.3]
     :vector-type-vec4 [1.1 1.2 1.3 1.4]
     :vector-type-mat2 [1.1 1.2
                        1.3 1.4]
     :vector-type-mat3 [1.1 1.2 0.0
                        1.3 1.4 0.0
                        0.0 0.0 1.0]
     :vector-type-mat4 [1.1 1.2 0.0 0.0
                        1.3 1.4 0.0 0.0
                        0.0 0.0 1.0 0.0
                        0.0 0.0 0.0 1.0]}

    :vector-type-mat3
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 1.3]
     :vector-type-vec4 [1.1 1.2 1.3 1.4]
     :vector-type-mat2 [1.1 1.2
                        1.4 1.5]
     :vector-type-mat3 [1.1 1.2 1.3
                        1.4 1.5 1.6
                        1.7 1.8 1.9]
     :vector-type-mat4 [1.1 1.2 1.3 0.0
                        1.4 1.5 1.6 0.0
                        1.7 1.8 1.9 0.0
                        0.0 0.0 0.0 1.0]}

    :vector-type-mat4
    {:vector-type-scalar [1.1]
     :vector-type-vec2 [1.1 1.2]
     :vector-type-vec3 [1.1 1.2 1.3]
     :vector-type-vec4 [1.1 1.2 1.3 1.4]
     :vector-type-mat2 [1.1 1.2
                        1.5 1.6]
     :vector-type-mat3 [1.1 1.2 1.3
                        1.5 1.6 1.7
                        1.9 2.0 2.1]
     :vector-type-mat4 [1.1 1.2 1.3 1.4
                        1.5 1.6 1.7 1.8
                        1.9 2.0 2.1 2.2
                        2.3 2.4 2.5 2.6]}}

   :semantic-type-position
   {nil
    {:vector-type-vec4 [0.0 0.0 0.0 1.0]}

    :vector-type-vec2
    {:vector-type-vec4 [1.1 1.2 0.0 1.0]}

    :vector-type-vec3
    {:vector-type-vec4 [1.1 1.2 1.3 1.0]}}

   :semantic-type-color
   {nil
    {:vector-type-scalar [1.0]
     :vector-type-vec2 [1.0 1.0]
     :vector-type-vec3 [1.0 1.0 1.0]
     :vector-type-vec4 [1.0 1.0 1.0 1.0]}

    :vector-type-vec2
    {:vector-type-vec4 [1.1 1.2 0.0 1.0]}

    :vector-type-vec3
    {:vector-type-vec4 [1.1 1.2 1.3 1.0]}}

   :semantic-type-tangent
   {nil
    {:vector-type-vec4 [0.0 0.0 0.0 1.0]}

    :vector-type-vec2
    {:vector-type-vec4 [1.1 1.2 0.0 1.0]}

    :vector-type-vec3
    {:vector-type-vec4 [1.1 1.2 1.3 1.0]}}})

(deftest convert-double-values-rule-declaration-test
  (is (= (conj graphics.types/vector-types nil)
         (set (keys (convert-double-values-rule-declaration :semantic-type-none)))))
  (is (every? #(is (= graphics.types/vector-types (set (keys %))))
              (vals (convert-double-values-rule-declaration :semantic-type-none)))))

(def ^:private convert-double-values-rules
  (let [default-semantic-rules (convert-double-values-rule-declaration :semantic-type-none)]
    (into {}
          (map (fn [semantic-type]
                 (let [declared-semantic-rules (convert-double-values-rule-declaration semantic-type)
                       semantic-rules (merge-with merge default-semantic-rules declared-semantic-rules)]
                   (pair semantic-type semantic-rules))))
          graphics.types/semantic-types)))

(deftest convert-double-values-rules-test
  ;; Sanity checks to verify the rules were merged as expected.
  (is (= {:vector-type-scalar [1.1]
          :vector-type-vec2 [1.1 1.2]
          :vector-type-vec3 [1.1 1.2 1.3]
          :vector-type-vec4 [1.1 1.2 1.3 1.0]
          :vector-type-mat2 [1.1 1.2
                             1.3 0.0]
          :vector-type-mat3 [1.1 1.2 1.3
                             0.0 0.0 0.0
                             0.0 0.0 0.0]
          :vector-type-mat4 [1.1 1.2 1.3 0.0
                             0.0 0.0 0.0 0.0
                             0.0 0.0 0.0 0.0
                             0.0 0.0 0.0 0.0]}
         (get-in convert-double-values-rules
                 [:semantic-type-position :vector-type-vec3])))
  (is (= {:vector-type-scalar [1.0]
          :vector-type-vec2 [1.0 1.0]
          :vector-type-vec3 [1.0 1.0 1.0]
          :vector-type-vec4 [1.0 1.0 1.0 1.0]
          :vector-type-mat2 [1.0 0.0
                             0.0 1.0]
          :vector-type-mat3 [1.0 0.0 0.0
                             0.0 1.0 0.0
                             0.0 0.0 1.0]
          :vector-type-mat4 [1.0 0.0 0.0 0.0
                             0.0 1.0 0.0 0.0
                             0.0 0.0 1.0 0.0
                             0.0 0.0 0.0 1.0]}
         (get-in convert-double-values-rules
                 [:semantic-type-color nil]))))

(deftest convert-double-values-test
  (doseq [[semantic-type semantic-rules] convert-double-values-rules
          [old-vector-type new-vector-type->expected-result] semantic-rules
          :let [original-doubles (if old-vector-type
                                   (new-vector-type->expected-result old-vector-type)
                                   [])]
          [new-vector-type expected-doubles] new-vector-type->expected-result]
    (is (= expected-doubles
           (graphics/convert-double-values original-doubles semantic-type old-vector-type new-vector-type))
        (format "%s, %s -> %s"
                (semantic-type->label semantic-type)
                (vector-type->label old-vector-type)
                (vector-type->label new-vector-type)))))

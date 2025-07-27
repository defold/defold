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

(ns editor.graphics.types-test
  (:require [clojure.test :refer :all]
            [editor.graphics.types :as types]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]))

(set! *warn-on-reflection* true)

(def ^:private random-values [nil 1 "string" :keyword (Object.) #{}])

(defn- pb-ints-by-val [^Class pb-enum-class]
  (into {}
        (map (juxt protobuf/pb-enum->val protobuf/pb-enum->int))
        (protobuf/protocol-message-enums pb-enum-class)))

(deftest buffer-data-type?-test
  (is (every? false? (map types/buffer-data-type? random-values)))
  (is (every? true? (map types/buffer-data-type? types/buffer-data-types))))

(deftest buffer-data-type->data-type-test
  (is (= {:byte :type-byte
          :ubyte :type-unsigned-byte
          :short :type-short
          :ushort :type-unsigned-short
          :int :type-int
          :uint :type-unsigned-int
          :float :type-float}
         (into {}
               (map (juxt identity types/buffer-data-type->data-type))
               types/buffer-data-types))))

(deftest coordinate-space?-test
  (is (every? false? (map types/coordinate-space? random-values)))
  (is (every? true? (map types/coordinate-space? types/coordinate-spaces))))

(deftest coordinate-space-pb-int-test
  (is (= (pb-ints-by-val Graphics$CoordinateSpace)
         (into {}
               (map (juxt identity types/coordinate-space-pb-int))
               types/coordinate-spaces))))

(deftest data-type?-test
  (is (every? false? (map types/data-type? random-values)))
  (is (every? true? (map types/data-type? types/data-types))))

(deftest data-type-byte-size-test
  (is (= {:type-byte Byte/BYTES
          :type-unsigned-byte Byte/BYTES
          :type-short Short/BYTES
          :type-unsigned-short Short/BYTES
          :type-int Integer/BYTES
          :type-unsigned-int Integer/BYTES
          :type-float Float/BYTES}
         (into {}
               (map (juxt identity types/data-type-byte-size))
               types/data-types))))

(pb-ints-by-val Graphics$CoordinateSpace)

(deftest data-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$DataType)
         (into {}
               (map (juxt identity types/data-type-pb-int))
               types/data-types))))

(deftest data-type->buffer-data-type-test
  (is (= {:type-byte :byte
          :type-unsigned-byte :ubyte
          :type-short :short
          :type-unsigned-short :ushort
          :type-int :int
          :type-unsigned-int :uint
          :type-float :float}
         (into {}
               (map (juxt identity types/data-type->buffer-data-type))
               types/data-types))))

(deftest semantic-type?-test
  (is (every? false? (map types/semantic-type? random-values)))
  (is (every? true? (map types/semantic-type? types/semantic-types))))

(deftest semantic-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$SemanticType)
         (into {}
               (map (juxt identity types/semantic-type-pb-int))
               types/semantic-types))))

(deftest engine-provided-semantic-type?-test
  (is (= {:semantic-type-none false
          :semantic-type-position true
          :semantic-type-texcoord false
          :semantic-type-page-index false
          :semantic-type-color false
          :semantic-type-normal false
          :semantic-type-tangent false
          :semantic-type-world-matrix true
          :semantic-type-normal-matrix true
          :semantic-type-bone-weights false
          :semantic-type-bone-indices false}
         (into {}
               (map (juxt identity types/engine-provided-semantic-type?))
               types/semantic-types))))

(deftest vector-type?-test
  (is (every? false? (map types/vector-type? random-values)))
  (is (every? true? (map types/vector-type? types/vector-types))))

(deftest vector-type-component-count-test
  (is (= {:vector-type-scalar 1
          :vector-type-vec2 2
          :vector-type-vec3 3
          :vector-type-vec4 4
          :vector-type-mat2 4
          :vector-type-mat3 9
          :vector-type-mat4 16}
         (into {}
               (map (juxt identity types/vector-type-component-count))
               types/vector-types))))

(deftest vector-type-attribute-count-test
  (is (= {:vector-type-scalar 1
          :vector-type-vec2 1
          :vector-type-vec3 1
          :vector-type-vec4 1
          :vector-type-mat2 2
          :vector-type-mat3 3
          :vector-type-mat4 4}
         (into {}
               (map (juxt identity types/vector-type-attribute-count))
               types/vector-types))))

(deftest vector-type-row-column-count-test
  (is (= {:vector-type-scalar -1
          :vector-type-vec2 -1
          :vector-type-vec3 -1
          :vector-type-vec4 -1
          :vector-type-mat2 2
          :vector-type-mat3 3
          :vector-type-mat4 4}
         (into {}
               (map (juxt identity types/vector-type-row-column-count))
               types/vector-types))))

(deftest vector-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$VectorType)
         (into {}
               (map (juxt identity types/vector-type-pb-int))
               types/vector-types))))

(deftest vertex-step-function?-test
  (is (every? false? (map types/vertex-step-function? random-values)))
  (is (every? true? (map types/vertex-step-function? types/vertex-step-functions))))

(deftest vertex-step-function-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexStepFunction)
         (into {}
               (map (juxt identity types/vertex-step-function-pb-int))
               types/vertex-step-functions))))

(deftest request-id?
  (is (true? (types/request-id? 1)))
  (is (true? (types/request-id? ::request-id)))
  (is (true? (types/request-id? [:a 1])))
  (is (true? (types/request-id? {:key "value"})))
  (is (true? (types/request-id? #{:key})))
  (is (false? (types/request-id? nil)))
  (is (false? (types/request-id? (Object.))))
  (is (false? (types/request-id? (map inc [0 1]))))
  (is (false? (types/request-id? (eduction (map inc) [0 1])))))

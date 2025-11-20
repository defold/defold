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
            [editor.buffers :as buffers]
            [editor.graphics.types :as graphics.types]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]))

(set! *warn-on-reflection* true)

(def ^:private random-values [nil 1 "string" :keyword (Object.) #{}])

(defn- pb-ints-by-val [^Class pb-enum-class]
  (into {}
        (map (juxt protobuf/pb-enum->val protobuf/pb-enum->int))
        (protobuf/protocol-message-enums pb-enum-class)))

(deftest buffer-data-type-data-type-test
  (is (= {:byte :type-byte
          :ubyte :type-unsigned-byte
          :short :type-short
          :ushort :type-unsigned-short
          :int :type-int
          :uint :type-unsigned-int
          :float :type-float}
         (into {}
               (map (juxt identity graphics.types/buffer-data-type-data-type))
               (disj buffers/buffer-data-types :double :long)))))

(deftest coordinate-space?-test
  (is (every? false? (map graphics.types/coordinate-space? random-values)))
  (is (every? true? (map graphics.types/coordinate-space? graphics.types/coordinate-spaces)))
  (is (true? (graphics.types/coordinate-space? :coordinate-space-default))))

(deftest concrete-coordinate-space?-test
  (is (every? false? (map graphics.types/concrete-coordinate-space? random-values)))
  (is (every? true? (map graphics.types/concrete-coordinate-space? graphics.types/concrete-coordinate-spaces)))
  (is (false? (graphics.types/concrete-coordinate-space? :coordinate-space-default))))

(deftest coordinate-space-pb-int-test
  (is (= (pb-ints-by-val Graphics$CoordinateSpace)
         (into {}
               (map (juxt identity graphics.types/coordinate-space-pb-int))
               graphics.types/coordinate-spaces))))

(deftest data-type?-test
  (is (every? false? (map graphics.types/data-type? random-values)))
  (is (every? true? (map graphics.types/data-type? graphics.types/data-types))))

(deftest data-type-byte-size-test
  (is (= {:type-byte Byte/BYTES
          :type-unsigned-byte Byte/BYTES
          :type-short Short/BYTES
          :type-unsigned-short Short/BYTES
          :type-int Integer/BYTES
          :type-unsigned-int Integer/BYTES
          :type-float Float/BYTES}
         (into {}
               (map (juxt identity graphics.types/data-type-byte-size))
               graphics.types/data-types))))

(deftest data-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$DataType)
         (into {}
               (map (juxt identity graphics.types/data-type-pb-int))
               graphics.types/data-types))))

(deftest data-type-buffer-data-type-test
  (is (= {:type-byte :byte
          :type-unsigned-byte :ubyte
          :type-short :short
          :type-unsigned-short :ushort
          :type-int :int
          :type-unsigned-int :uint
          :type-float :float}
         (into {}
               (map (juxt identity graphics.types/data-type-buffer-data-type))
               graphics.types/data-types))))

(deftest semantic-type?-test
  (is (every? false? (map graphics.types/semantic-type? random-values)))
  (is (every? true? (map graphics.types/semantic-type? graphics.types/semantic-types))))

(deftest semantic-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$SemanticType)
         (into {}
               (map (juxt identity graphics.types/semantic-type-pb-int))
               graphics.types/semantic-types))))

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
               (map (juxt identity graphics.types/engine-provided-semantic-type?))
               graphics.types/semantic-types))))

(deftest vector-type?-test
  (is (every? false? (map graphics.types/vector-type? random-values)))
  (is (every? true? (map graphics.types/vector-type? graphics.types/vector-types))))

(deftest vector-type-component-count-test
  (is (= {:vector-type-scalar 1
          :vector-type-vec2 2
          :vector-type-vec3 3
          :vector-type-vec4 4
          :vector-type-mat2 4
          :vector-type-mat3 9
          :vector-type-mat4 16}
         (into {}
               (map (juxt identity graphics.types/vector-type-component-count))
               graphics.types/vector-types))))

(deftest vector-type-attribute-count-test
  (is (= {:vector-type-scalar 1
          :vector-type-vec2 1
          :vector-type-vec3 1
          :vector-type-vec4 1
          :vector-type-mat2 2
          :vector-type-mat3 3
          :vector-type-mat4 4}
         (into {}
               (map (juxt identity graphics.types/vector-type-attribute-count))
               graphics.types/vector-types))))

(deftest vector-type-row-column-count-test
  (is (= {:vector-type-scalar -1
          :vector-type-vec2 -1
          :vector-type-vec3 -1
          :vector-type-vec4 -1
          :vector-type-mat2 2
          :vector-type-mat3 3
          :vector-type-mat4 4}
         (into {}
               (map (juxt identity graphics.types/vector-type-row-column-count))
               graphics.types/vector-types))))

(deftest vector-type-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexAttribute$VectorType)
         (into {}
               (map (juxt identity graphics.types/vector-type-pb-int))
               graphics.types/vector-types))))

(deftest vertex-step-function?-test
  (is (every? false? (map graphics.types/vertex-step-function? random-values)))
  (is (every? true? (map graphics.types/vertex-step-function? graphics.types/vertex-step-functions))))

(deftest vertex-step-function-pb-int-test
  (is (= (pb-ints-by-val Graphics$VertexStepFunction)
         (into {}
               (map (juxt identity graphics.types/vertex-step-function-pb-int))
               graphics.types/vertex-step-functions))))

(deftest request-id?
  (is (true? (graphics.types/request-id? 1)))
  (is (true? (graphics.types/request-id? ::request-id)))
  (is (true? (graphics.types/request-id? [:a 1])))
  (is (true? (graphics.types/request-id? {:key "value"})))
  (is (true? (graphics.types/request-id? #{:key})))
  (is (false? (graphics.types/request-id? nil)))
  (is (false? (graphics.types/request-id? (Object.))))
  (is (false? (graphics.types/request-id? (map inc [0 1]))))
  (is (false? (graphics.types/request-id? (eduction (map inc) [0 1])))))

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

(ns editor.graphics.types
  (:require [editor.protobuf :as protobuf]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [clojure.lang IHashEq]
           [com.dynamo.bob.pipeline GraphicsUtil]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def buffer-data-types
  #{:byte
    :ubyte
    :short
    :ushort
    :int
    :uint
    :float})

(fn/defamong buffer-data-type? buffer-data-types)

(defn buffer-data-type->data-type [buffer-data-type]
  (case buffer-data-type
    :byte :type-byte
    :ubyte :type-unsigned-byte
    :short :type-short
    :ushort :type-unsigned-short
    :int :type-int
    :uint :type-unsigned-int
    :float :type-float))

(def coordinate-spaces (protobuf/valid-enum-values Graphics$CoordinateSpace))

(fn/defamong coordinate-space? coordinate-spaces)

(defn coordinate-space-pb-int
  ^long [coordinate-space]
  (case coordinate-space
    :coordinate-space-default Graphics$CoordinateSpace/COORDINATE_SPACE_DEFAULT_VALUE
    :coordinate-space-world Graphics$CoordinateSpace/COORDINATE_SPACE_WORLD_VALUE
    :coordinate-space-local Graphics$CoordinateSpace/COORDINATE_SPACE_LOCAL_VALUE))

(def data-types (protobuf/valid-enum-values Graphics$VertexAttribute$DataType))

(fn/defamong data-type? data-types)

(defn data-type-byte-size
  ^long [data-type]
  (case data-type
    (:type-byte :type-unsigned-byte) Byte/BYTES
    (:type-short :type-unsigned-short) Short/BYTES
    (:type-int :type-unsigned-int) Integer/BYTES
    (:type-float) Float/BYTES))

(defn data-type-pb-int
  ^long [data-type]
  (case data-type
    :type-byte Graphics$VertexAttribute$DataType/TYPE_BYTE_VALUE
    :type-unsigned-byte Graphics$VertexAttribute$DataType/TYPE_UNSIGNED_BYTE_VALUE
    :type-short Graphics$VertexAttribute$DataType/TYPE_SHORT_VALUE
    :type-unsigned-short Graphics$VertexAttribute$DataType/TYPE_UNSIGNED_SHORT_VALUE
    :type-int  Graphics$VertexAttribute$DataType/TYPE_INT_VALUE
    :type-unsigned-int Graphics$VertexAttribute$DataType/TYPE_UNSIGNED_INT_VALUE
    :type-float Graphics$VertexAttribute$DataType/TYPE_FLOAT_VALUE))

(defn data-type->buffer-data-type [data-type]
  (case data-type
    :type-byte :byte
    :type-unsigned-byte :ubyte
    :type-short :short
    :type-unsigned-short :ushort
    :type-int :int
    :type-unsigned-int :uint
    :type-float :float))

(def semantic-types (protobuf/valid-enum-values Graphics$VertexAttribute$SemanticType))

(fn/defamong semantic-type? semantic-types)

(defn semantic-type-pb-int
  ^long [semantic-type]
  (case semantic-type
    :semantic-type-none Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_NONE_VALUE
    :semantic-type-position Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_POSITION_VALUE
    :semantic-type-texcoord Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_TEXCOORD_VALUE
    :semantic-type-page-index Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_PAGE_INDEX_VALUE
    :semantic-type-color Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_COLOR_VALUE
    :semantic-type-normal Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_NORMAL_VALUE
    :semantic-type-tangent Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_TANGENT_VALUE
    :semantic-type-world-matrix Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_WORLD_MATRIX_VALUE
    :semantic-type-normal-matrix Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_NORMAL_MATRIX_VALUE
    :semantic-type-bone-weights Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_BONE_WEIGHTS_VALUE
    :semantic-type-bone-indices Graphics$VertexAttribute$SemanticType/SEMANTIC_TYPE_BONE_INDICES_VALUE))

(def engine-provided-semantic-types
  (into #{}
        (keep (fn [^Graphics$VertexAttribute$SemanticType pb-semantic-type]
                (when (GraphicsUtil/isEngineProvidedAttributeSemanticType pb-semantic-type)
                  (protobuf/pb-enum->val pb-semantic-type))))
        (protobuf/protocol-message-enums Graphics$VertexAttribute$SemanticType)))

(fn/defamong engine-provided-semantic-type? engine-provided-semantic-types)

(def vector-types (protobuf/valid-enum-values Graphics$VertexAttribute$VectorType))

(fn/defamong vector-type? vector-types)

(defn vector-type-component-count
  ^long [vector-type]
  (case vector-type
    :vector-type-scalar 1
    :vector-type-vec2 2
    :vector-type-vec3 3
    :vector-type-vec4 4
    :vector-type-mat2 4
    :vector-type-mat3 9
    :vector-type-mat4 16))

(defn vector-type-row-column-count
  ^long [vector-type]
  (case vector-type
    :vector-type-mat2 2
    :vector-type-mat3 3
    :vector-type-mat4 4
    -1))

(defn vector-type-pb-int
  ^long [vector-type]
  (case vector-type
    :vector-type-scalar Graphics$VertexAttribute$VectorType/VECTOR_TYPE_SCALAR_VALUE
    :vector-type-vec2 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_VEC2_VALUE
    :vector-type-vec3 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_VEC3_VALUE
    :vector-type-vec4 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_VEC4_VALUE
    :vector-type-mat2 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_MAT2_VALUE
    :vector-type-mat3 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_MAT3_VALUE
    :vector-type-mat4 Graphics$VertexAttribute$VectorType/VECTOR_TYPE_MAT4_VALUE))

(def vertex-step-functions (protobuf/valid-enum-values Graphics$VertexStepFunction))

(fn/defamong vertex-step-function? vertex-step-functions)

(defn vertex-step-function-pb-int
  ^long [vertex-step-function]
  (case vertex-step-function
    :vertex-step-function-vertex Graphics$VertexStepFunction/VERTEX_STEP_FUNCTION_VERTEX_VALUE
    :vertex-step-function-instance Graphics$VertexStepFunction/VERTEX_STEP_FUNCTION_INSTANCE_VALUE))

(defn request-id? [value]
  ;; We might want to add support for other values as well.
  (or (number? value)
      (and (instance? IHashEq value)
           (not (coll/lazy? value)))))

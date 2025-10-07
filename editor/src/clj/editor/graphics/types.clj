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
  (:require [editor.buffers]
            [editor.protobuf :as protobuf]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.ensure :as ensure]
            [util.fn :as fn]
            [util.num :as num])
  (:import [clojure.lang IHashEq]
           [com.dynamo.bob.pipeline GraphicsUtil]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]
           [editor.buffers BufferData]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record ElementType
  [vector-type
   data-type
   ^boolean normalize])

(defonce/protocol ElementBuffer
  (buffer-data ^BufferData [this] "Returns the BufferData that contains the elements of this buffer.")
  (element-type ^ElementType [this] "Returns the ElementType of elements in this buffer."))

(defonce ^:private vec1-doubles-zero (vector-of :double 0.0))
(defonce ^:private vec1-doubles-one (vector-of :double 1.0))

(defonce ^:private vec2-doubles-zero (vector-of :double 0.0 0.0))
(defonce ^:private vec2-doubles-one (vector-of :double 1.0 1.0))

(defonce ^:private vec3-doubles-zero (vector-of :double 0.0 0.0 0.0))
(defonce ^:private vec3-doubles-one (vector-of :double 1.0 1.0 1.0))

(defonce ^:private vec4-doubles-zero (vector-of :double 0.0 0.0 0.0 0.0))
(defonce ^:private vec4-doubles-one (vector-of :double 1.0 1.0 1.0 1.0))
(defonce ^:private vec4-doubles-xyz-zero-w-one (vector-of :double 0.0 0.0 0.0 1.0))

(defonce ^:private mat2-doubles-identity
  (vector-of :double
    1.0 0.0
    0.0 1.0))

(defonce ^:private mat3-doubles-identity
  (vector-of :double
    1.0 0.0 0.0
    0.0 1.0 0.0
    0.0 0.0 1.0))

(defonce ^:private mat4-doubles-identity
  (vector-of :double
    1.0 0.0 0.0 0.0
    0.0 1.0 0.0 0.0
    0.0 0.0 1.0 0.0
    0.0 0.0 0.0 1.0))

(defn default-attribute-doubles [semantic-type vector-type]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color vec1-doubles-one
                          vec1-doubles-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color vec2-doubles-one
                        vec2-doubles-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color vec3-doubles-one
                        vec3-doubles-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position vec4-doubles-xyz-zero-w-one
                        :semantic-type-color vec4-doubles-one
                        :semantic-type-tangent vec4-doubles-xyz-zero-w-one
                        vec4-doubles-zero)
    :vector-type-mat2 mat2-doubles-identity
    :vector-type-mat3 mat3-doubles-identity
    :vector-type-mat4 mat4-doubles-identity))

(defn- default-attribute-value-array-raw [semantic-type ^ElementType element-type]
  (let [[array-fn normalize-fn]
        (case (.-data-type element-type)
          :type-byte [byte-array num/normalized->byte]
          :type-unsigned-byte [byte-array num/normalized->ubyte]
          :type-short [short-array num/normalized->short]
          :type-unsigned-short [short-array num/normalized->ushort]
          :type-int [int-array num/normalized->int]
          :type-unsigned-int [int-array num/normalized->uint]
          :type-float [float-array identity])

        doubles (default-attribute-doubles semantic-type (.-vector-type element-type))
        array-values (cond->> doubles (.-normalize element-type) (mapv normalize-fn))]
    (array-fn array-values)))

(def ^{:arglists '([semantic-type ^ElementType element-type])} default-attribute-value-array (fn/memoize default-attribute-value-array-raw))

(defonce attribute-transforms
  #{:attribute-transform-none
    :attribute-transform-normal
    :attribute-transform-world})

(fn/defamong attribute-transform? attribute-transforms)

(defn attribute-transform [semantic-type coordinate-space]
  (case semantic-type
    (:semantic-type-position)
    (case coordinate-space
      :coordinate-space-local :attribute-transform-none
      :coordinate-space-world :attribute-transform-world)

    (:semantic-type-normal :semantic-type-tangent)
    (case coordinate-space
      :coordinate-space-local :attribute-transform-none
      :coordinate-space-world :attribute-transform-normal)

    ;; else
    :attribute-transform-none))

(defonce buffer-data-types
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

(def channel? nat-int?)

(defonce coordinate-spaces (protobuf/valid-enum-values Graphics$CoordinateSpace))

(fn/defamong coordinate-space? coordinate-spaces)

(defonce concrete-coordinate-spaces (disj coordinate-spaces :coordinate-space-default))

(fn/defamong concrete-coordinate-space? concrete-coordinate-spaces)

(defn coordinate-space-pb-int
  ^long [coordinate-space]
  (case coordinate-space
    :coordinate-space-default Graphics$CoordinateSpace/COORDINATE_SPACE_DEFAULT_VALUE
    :coordinate-space-world Graphics$CoordinateSpace/COORDINATE_SPACE_WORLD_VALUE
    :coordinate-space-local Graphics$CoordinateSpace/COORDINATE_SPACE_LOCAL_VALUE))

(defonce data-types (protobuf/valid-enum-values Graphics$VertexAttribute$DataType))

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
    :type-int Graphics$VertexAttribute$DataType/TYPE_INT_VALUE
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

(defonce semantic-types (protobuf/valid-enum-values Graphics$VertexAttribute$SemanticType))

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

(def location? nat-int?)

(defn location-vector? [value]
  (and (= :int (coll/primitive-vector-type value))
       (pos? (count value))
       (coll/not-any? neg? value)))

(def usage? #{:dynamic :static})

(defonce vector-types (protobuf/valid-enum-values Graphics$VertexAttribute$VectorType))

(fn/defamong vector-type? vector-types)

(defn component-count-vector-type
  [^long component-count is-matrix]
  (case component-count
    1 :vector-type-scalar
    2 :vector-type-vec2
    3 :vector-type-vec3
    4 (if is-matrix :vector-type-mat2 :vector-type-vec4)
    9 :vector-type-mat3
    16 :vector-type-mat4))

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

(defn vector-type-attribute-count
  ;; Each GLSL attribute hosts up to four components. Matrix attributes are
  ;; spread across multiple attributes, even if the value could technically fit
  ;; into four components. This means a mat2 will use the x and y components of
  ;; two attributes.
  ^long [vector-type]
  (case vector-type
    :vector-type-scalar 1
    :vector-type-vec2 1
    :vector-type-vec3 1
    :vector-type-vec4 1
    :vector-type-mat2 2
    :vector-type-mat3 3
    :vector-type-mat4 4))

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

(defonce vertex-step-functions (protobuf/valid-enum-values Graphics$VertexStepFunction))

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

(definline element-type? [value]
  `(instance? ElementType ~value))

(definline element-buffer? [value]
  `(satisfies? ElementBuffer ~value))

(defn element-count
  ^long [element-buffer]
  (if (nil? element-buffer)
    0
    (let [buffer-data (buffer-data element-buffer)
          item-count (count buffer-data)]
      (if (zero? item-count)
        0
        (let [element-type (element-type element-buffer)
              vector-type (.-vector-type element-type)
              component-count (vector-type-component-count vector-type)]
          (quot item-count component-count))))))

(defn element-data-types-assignable? [^ElementType source ^ElementType target]
  (let [source-data-type (.-data-type source)
        target-data-type (.-data-type target)]
    (or (= target-data-type source-data-type)
        (case source-data-type
          (:type-byte :type-unsigned-byte :type-short :type-unsigned-short :type-int :type-unsigned-int)
          (case target-data-type
            (:type-float) (.-normalize source)
            (<= (data-type-byte-size source-data-type)
                (data-type-byte-size target-data-type)))

          (:type-float)
          (case target
            (:type-float) true
            false)))))

(defn element-vector-types-assignable? [^ElementType source ^ElementType target]
  (let [source-vector-type (.-vector-type source)
        target-vector-type (.-vector-type target)]
    (or (= target-vector-type source-vector-type)
        (case source-vector-type
          (:vector-type-scalar :vector-type-vec2 :vector-type-vec3 :vector-type-vec4)
          (case target-vector-type
            (:vector-type-scalar :vector-type-vec2 :vector-type-vec3 :vector-type-vec4) true
            false)

          (:vector-type-mat2 :vector-type-mat3 :vector-type-mat4)
          (case target-vector-type
            (:vector-type-mat2 :vector-type-mat3 :vector-type-mat4) true
            false)))))

(defn element-types-assignable? [^ElementType source ^ElementType target]
  (and (element-data-types-assignable? source target)
       (element-vector-types-assignable? source target)))

(defn floating-point-element-type? [value]
  (and (element-type? value)
       (= :type-float (.-data-type ^ElementType value))))

(defn ensure-floating-point-target-element-type [^ElementType target-element-type]
  (when-not (floating-point-element-type? target-element-type)
    (throw
      (IllegalArgumentException.
        (str "floating-point values cannot be assigned to target data-type: "
             (.-data-type target-element-type))))))

(defn- make-element-type-raw
  ^ElementType [vector-type data-type normalize]
  (ensure/argument vector-type vector-type?)
  (ensure/argument data-type data-type?)
  (ensure/argument normalize boolean? "%s must be a boolean")
  (->ElementType vector-type data-type normalize))

(def ^{:arglists '([vector-type data-type normalize])} make-element-type (fn/memoize make-element-type-raw))

(defn attribute-name-key [^String attribute-name]
  (protobuf/field-name->key attribute-name))

(def attribute-key? keyword?)

(defn infer-semantic-type
  "Attempt to infer the semantic-type based on the supplied attribute-key name.
  The engine runtime uses the same heuristics for shader attributes that are not
  declared in the material. Returns :semantic-type-none if there is no match."
  [attribute-key]
  (case attribute-key
    :position :semantic-type-position
    :normal :semantic-type-normal
    :tangent :semantic-type-tangent
    :binormal :semantic-type-binormal
    :color :semantic-type-color
    (:texcoord :texcoord0 :texcoord1) :semantic-type-texcoord
    :page-index :semantic-type-page-index
    :bone-indices :semantic-type-bone-indices
    :bone-weights :semantic-type-bone-weights
    :mtx-world :semantic-type-world-matrix
    :mtx-normal :semantic-type-normal-matrix
    :semantic-type-none))

(defn infer-normalize
  "Attempt to infer if the data supplied to a vertex attribute should be
  normalized when read in the shader. Currently unused, as the engine runtime
  doesn't do this, and we want the editor to match its behavior."
  [semantic-type data-type]
  (and (not= :type-float data-type)
       (case semantic-type
         (:semantic-type-bone-indices :semantic-type-none :semantic-type-page-index) false
         true)))

(defn attribute-reflection-info? [value]
  (and (map? value)
       (string? (:name value))
       (attribute-key? (:name-key value))
       (location? (:location value))
       (data-type? (:data-type value))
       (vector-type? (:vector-type value))
       (boolean? (:normalize value))))

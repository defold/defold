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
  (:require [clojure.string :as string]
            [editor.buffers]
            [editor.protobuf :as protobuf]
            [util.array :as array]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.ensure :as ensure]
            [util.fn :as fn]
            [util.num :as num])
  (:import [clojure.lang IHashEq]
           [com.dynamo.bob.pipeline GraphicsUtil]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$ShaderDesc$ShaderType Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]
           [com.sun.jna Pointer]
           [editor.buffers BufferData]
           [javax.vecmath Vector4d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record ElementType
  [vector-type
   data-type
   ^boolean normalize])

(defonce/protocol ElementBuffer
  (^BufferData buffer-data [this] "Returns the BufferData that contains the elements of this buffer.")
  (^ElementType element-type [this] "Returns the ElementType of elements in this buffer."))

(defonce/protocol ValueBinding
  (with-value [this new-value] "Return a new instance with the value replaced."))

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

(defn buffer-data-type-data-type [buffer-data-type]
  (case buffer-data-type
    :byte :type-byte
    :ubyte :type-unsigned-byte
    :short :type-short
    :ushort :type-unsigned-short
    :int :type-int
    :uint :type-unsigned-int
    :float :type-float))

(def array-size? nat-int?)

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

(defn data-type-primitive-type [data-type]
  (case data-type
    (:type-byte :type-unsigned-byte) :byte
    (:type-short :type-unsigned-short) :short
    (:type-int :type-unsigned-int) :int
    (:type-float) :float))

(defn data-type-buffer-data-type [data-type]
  (case data-type
    :type-byte :byte
    :type-unsigned-byte :ubyte
    :type-short :short
    :type-unsigned-short :ushort
    :type-int :int
    :type-unsigned-int :uint
    :type-float :float))

(defn value-to-double-fn [data-type normalize]
  (case data-type
    :type-byte (if normalize num/byte-range->normalized double)
    :type-unsigned-byte (if normalize num/ubyte-range->normalized double)
    :type-short (if normalize num/short-range->normalized double)
    :type-unsigned-short (if normalize num/ushort-range->normalized double)
    :type-int (if normalize num/int-range->normalized double)
    :type-unsigned-int (if normalize num/uint-range->normalized double)
    :type-float double))

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
    (do
      (assert (semantic-type? semantic-type))
      :attribute-transform-none)))

(defn attribute-transform-render-arg-key [attribute-transform]
  (case attribute-transform
    :attribute-transform-normal :actual/world-rotation
    :attribute-transform-world :actual/world))

(defn attribute-transform-w-component
  ^double [attribute-transform]
  (case attribute-transform
    :attribute-transform-normal 0.0
    :attribute-transform-world 1.0))

(defn assign-attribute-transform [{:keys [coordinate-space semantic-type] :as attribute-info} default-coordinate-space]
  (ensure/argument default-coordinate-space concrete-coordinate-space?)
  (let [coordinate-space
        (case coordinate-space
          (:coordinate-space-default nil) default-coordinate-space
          (:coordinate-space-local :coordinate-space-world) coordinate-space)

        attribute-transform
        (attribute-transform semantic-type coordinate-space)]

    (assoc attribute-info
      :coordinate-space coordinate-space
      :attribute-transform attribute-transform)))

(def location? nat-int?)

(defn location-vector? [value]
  (and (= :int (coll/primitive-vector-type value))
       (pos? (count value))
       (coll/not-any? neg? value)))

(def usage? #{:dynamic :static :stream})

(defonce vector-types (protobuf/valid-enum-values Graphics$VertexAttribute$VectorType))

(fn/defamong vector-type? vector-types)

(defn component-count-vector-type [^long component-count is-matrix]
  {:post [(vector-type? %)]}
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

(defonce uniform-types
  #{:uniform-type-bool
    :uniform-type-bool-vec2
    :uniform-type-bool-vec3
    :uniform-type-bool-vec4
    :uniform-type-float
    :uniform-type-float-mat2
    :uniform-type-float-mat3
    :uniform-type-float-mat4
    :uniform-type-float-vec2
    :uniform-type-float-vec3
    :uniform-type-float-vec4
    :uniform-type-int
    :uniform-type-int-vec2
    :uniform-type-int-vec3
    :uniform-type-int-vec4
    :uniform-type-sampler-2d
    :uniform-type-sampler-cube})

(fn/defamong uniform-type? uniform-types)

(defn sampler-uniform-type? [uniform-type]
  (case uniform-type
    (:uniform-type-sampler-2d :uniform-type-sampler-cube) true
    (do (assert (uniform-type? uniform-type))
        false)))

(defn assign-vector-components!
  ^Vector4d [^Vector4d vector value-array ^ElementType element-type]
  (let [vector-type (.-vector-type element-type)]
    (if (and (= :vector-type-vec4 vector-type)
             (= double/1 (class value-array)))
      (.set vector ^double/1 value-array)
      (let [nth-fn (array/nth-fn value-array)
            double-fn (value-to-double-fn (.-data-type element-type) (.-normalize element-type))
            get (fn get
                  ^double [^long component-index]
                  (double-fn (nth-fn value-array component-index)))]
        (case vector-type
          :vector-type-scalar
          (.setX vector (get 0))

          :vector-type-vec2
          (do (.setX vector (get 0))
              (.setY vector (get 1)))

          :vector-type-vec3
          (do (.setX vector (get 0))
              (.setY vector (get 1))
              (.setZ vector (get 2)))

          :vector-type-vec4
          (do (.setX vector (get 0))
              (.setY vector (get 1))
              (.setZ vector (get 2))
              (.setW vector (get 3)))

          ;; else
          (throw
            (ex-info
              (format "%s cannot be assigned to Vector4d" vector-type)
              {:value-array value-array
               :element-type element-type}))))))
  vector)

(defonce shader-types (protobuf/valid-enum-values Graphics$ShaderDesc$ShaderType))

(fn/defamong shader-type? shader-types)

(defn type-ext-shader-type [^String type-ext]
  (case type-ext
    "cp" :shader-type-compute
    "fp" :shader-type-fragment
    "vp" :shader-type-vertex))

(defn shader-type-pb-int
  ^long [shader-type]
  (case shader-type
    :shader-type-compute Graphics$ShaderDesc$ShaderType/SHADER_TYPE_COMPUTE_VALUE
    :shader-type-fragment Graphics$ShaderDesc$ShaderType/SHADER_TYPE_FRAGMENT_VALUE
    :shader-type-vertex Graphics$ShaderDesc$ShaderType/SHADER_TYPE_VERTEX_VALUE))

(defn shader-type-pb-shader-type
  ^Graphics$ShaderDesc$ShaderType [shader-type]
  (case shader-type
    :shader-type-compute Graphics$ShaderDesc$ShaderType/SHADER_TYPE_COMPUTE
    :shader-type-fragment Graphics$ShaderDesc$ShaderType/SHADER_TYPE_FRAGMENT
    :shader-type-vertex Graphics$ShaderDesc$ShaderType/SHADER_TYPE_VERTEX))

(defn filename-shader-type [^String filename]
  {:pre [(string? filename)
         (pos? (count filename))]}
  (let [type-ext (string/lower-case (FilenameUtils/getExtension filename))]
    (type-ext-shader-type type-ext)))

(defn filename-pb-shader-type
  ^Graphics$ShaderDesc$ShaderType [^String filename]
  (-> filename
      filename-shader-type
      shader-type-pb-shader-type))

(defonce vertex-step-functions (protobuf/valid-enum-values Graphics$VertexStepFunction))

(fn/defamong vertex-step-function? vertex-step-functions)

(defn vertex-step-function-pb-int
  ^long [vertex-step-function]
  (case vertex-step-function
    :vertex-step-function-vertex Graphics$VertexStepFunction/VERTEX_STEP_FUNCTION_VERTEX_VALUE
    :vertex-step-function-instance Graphics$VertexStepFunction/VERTEX_STEP_FUNCTION_INSTANCE_VALUE))

(defn request-id? [value]
  ;; A good request-id must implement efficient hashing. These cover what we've
  ;; seen so far, but we might want to add support for other values as well.
  (or (number? value)
      (keyword? value)
      (instance? Pointer value)
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

(defn- make-element-type-raw
  ^ElementType [vector-type data-type normalize]
  (ensure/argument vector-type vector-type?)
  (ensure/argument data-type data-type?)
  (ensure/argument normalize boolean? "%s must be a boolean")
  (->ElementType vector-type data-type normalize))

(def ^{:arglists '([vector-type data-type normalize])} make-element-type (fn/memoize make-element-type-raw))

(defn element-type-value-array? [^ElementType element-type value]
  (and (= (data-type-primitive-type (.-data-type element-type))
          (array/primitive-type value))
       (= (vector-type-component-count (.-vector-type element-type))
          (array/length value))))

(defn ensure-element-type-value-array [^ElementType element-type value]
  (when-not (element-type-value-array? element-type value)
    (throw
      (ex-info
        "The value must be a primitive array matching the element-type."
        {:value value
         :element-type element-type}))))

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
         (do (assert (semantic-type? semantic-type))
             true))))

(defn uniform-reflection-info? [value]
  (and (map? value)
       (string? (:name value))
       (uniform-type? (:uniform-type value))
       (array-size? (:array-size value))
       (let [location (:location value)]
         (or (= -1 location) ; Built-in uniforms like gl_ModelViewProjectionMatrix have no location.
             (location? location)))))

(defn attribute-info? [value]
  (and (map? value)
       (string? (:name value))
       (attribute-key? (:name-key value))
       (vector-type? (:vector-type value))
       (data-type? (:data-type value))
       (semantic-type? (:semantic-type value))
       (coordinate-space? (:coordinate-space value))
       (vertex-step-function? (:step-function value))
       (boolean? (:normalize value))))

(defn attribute-reflection-info? [value]
  (and (attribute-info? value)
       (location? (:location value))
       (array-size? (:array-size value))))

(defn attribute-info-with-reflection-info [attribute-info attribute-reflection-info]
  {:pre [(attribute-info? attribute-info)]}
  (let [location (:location attribute-reflection-info)
        array-size (:array-size attribute-reflection-info)
        source-vector-type (:vector-type attribute-info)
        target-vector-type (:vector-type attribute-reflection-info)]
    (assert (location? location))
    (assert (array-size? array-size))
    (cond-> (assoc attribute-info
              :location location
              :array-size array-size)

            (< (vector-type-component-count target-vector-type)
               (vector-type-component-count source-vector-type))
            (assoc :vector-type target-vector-type))))

(defn attribute-info-byte-size
  ^long [{:keys [data-type vector-type :as _attribute-info]}]
  (* (data-type-byte-size data-type)
     (vector-type-component-count vector-type)))

(defn attribute-info-element-type
  ^ElementType [attribute-info]
  (make-element-type (:vector-type attribute-info)
                     (:data-type attribute-info)
                     (:normalize attribute-info)))

(defn make-vertex-description [attribute-infos]
  {:pre [(vector? attribute-infos)
         (every? attribute-info? attribute-infos)]}
  (let [byte-size (transduce (map attribute-info-byte-size) + 0 attribute-infos)]
    {:size byte-size
     :attributes attribute-infos}))

(defn vertex-description? [value]
  (and (map? value)
       (pos-int? (:size value))
       (let [attributes (:attributes value)]
         (and (vector? attributes)
              (pos? (count attributes))
              (every? attribute-info? attributes)))))

(def renderable-tag? keyword?)

(defn renderable-tags? [value]
  (and (set? value)
       (coll/every? renderable-tag? value)))

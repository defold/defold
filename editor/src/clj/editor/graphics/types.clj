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
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.ensure :as ensure]
            [util.fn :as fn]
            [util.num :as num])
  (:import [clojure.lang IHashEq]
           [com.dynamo.bob.pipeline GraphicsUtil]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record ElementType
  [vector-type
   data-type
   ^boolean normalize])

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

(defonce ^:private vec1-byte-array-zero (byte-array vec1-doubles-zero))
(defonce ^:private vec1-byte-array-one (byte-array vec1-doubles-one))
(defonce ^:private vec1-byte-array-normalized-one (byte-array (mapv num/normalized->byte vec1-doubles-one)))
(defonce ^:private vec1-byte-array-normalized-unsigned-one (byte-array (mapv num/normalized->ubyte vec1-doubles-one)))

(defonce ^:private vec2-byte-array-zero (byte-array vec2-doubles-zero))
(defonce ^:private vec2-byte-array-one (byte-array vec2-doubles-one))
(defonce ^:private vec2-byte-array-normalized-one (byte-array (mapv num/normalized->byte vec2-doubles-one)))
(defonce ^:private vec2-byte-array-normalized-unsigned-one (byte-array (mapv num/normalized->ubyte vec2-doubles-one)))

(defonce ^:private vec3-byte-array-zero (byte-array vec3-doubles-zero))
(defonce ^:private vec3-byte-array-one (byte-array vec3-doubles-one))
(defonce ^:private vec3-byte-array-normalized-one (byte-array (mapv num/normalized->byte vec3-doubles-one)))
(defonce ^:private vec3-byte-array-normalized-unsigned-one (byte-array (mapv num/normalized->ubyte vec3-doubles-one)))

(defonce ^:private vec4-byte-array-zero (byte-array vec4-doubles-zero))
(defonce ^:private vec4-byte-array-one (byte-array vec4-doubles-one))
(defonce ^:private vec4-byte-array-normalized-one (byte-array (mapv num/normalized->byte vec4-doubles-one)))
(defonce ^:private vec4-byte-array-normalized-unsigned-one (byte-array (mapv num/normalized->ubyte vec4-doubles-one)))
(defonce ^:private vec4-byte-array-xyz-zero-w-one (byte-array vec4-doubles-xyz-zero-w-one))
(defonce ^:private vec4-byte-array-normalized-xyz-zero-w-one (byte-array (mapv num/normalized->byte vec4-doubles-xyz-zero-w-one)))
(defonce ^:private vec4-byte-array-normalized-unsigned-xyz-zero-w-one (byte-array (mapv num/normalized->ubyte vec4-doubles-xyz-zero-w-one)))

(defonce ^:private mat2-byte-array-identity (byte-array mat2-doubles-identity))
(defonce ^:private mat2-byte-array-normalized-identity (byte-array (mapv num/normalized->byte mat2-doubles-identity)))
(defonce ^:private mat2-byte-array-normalized-unsigned-identity (byte-array (mapv num/normalized->ubyte mat2-doubles-identity)))
(defonce ^:private mat3-byte-array-identity (byte-array mat3-doubles-identity))
(defonce ^:private mat3-byte-array-normalized-identity (byte-array (mapv num/normalized->byte mat3-doubles-identity)))
(defonce ^:private mat3-byte-array-normalized-unsigned-identity (byte-array (mapv num/normalized->ubyte mat3-doubles-identity)))
(defonce ^:private mat4-byte-array-identity (byte-array mat4-doubles-identity))
(defonce ^:private mat4-byte-array-normalized-identity (byte-array (mapv num/normalized->byte mat4-doubles-identity)))
(defonce ^:private mat4-byte-array-normalized-unsigned-identity (byte-array (mapv num/normalized->ubyte mat4-doubles-identity)))

(defn default-attribute-byte-array
  ^bytes [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-byte-array-normalized-one vec1-byte-array-one)
                          vec1-byte-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-byte-array-normalized-one vec2-byte-array-one)
                        vec2-byte-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-byte-array-normalized-one vec3-byte-array-one)
                        vec3-byte-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-byte-array-normalized-xyz-zero-w-one vec4-byte-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-byte-array-normalized-one vec4-byte-array-one)
                        :semantic-type-tangent (if normalize vec4-byte-array-normalized-xyz-zero-w-one vec4-byte-array-xyz-zero-w-one)
                        vec4-byte-array-zero)
    :vector-type-mat2 (if normalize mat2-byte-array-normalized-identity mat2-byte-array-identity)
    :vector-type-mat3 (if normalize mat3-byte-array-normalized-identity mat3-byte-array-identity)
    :vector-type-mat4 (if normalize mat4-byte-array-normalized-identity mat4-byte-array-identity)))

(defn default-attribute-unsigned-byte-array
  ^bytes [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-byte-array-normalized-unsigned-one vec1-byte-array-one)
                          vec1-byte-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-byte-array-normalized-unsigned-one vec2-byte-array-one)
                        vec2-byte-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-byte-array-normalized-unsigned-one vec3-byte-array-one)
                        vec3-byte-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-byte-array-normalized-unsigned-xyz-zero-w-one vec4-byte-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-byte-array-normalized-unsigned-one vec4-byte-array-one)
                        :semantic-type-tangent (if normalize vec4-byte-array-normalized-unsigned-xyz-zero-w-one vec4-byte-array-xyz-zero-w-one)
                        vec4-byte-array-zero)
    :vector-type-mat2 (if normalize mat2-byte-array-normalized-unsigned-identity mat2-byte-array-identity)
    :vector-type-mat3 (if normalize mat3-byte-array-normalized-unsigned-identity mat3-byte-array-identity)
    :vector-type-mat4 (if normalize mat4-byte-array-normalized-unsigned-identity mat4-byte-array-identity)))

(defonce ^:private vec1-short-array-zero (short-array vec1-doubles-zero))
(defonce ^:private vec1-short-array-one (short-array vec1-doubles-one))
(defonce ^:private vec1-short-array-normalized-one (short-array (mapv num/normalized->short vec1-doubles-one)))
(defonce ^:private vec1-short-array-normalized-unsigned-one (short-array (mapv num/normalized->ushort vec1-doubles-one)))

(defonce ^:private vec2-short-array-zero (short-array vec2-doubles-zero))
(defonce ^:private vec2-short-array-one (short-array vec2-doubles-one))
(defonce ^:private vec2-short-array-normalized-one (short-array (mapv num/normalized->short vec2-doubles-one)))
(defonce ^:private vec2-short-array-normalized-unsigned-one (short-array (mapv num/normalized->ushort vec2-doubles-one)))

(defonce ^:private vec3-short-array-zero (short-array vec3-doubles-zero))
(defonce ^:private vec3-short-array-one (short-array vec3-doubles-one))
(defonce ^:private vec3-short-array-normalized-one (short-array (mapv num/normalized->short vec3-doubles-one)))
(defonce ^:private vec3-short-array-normalized-unsigned-one (short-array (mapv num/normalized->ushort vec3-doubles-one)))

(defonce ^:private vec4-short-array-zero (short-array vec4-doubles-zero))
(defonce ^:private vec4-short-array-one (short-array vec4-doubles-one))
(defonce ^:private vec4-short-array-normalized-one (short-array (mapv num/normalized->short vec4-doubles-one)))
(defonce ^:private vec4-short-array-normalized-unsigned-one (short-array (mapv num/normalized->ushort vec4-doubles-one)))
(defonce ^:private vec4-short-array-xyz-zero-w-one (short-array vec4-doubles-xyz-zero-w-one))
(defonce ^:private vec4-short-array-normalized-xyz-zero-w-one (short-array (mapv num/normalized->short vec4-doubles-xyz-zero-w-one)))
(defonce ^:private vec4-short-array-normalized-unsigned-xyz-zero-w-one (short-array (mapv num/normalized->ushort vec4-doubles-xyz-zero-w-one)))

(defonce ^:private mat2-short-array-identity (short-array mat2-doubles-identity))
(defonce ^:private mat2-short-array-normalized-identity (short-array (mapv num/normalized->short mat2-doubles-identity)))
(defonce ^:private mat2-short-array-normalized-unsigned-identity (short-array (mapv num/normalized->ushort mat2-doubles-identity)))
(defonce ^:private mat3-short-array-identity (short-array mat3-doubles-identity))
(defonce ^:private mat3-short-array-normalized-identity (short-array (mapv num/normalized->short mat3-doubles-identity)))
(defonce ^:private mat3-short-array-normalized-unsigned-identity (short-array (mapv num/normalized->ushort mat3-doubles-identity)))
(defonce ^:private mat4-short-array-identity (short-array mat4-doubles-identity))
(defonce ^:private mat4-short-array-normalized-identity (short-array (mapv num/normalized->short mat4-doubles-identity)))
(defonce ^:private mat4-short-array-normalized-unsigned-identity (short-array (mapv num/normalized->ushort mat4-doubles-identity)))

(defn default-attribute-short-array
  ^shorts [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-short-array-normalized-one vec1-short-array-one)
                          vec1-short-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-short-array-normalized-one vec2-short-array-one)
                        vec2-short-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-short-array-normalized-one vec3-short-array-one)
                        vec3-short-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-short-array-normalized-xyz-zero-w-one vec4-short-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-short-array-normalized-one vec4-short-array-one)
                        :semantic-type-tangent (if normalize vec4-short-array-normalized-xyz-zero-w-one vec4-short-array-xyz-zero-w-one)
                        vec4-short-array-zero)
    :vector-type-mat2 (if normalize mat2-short-array-normalized-identity mat2-short-array-identity)
    :vector-type-mat3 (if normalize mat3-short-array-normalized-identity mat3-short-array-identity)
    :vector-type-mat4 (if normalize mat4-short-array-normalized-identity mat4-short-array-identity)))

(defn default-attribute-unsigned-short-array
  ^shorts [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-short-array-normalized-unsigned-one vec1-short-array-one)
                          vec1-short-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-short-array-normalized-unsigned-one vec2-short-array-one)
                        vec2-short-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-short-array-normalized-unsigned-one vec3-short-array-one)
                        vec3-short-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-short-array-normalized-unsigned-xyz-zero-w-one vec4-short-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-short-array-normalized-unsigned-one vec4-short-array-one)
                        :semantic-type-tangent (if normalize vec4-short-array-normalized-unsigned-xyz-zero-w-one vec4-short-array-xyz-zero-w-one)
                        vec4-short-array-zero)
    :vector-type-mat2 (if normalize mat2-short-array-normalized-unsigned-identity mat2-short-array-identity)
    :vector-type-mat3 (if normalize mat3-short-array-normalized-unsigned-identity mat3-short-array-identity)
    :vector-type-mat4 (if normalize mat4-short-array-normalized-unsigned-identity mat4-short-array-identity)))

(defonce ^:private vec1-int-array-zero (int-array vec1-doubles-zero))
(defonce ^:private vec1-int-array-one (int-array vec1-doubles-one))
(defonce ^:private vec1-int-array-normalized-one (int-array (mapv num/normalized->int vec1-doubles-one)))
(defonce ^:private vec1-int-array-normalized-unsigned-one (int-array (mapv num/normalized->uint vec1-doubles-one)))

(defonce ^:private vec2-int-array-zero (int-array vec2-doubles-zero))
(defonce ^:private vec2-int-array-one (int-array vec2-doubles-one))
(defonce ^:private vec2-int-array-normalized-one (int-array (mapv num/normalized->int vec2-doubles-one)))
(defonce ^:private vec2-int-array-normalized-unsigned-one (int-array (mapv num/normalized->uint vec2-doubles-one)))

(defonce ^:private vec3-int-array-zero (int-array vec3-doubles-zero))
(defonce ^:private vec3-int-array-one (int-array vec3-doubles-one))
(defonce ^:private vec3-int-array-normalized-one (int-array (mapv num/normalized->int vec3-doubles-one)))
(defonce ^:private vec3-int-array-normalized-unsigned-one (int-array (mapv num/normalized->uint vec3-doubles-one)))

(defonce ^:private vec4-int-array-zero (int-array vec4-doubles-zero))
(defonce ^:private vec4-int-array-one (int-array vec4-doubles-one))
(defonce ^:private vec4-int-array-normalized-one (int-array (mapv num/normalized->int vec4-doubles-one)))
(defonce ^:private vec4-int-array-normalized-unsigned-one (int-array (mapv num/normalized->uint vec4-doubles-one)))
(defonce ^:private vec4-int-array-xyz-zero-w-one (int-array vec4-doubles-xyz-zero-w-one))
(defonce ^:private vec4-int-array-normalized-xyz-zero-w-one (int-array (mapv num/normalized->int vec4-doubles-xyz-zero-w-one)))
(defonce ^:private vec4-int-array-normalized-unsigned-xyz-zero-w-one (int-array (mapv num/normalized->uint vec4-doubles-xyz-zero-w-one)))

(defonce ^:private mat2-int-array-identity (int-array mat2-doubles-identity))
(defonce ^:private mat2-int-array-normalized-identity (int-array (mapv num/normalized->int mat2-doubles-identity)))
(defonce ^:private mat2-int-array-normalized-unsigned-identity (int-array (mapv num/normalized->uint mat2-doubles-identity)))
(defonce ^:private mat3-int-array-identity (int-array mat3-doubles-identity))
(defonce ^:private mat3-int-array-normalized-identity (int-array (mapv num/normalized->int mat3-doubles-identity)))
(defonce ^:private mat3-int-array-normalized-unsigned-identity (int-array (mapv num/normalized->uint mat3-doubles-identity)))
(defonce ^:private mat4-int-array-identity (int-array mat4-doubles-identity))
(defonce ^:private mat4-int-array-normalized-identity (int-array (mapv num/normalized->int mat4-doubles-identity)))
(defonce ^:private mat4-int-array-normalized-unsigned-identity (int-array (mapv num/normalized->uint mat4-doubles-identity)))

(defn default-attribute-int-array
  ^ints [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-int-array-normalized-one vec1-int-array-one)
                          vec1-int-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-int-array-normalized-one vec2-int-array-one)
                        vec2-int-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-int-array-normalized-one vec3-int-array-one)
                        vec3-int-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-int-array-normalized-xyz-zero-w-one vec4-int-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-int-array-normalized-one vec4-int-array-one)
                        :semantic-type-tangent (if normalize vec4-int-array-normalized-xyz-zero-w-one vec4-int-array-xyz-zero-w-one)
                        vec4-int-array-zero)
    :vector-type-mat2 (if normalize mat2-int-array-normalized-identity mat2-int-array-identity)
    :vector-type-mat3 (if normalize mat3-int-array-normalized-identity mat3-int-array-identity)
    :vector-type-mat4 (if normalize mat4-int-array-normalized-identity mat4-int-array-identity)))

(defn default-attribute-unsigned-int-array
  ^ints [semantic-type vector-type normalize]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color (if normalize vec1-int-array-normalized-unsigned-one vec1-int-array-one)
                          vec1-int-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color (if normalize vec2-int-array-normalized-unsigned-one vec2-int-array-one)
                        vec2-int-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color (if normalize vec3-int-array-normalized-unsigned-one vec3-int-array-one)
                        vec3-int-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position (if normalize vec4-int-array-normalized-unsigned-xyz-zero-w-one vec4-int-array-xyz-zero-w-one)
                        :semantic-type-color (if normalize vec4-int-array-normalized-unsigned-one vec4-int-array-one)
                        :semantic-type-tangent (if normalize vec4-int-array-normalized-unsigned-xyz-zero-w-one vec4-int-array-xyz-zero-w-one)
                        vec4-int-array-zero)
    :vector-type-mat2 (if normalize mat2-int-array-normalized-unsigned-identity mat2-int-array-identity)
    :vector-type-mat3 (if normalize mat3-int-array-normalized-unsigned-identity mat3-int-array-identity)
    :vector-type-mat4 (if normalize mat4-int-array-normalized-unsigned-identity mat4-int-array-identity)))

(defonce ^:private vec1-float-array-zero (float-array vec1-doubles-zero))
(defonce ^:private vec1-float-array-one (float-array vec1-doubles-one))

(defonce ^:private vec2-float-array-zero (float-array vec2-doubles-zero))
(defonce ^:private vec2-float-array-one (float-array vec2-doubles-one))

(defonce ^:private vec3-float-array-zero (float-array vec3-doubles-zero))
(defonce ^:private vec3-float-array-one (float-array vec3-doubles-one))

(defonce ^:private vec4-float-array-zero (float-array vec4-doubles-zero))
(defonce ^:private vec4-float-array-one (float-array vec4-doubles-one))
(defonce ^:private vec4-float-array-xyz-zero-w-one (float-array vec4-doubles-xyz-zero-w-one))

(defonce ^:private mat2-float-array-identity (float-array mat2-doubles-identity))
(defonce ^:private mat3-float-array-identity (float-array mat3-doubles-identity))
(defonce ^:private mat4-float-array-identity (float-array mat4-doubles-identity))

(defn default-attribute-float-array
  ^floats [semantic-type vector-type]
  (case vector-type
    :vector-type-scalar (case semantic-type
                          :semantic-type-color vec1-float-array-one
                          vec1-float-array-zero)
    :vector-type-vec2 (case semantic-type
                        :semantic-type-color vec2-float-array-one
                        vec2-float-array-zero)
    :vector-type-vec3 (case semantic-type
                        :semantic-type-color vec3-float-array-one
                        vec3-float-array-zero)
    :vector-type-vec4 (case semantic-type
                        :semantic-type-position vec4-float-array-xyz-zero-w-one
                        :semantic-type-color vec4-float-array-one
                        :semantic-type-tangent vec4-float-array-xyz-zero-w-one
                        vec4-float-array-zero)
    :vector-type-mat2 mat2-float-array-identity
    :vector-type-mat3 mat3-float-array-identity
    :vector-type-mat4 mat4-float-array-identity))

(defn default-attribute-value-array [semantic-type ^ElementType element-type]
  (case (.-data-type element-type)
    :type-byte (default-attribute-byte-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-unsigned-byte (default-attribute-unsigned-byte-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-short (default-attribute-short-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-unsigned-short (default-attribute-unsigned-short-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-int (default-attribute-int-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-unsigned-int (default-attribute-unsigned-int-array semantic-type (.-vector-type element-type) (.-normalize element-type))
    :type-float (default-attribute-float-array semantic-type (.-vector-type element-type))))

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

(defonce coordinate-spaces (protobuf/valid-enum-values Graphics$CoordinateSpace))

(fn/defamong coordinate-space? coordinate-spaces)

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

(defonce vector-types (protobuf/valid-enum-values Graphics$VertexAttribute$VectorType))

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

(def ^{:arglists '(^ElementType [vector-type data-type normalize])} make-element-type (fn/memoize make-element-type-raw))

(defn attribute-name-key [^String attribute-name]
  (protobuf/field-name->key attribute-name))

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
    (:texcoord :texcoord0) :semantic-type-texcoord
    :page-index :semantic-type-page-index
    :blend-indices :semantic-type-blend-indices
    :blend-weights :semantic-type-blend-weights
    :mtx-world :semantic-type-world-matrix
    :mtx-normal :semantic-type-normal-matrix
    :semantic-type-none))

(defn infer-normalize
  [semantic-type data-type]
  (and (not= :type-float data-type)
       (case semantic-type
         (:semantic-type-bone-indices :semantic-type-none :semantic-type-page-index) false
         true)))

(defn infer-semantic-type->locations [attribute-key+location-pairs]
  (->> attribute-key+location-pairs
       (sort-by val)
       (util/group-into
         {} (vector-of :int)
         #(infer-semantic-type (key %))
         val)))

(defn infer-location->element-type [attribute-reflection-infos]
  (coll/transfer attribute-reflection-infos {}
    (map (fn [{:keys [location data-type name-key vector-type]}]
           (let [semantic-type (infer-semantic-type name-key)
                 normalize (infer-normalize semantic-type data-type)
                 element-type (make-element-type vector-type data-type normalize)]
             (pair location element-type))))))

(defn element-types-by-location? [value]
  (and (map? value)
       (coll/every? (fn [entry]
                      (and (location? (key entry))
                           (element-type? (val entry))))
                    value)))

(defn location-vectors-by-semantic-type? [value]
  (and (map? value)
       (coll/every? (fn [entry]
                      (and (semantic-type? (key entry))
                           (location-vector? (val entry))))
                    value)))

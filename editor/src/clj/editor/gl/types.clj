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

(ns editor.gl.types
  (:require [editor.graphics.types :as graphics.types]
            [util.coll :refer [pair]]
            [util.defonce :as defonce])
  (:import [com.jogamp.opengl GL2]
           [editor.buffers BufferData]
           [editor.graphics.types ElementType]
           [java.nio Buffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/protocol GLBinding
  (bind! [this gl render-args] "Bind this object to the GPU context.")
  (unbind! [this gl render-args] "Unbind this object from the GPU context."))

(def
  ^{:doc "Returns true if the value satisfies the GLBinding protocol."
    :arglists '([value])}
  gl-binding?
  ;; This is quite a bit faster than the satisfies? function.
  (let [gl-binding-class (:on-interface GLBinding)]
    (fn gl-binding? [value]
      (instance? gl-binding-class value))))

(defn data-type-gl-type
  ^long [data-type]
  (case data-type
    :type-byte GL2/GL_BYTE
    :type-unsigned-byte GL2/GL_UNSIGNED_BYTE
    :type-short GL2/GL_SHORT
    :type-unsigned-short GL2/GL_UNSIGNED_SHORT
    :type-int GL2/GL_INT
    :type-unsigned-int GL2/GL_UNSIGNED_INT
    :type-float GL2/GL_FLOAT))

(defn gl-compatible-shader-type? [value]
  ;; Enum values for compute shaders exist in Protobuf but are not supported by
  ;; the OpenGL profile we use in the editor. Uncomment if we update.
  (case value
    (:shader-type-vertex :shader-type-fragment #_ :shader-type-compute) true
    (do (assert (graphics.types/shader-type? value))
        false)))

(defn shader-type-gl-type
  ^long [shader-type]
  ;; Enum values for compute shaders exist in Protobuf but are not supported by
  ;; the OpenGL profile we use in the editor. Uncomment if we update.
  (case shader-type
    :shader-type-vertex GL2/GL_VERTEX_SHADER
    :shader-type-fragment GL2/GL_FRAGMENT_SHADER
    #_#_ :shader-type-compute GL3/GL_COMPUTE_SHADER))

(defn gl-attribute-type-vector-type+data-type [^long gl-attribute-type]
  {:post [(graphics.types/vector-type? (key %))
          (graphics.types/data-type? (val %))]}
  (condp = gl-attribute-type
    GL2/GL_FLOAT (pair :vector-type-scalar :type-float)
    GL2/GL_FLOAT_VEC2 (pair :vector-type-vec2 :type-float)
    GL2/GL_FLOAT_VEC3 (pair :vector-type-vec3 :type-float)
    GL2/GL_FLOAT_VEC4 (pair :vector-type-vec4 :type-float)
    GL2/GL_FLOAT_MAT2 (pair :vector-type-mat2 :type-float)
    GL2/GL_FLOAT_MAT3 (pair :vector-type-mat3 :type-float)
    GL2/GL_FLOAT_MAT4 (pair :vector-type-mat4 :type-float)
    GL2/GL_INT (pair :vector-type-scalar :type-int)
    GL2/GL_INT_VEC2 (pair :vector-type-vec2 :type-int)
    GL2/GL_INT_VEC3 (pair :vector-type-vec3 :type-int)
    GL2/GL_INT_VEC4 (pair :vector-type-vec4 :type-int)
    GL2/GL_UNSIGNED_INT (pair :vector-type-scalar :type-unsigned-int)
    GL2/GL_UNSIGNED_INT_VEC2 (pair :vector-type-vec2 :type-unsigned-int)
    GL2/GL_UNSIGNED_INT_VEC3 (pair :vector-type-vec3 :type-unsigned-int)
    GL2/GL_UNSIGNED_INT_VEC4 (pair :vector-type-vec4 :type-unsigned-int)))

(defn gl-uniform-type-uniform-type [^long gl-uniform-type]
  {:post [(graphics.types/uniform-type? %)]}
  (condp = gl-uniform-type
    GL2/GL_FLOAT :uniform-type-float
    GL2/GL_FLOAT_VEC2 :uniform-type-float-vec2
    GL2/GL_FLOAT_VEC3 :uniform-type-float-vec3
    GL2/GL_FLOAT_VEC4 :uniform-type-float-vec4
    GL2/GL_FLOAT_MAT2 :uniform-type-float-mat2
    GL2/GL_FLOAT_MAT3 :uniform-type-float-mat3
    GL2/GL_FLOAT_MAT4 :uniform-type-float-mat4
    GL2/GL_INT :uniform-type-int
    GL2/GL_INT_VEC2 :uniform-type-int-vec2
    GL2/GL_INT_VEC3 :uniform-type-int-vec3
    GL2/GL_INT_VEC4 :uniform-type-int-vec4
    GL2/GL_BOOL :uniform-type-bool
    GL2/GL_BOOL_VEC2 :uniform-type-bool-vec2
    GL2/GL_BOOL_VEC3 :uniform-type-bool-vec3
    GL2/GL_BOOL_VEC4 :uniform-type-bool-vec4
    GL2/GL_SAMPLER_2D :uniform-type-sampler-2d
    GL2/GL_SAMPLER_CUBE :uniform-type-sampler-cube))

(defn element-type-gl-type
  ^long [^ElementType element-type]
  (data-type-gl-type (.-data-type element-type)))

(defn element-buffer-gl-type
  ^long [element-buffer]
  (-> element-buffer graphics.types/element-type element-type-gl-type))

(defn usage-gl-usage
  ^long [usage]
  (case usage
    :dynamic GL2/GL_DYNAMIC_DRAW
    :static GL2/GL_STATIC_DRAW
    :stream GL2/GL_STREAM_DRAW))

(defn gl-compatible-buffer? [^Buffer value]
  (and (instance? Buffer value)
       (or (.isDirect value)
           (.hasArray value))))

(defn gl-compatible-buffer-data? [^BufferData value]
  (and (instance? BufferData value)
       (gl-compatible-buffer? (.-data value))))

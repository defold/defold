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

(ns editor.gl.attribute
  (:require [editor.gl :as gl]
            [editor.gl.buffer :as gl.buffer]
            [editor.gl.protocols :refer [GlBind]]
            [editor.graphics.types :as types]
            [util.defonce :as defonce]
            [util.ensure :as ensure])
  (:import [com.jogamp.opengl GL2]
           [editor.buffers BufferData]
           [editor.graphics.types ElementType]
           [javax.vecmath Matrix4d Tuple4d]
           [java.nio Buffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- clear-attribute!
  [shader-vector-type ^GL2 gl ^long base-location]
  (let [attribute-count (types/vector-type-attribute-count shader-vector-type)]
    (gl/clear-attributes! gl base-location attribute-count)))

(defn- assign-attribute-from-buffer!
  [^Buffer buffer ^ElementType buffer-element-type ^GL2 gl ^long base-location]
  ;; TODO!
  )

(defn- assign-attribute-from-floats!
  [^floats floats ^ElementType shader-element-type ^GL2 gl ^long base-location]
  ;; TODO(instancing): Should we deprecate the ability to do this? Supporting
  ;; Tuple4d and Matrix4d should suffice, right?
  (types/ensure-floating-point-target-element-type shader-element-type)
  (case (.-vector-type shader-element-type)
    :vector-type-scalar
    (.glVertexAttrib1fv gl base-location floats 0)

    :vector-type-vec2
    (.glVertexAttrib2fv gl base-location floats 0)

    :vector-type-vec3
    (.glVertexAttrib3fv gl base-location floats 0)

    :vector-type-vec4
    (.glVertexAttrib4fv gl base-location floats 0)

    :vector-type-mat2
    (do (.glVertexAttrib4f gl (+ base-location 0) (aget floats 0) (aget floats 1) 0.0 0.0)
        (.glVertexAttrib4f gl (+ base-location 1) (aget floats 2) (aget floats 3) 0.0 0.0))

    :vector-type-mat3
    (do (.glVertexAttrib4f gl (+ base-location 0) (aget floats 0) (aget floats 1) (aget floats 2) 0.0)
        (.glVertexAttrib4f gl (+ base-location 1) (aget floats 3) (aget floats 4) (aget floats 5) 0.0)
        (.glVertexAttrib4f gl (+ base-location 2) (aget floats 6) (aget floats 7) (aget floats 8) 0.0))

    :vector-type-mat4
    (do (.glVertexAttrib4f gl (+ base-location 0) (aget floats 0) (aget floats 1) (aget floats 2) (aget floats 3))
        (.glVertexAttrib4f gl (+ base-location 1) (aget floats 4) (aget floats 5) (aget floats 6) (aget floats 7))
        (.glVertexAttrib4f gl (+ base-location 2) (aget floats 8) (aget floats 9) (aget floats 10) (aget floats 11))
        (.glVertexAttrib4f gl (+ base-location 3) (aget floats 12) (aget floats 13) (aget floats 14) (aget floats 15)))))

(defn- assign-attribute-from-matrix4d!
  [^Matrix4d matrix ^ElementType shader-element-type ^GL2 gl ^long base-location]
  (types/ensure-floating-point-target-element-type shader-element-type)
  (case (.-vector-type shader-element-type)
    (:vector-type-mat2)
    (do (.glVertexAttrib4f gl (+ base-location 0) (.m00 matrix) (.m10 matrix) 0.0 0.0)
        (.glVertexAttrib4f gl (+ base-location 1) (.m01 matrix) (.m11 matrix) 0.0 0.0))

    (:vector-type-mat3)
    (do (.glVertexAttrib4f gl (+ base-location 0) (.m00 matrix) (.m10 matrix) (.m20 matrix) 0.0)
        (.glVertexAttrib4f gl (+ base-location 1) (.m01 matrix) (.m11 matrix) (.m21 matrix) 0.0)
        (.glVertexAttrib4f gl (+ base-location 2) (.m02 matrix) (.m12 matrix) (.m22 matrix) 0.0))

    (:vector-type-mat4)
    (do (.glVertexAttrib4f gl (+ base-location 0) (.m00 matrix) (.m10 matrix) (.m20 matrix) (.m30 matrix))
        (.glVertexAttrib4f gl (+ base-location 1) (.m01 matrix) (.m11 matrix) (.m21 matrix) (.m31 matrix))
        (.glVertexAttrib4f gl (+ base-location 2) (.m02 matrix) (.m12 matrix) (.m22 matrix) (.m32 matrix))
        (.glVertexAttrib4f gl (+ base-location 3) (.m03 matrix) (.m13 matrix) (.m23 matrix) (.m33 matrix)))

    ;; I guess just write the first four elements to vectors?
    (:vector-type-scalar :vector-type-vec2 :vector-type-vec3 :vector-type-vec4)
    (.glVertexAttrib4f gl base-location (.m00 matrix) (.m10 matrix) (.m20 matrix) (.m30 matrix))))

(defn- assign-attribute-from-tuple4d!
  [^Tuple4d tuple ^ElementType shader-element-type ^GL2 gl ^long base-location]
  (types/ensure-floating-point-target-element-type shader-element-type)
  (.glVertexAttrib4f gl base-location (.x tuple) (.y tuple) (.z tuple) (.w tuple)))

(defn- assign-attribute-from-value!
  [value ^ElementType shader-element-type ^GL2 gl ^long base-location]
  (if (nil? value)
    (clear-attribute! shader-element-type gl base-location)
    (condp instance? value
      float/1 (assign-attribute-from-floats! value shader-element-type gl base-location)
      Tuple4d (assign-attribute-from-tuple4d! value shader-element-type gl base-location)
      Matrix4d (assign-attribute-from-matrix4d! value shader-element-type gl base-location)
      (throw
        (ex-info
          (str "Unsupported value: " value)
          {:value value
           :shader-element-type shader-element-type
           :base-location base-location})))))

(defonce/record AttributeRenderArgBinding
  [render-arg-key
   ^ElementType shader-element-type
   ^int base-location]

  GlBind
  (bind [this gl render-args]
    (let [render-arg-value (get render-args render-arg-key ::not-found)]
      (if (= ::not-found render-arg-value)
        (throw
          (ex-info
            (str "Key not found in render-args: " render-arg-key)
            (assoc this :render-args render-args)))
        (assign-attribute-from-value! render-arg-value shader-element-type gl base-location))))

  (unbind [_this gl _render-args]
    (clear-attribute! shader-vector-type gl base-location)))

(defonce/record AttributeValueBinding
  [^BufferData value-buffer-data
   ^ElementType buffer-element-type
   ^int base-location]

  GlBind
  (bind [_this gl _render-args]
    (let [buffer (.-data attribute-value)]
      (assign-attribute-from-buffer! buffer buffer-element-type gl base-location)))

  (unbind [_this gl _render-args]
    (clear-attribute! shader-vector-type gl base-location)))

(defonce/record AttributeBufferBinding
  [attribute-buffer-lifecycle
   ^int base-location]

  GlBind
  (bind [_this gl render-args]
    (gl/bind gl attribute-buffer-lifecycle render-args)
    (gl.buffer/assign-attribute! attribute-buffer-lifecycle gl base-location))

  (unbind [_this gl render-args]
    (gl.buffer/clear-attribute! attribute-buffer-lifecycle gl base-location)
    (gl/unbind gl attribute-buffer-lifecycle render-args)))

(defn make-attribute-buffer-binding
  ^AttributeBufferBinding [attribute-buffer-lifecycle ^long base-location]
  (ensure/argument attribute-buffer-lifecycle gl.buffer/attribute-buffer? "%s must be an attribute BufferLifecycle")
  (ensure/argument base-location nat-int? "%s must be a non-negative integer")
  (->AttributeBufferBinding attribute-buffer-lifecycle base-location))

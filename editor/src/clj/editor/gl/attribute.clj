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
            [util.array :as array]
            [util.defonce :as defonce]
            [util.ensure :as ensure])
  (:import [com.jogamp.opengl GL2]
           [editor.graphics.types ElementType]
           [javax.vecmath Matrix4d Tuple4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def base-location? nat-int?)

(defn- clear-attribute!
  [vector-type ^GL2 gl ^long base-location]
  (let [attribute-count (types/vector-type-attribute-count vector-type)]
    (gl/clear-attributes! gl base-location attribute-count)))

(defmacro ^:private def-assign-attribute-fn
  [name-sym set-attribute-1-sym set-attribute-2-sym set-attribute-3-sym set-attribute-4-sym]
  `(defn ~name-sym
     [~'value-array ~'vector-type ~(with-meta 'gl {:tag `GL2}) ~(with-meta 'base-location {:tag `long})]
     (case ~'vector-type
       :vector-type-scalar
       (~set-attribute-1-sym ~'gl ~'base-location ~'value-array 0)

       :vector-type-vec2
       (~set-attribute-2-sym ~'gl ~'base-location ~'value-array 0)

       :vector-type-vec3
       (~set-attribute-3-sym ~'gl ~'base-location ~'value-array 0)

       :vector-type-vec4
       (~set-attribute-4-sym ~'gl ~'base-location ~'value-array 0)

       :vector-type-mat2
       (do (~set-attribute-2-sym ~'gl (+ ~'base-location 0) ~'value-array 0)
           (~set-attribute-2-sym ~'gl (+ ~'base-location 1) ~'value-array 2))

       :vector-type-mat3
       (do (~set-attribute-3-sym ~'gl (+ ~'base-location 0) ~'value-array 0)
           (~set-attribute-3-sym ~'gl (+ ~'base-location 1) ~'value-array 3)
           (~set-attribute-3-sym ~'gl (+ ~'base-location 2) ~'value-array 6))

       :vector-type-mat4
       (do (~set-attribute-4-sym ~'gl (+ ~'base-location 0) ~'value-array 0)
           (~set-attribute-4-sym ~'gl (+ ~'base-location 1) ~'value-array 4)
           (~set-attribute-4-sym ~'gl (+ ~'base-location 2) ~'value-array 8)
           (~set-attribute-4-sym ~'gl (+ ~'base-location 3) ~'value-array 12)))))

(def-assign-attribute-fn
  assign-attribute-from-bytes!
  gl/set-attribute-1bv!
  gl/set-attribute-2bv!
  gl/set-attribute-3bv!
  gl/set-attribute-4bv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-bytes!
  gl/set-attribute-1nbv!
  gl/set-attribute-2nbv!
  gl/set-attribute-3nbv!
  gl/set-attribute-4nbv!)

(def-assign-attribute-fn
  assign-attribute-from-unsigned-bytes!
  gl/set-attribute-1ubv!
  gl/set-attribute-2ubv!
  gl/set-attribute-3ubv!
  gl/set-attribute-4ubv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-unsigned-bytes!
  gl/set-attribute-1nubv!
  gl/set-attribute-2nubv!
  gl/set-attribute-3nubv!
  gl/set-attribute-4nubv!)

(def-assign-attribute-fn
  assign-attribute-from-shorts!
  gl/set-attribute-1sv!
  gl/set-attribute-2sv!
  gl/set-attribute-3sv!
  gl/set-attribute-4sv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-shorts!
  gl/set-attribute-1nsv!
  gl/set-attribute-2nsv!
  gl/set-attribute-3nsv!
  gl/set-attribute-4nsv!)

(def-assign-attribute-fn
  assign-attribute-from-unsigned-shorts!
  gl/set-attribute-1usv!
  gl/set-attribute-2usv!
  gl/set-attribute-3usv!
  gl/set-attribute-4usv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-unsigned-shorts!
  gl/set-attribute-1nusv!
  gl/set-attribute-2nusv!
  gl/set-attribute-3nusv!
  gl/set-attribute-4nusv!)

(def-assign-attribute-fn
  assign-attribute-from-ints!
  gl/set-attribute-1iv!
  gl/set-attribute-2iv!
  gl/set-attribute-3iv!
  gl/set-attribute-4iv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-ints!
  gl/set-attribute-1niv!
  gl/set-attribute-2niv!
  gl/set-attribute-3niv!
  gl/set-attribute-4niv!)

(def-assign-attribute-fn
  assign-attribute-from-unsigned-ints!
  gl/set-attribute-1uiv!
  gl/set-attribute-2uiv!
  gl/set-attribute-3uiv!
  gl/set-attribute-4uiv!)

(def-assign-attribute-fn
  assign-attribute-from-normalized-unsigned-ints!
  gl/set-attribute-1nuiv!
  gl/set-attribute-2nuiv!
  gl/set-attribute-3nuiv!
  gl/set-attribute-4nuiv!)

(def-assign-attribute-fn
  assign-attribute-from-floats!
  gl/set-attribute-1fv!
  gl/set-attribute-2fv!
  gl/set-attribute-3fv!
  gl/set-attribute-4fv!)

(defn- assign-attribute-from-array!
  [value-array ^ElementType element-type ^GL2 gl ^long base-location]
  (case (.-data-type element-type)
    :type-byte
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-bytes! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-bytes! value-array (.-vector-type element-type) gl base-location))

    :type-unsigned-byte
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-unsigned-bytes! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-unsigned-bytes! value-array (.-vector-type element-type) gl base-location))

    :type-short
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-shorts! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-shorts! value-array (.-vector-type element-type) gl base-location))

    :type-unsigned-short
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-unsigned-shorts! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-unsigned-shorts! value-array (.-vector-type element-type) gl base-location))

    :type-int
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-ints! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-ints! value-array (.-vector-type element-type) gl base-location))

    :type-unsigned-int
    (if (.-normalize element-type)
      (assign-attribute-from-normalized-unsigned-ints! value-array (.-vector-type element-type) gl base-location)
      (assign-attribute-from-unsigned-ints! value-array (.-vector-type element-type) gl base-location))

    :type-float
    (assign-attribute-from-floats! value-array (.vector-type element-type) gl base-location)))

(defn- assign-attribute-from-matrix4d!
  [^Matrix4d matrix vector-type ^GL2 gl ^long base-location]
  (case vector-type
    (:vector-type-mat2)
    (do (.glVertexAttrib2f gl (+ base-location 0) (.m00 matrix) (.m10 matrix))
        (.glVertexAttrib2f gl (+ base-location 1) (.m01 matrix) (.m11 matrix)))

    (:vector-type-mat3)
    (do (.glVertexAttrib3f gl (+ base-location 0) (.m00 matrix) (.m10 matrix) (.m20 matrix))
        (.glVertexAttrib3f gl (+ base-location 1) (.m01 matrix) (.m11 matrix) (.m21 matrix))
        (.glVertexAttrib3f gl (+ base-location 2) (.m02 matrix) (.m12 matrix) (.m22 matrix)))

    (:vector-type-mat4)
    (do (.glVertexAttrib4f gl (+ base-location 0) (.m00 matrix) (.m10 matrix) (.m20 matrix) (.m30 matrix))
        (.glVertexAttrib4f gl (+ base-location 1) (.m01 matrix) (.m11 matrix) (.m21 matrix) (.m31 matrix))
        (.glVertexAttrib4f gl (+ base-location 2) (.m02 matrix) (.m12 matrix) (.m22 matrix) (.m32 matrix))
        (.glVertexAttrib4f gl (+ base-location 3) (.m03 matrix) (.m13 matrix) (.m23 matrix) (.m33 matrix)))

    ;; I guess just write the first four elements to vectors?
    (:vector-type-scalar :vector-type-vec2 :vector-type-vec3 :vector-type-vec4)
    (.glVertexAttrib4f gl base-location (.m00 matrix) (.m10 matrix) (.m20 matrix) (.m30 matrix))))

(defn- assign-attribute-from-tuple4d!
  [^Tuple4d tuple ^GL2 gl ^long base-location]
  (.glVertexAttrib4f gl base-location (.x tuple) (.y tuple) (.z tuple) (.w tuple)))

(defn- assign-attribute-from-value!
  [value ^ElementType element-type ^GL2 gl ^long base-location]
  (if (nil? value)
    (clear-attribute! (.-vector-type element-type) gl base-location)
    (condp instance? value
      Tuple4d (assign-attribute-from-tuple4d! value gl base-location)
      Matrix4d (assign-attribute-from-matrix4d! value (.-vector-type element-type) gl base-location)
      (assign-attribute-from-array! value element-type gl base-location))))

(defonce/record AttributeRenderArgBinding
  [render-arg-key
   ^ElementType element-type
   ^int base-location]

  GlBind
  (bind [this gl render-args]
    (let [render-arg-value (get render-args render-arg-key ::not-found)]
      (if (= ::not-found render-arg-value)
        (throw
          (ex-info
            (str "Key not found in render-args: " render-arg-key)
            (assoc this :render-args render-args)))
        (assign-attribute-from-value! render-arg-value element-type gl base-location))))

  (unbind [_this gl _render-args]
    (clear-attribute! (.-vector-type element-type) gl base-location)))

(defn make-render-arg-binding
  ^AttributeRenderArgBinding [render-arg-key ^ElementType element-type ^long base-location]
  (ensure/argument render-arg-key keyword? "%s must be a keyword")
  (ensure/argument-type element-type ElementType)
  (ensure/argument base-location base-location? "%s must be a non-negative integer")
  (->AttributeRenderArgBinding render-arg-key element-type base-location))

(defonce/record AttributeValueBinding
  [value-array
   ^ElementType element-type
   ^int base-location]

  GlBind
  (bind [_this gl _render-args]
    (assign-attribute-from-array! value-array element-type gl base-location))

  (unbind [_this gl _render-args]
    (clear-attribute! (.-vector-type element-type) gl base-location)))

(defn value-array? [value]
  (case (array/primitive-type value)
    (:byte :short :int :long :float)
    (case (count value)
      (1 2 3 4 9 16) true
      false)
    false))

(defn make-value-binding
  ^AttributeValueBinding [value-array ^ElementType element-type ^long base-location]
  (ensure/argument value-array value-array?)
  (ensure/argument-type element-type ElementType)
  (ensure/argument base-location base-location? "%s must be a non-negative integer")
  (->AttributeValueBinding value-array element-type base-location))

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

(defn make-buffer-binding
  ^AttributeBufferBinding [attribute-buffer-lifecycle ^long base-location]
  (ensure/argument attribute-buffer-lifecycle gl.buffer/attribute-buffer? "%s must be an attribute BufferLifecycle")
  (ensure/argument base-location base-location? "%s must be a non-negative integer")
  (->AttributeBufferBinding attribute-buffer-lifecycle base-location))

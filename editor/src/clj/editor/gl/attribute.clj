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
  (:require [editor.buffers :as buffers]
            [editor.gl :as gl]
            [editor.gl.types :as gl.types]
            [editor.graphics.types :as graphics.types]
            [editor.math :as math]
            [editor.scene-cache :as scene-cache]
            [util.coll :refer [pair]]
            [util.defonce :as defonce]
            [util.ensure :as ensure]
            [util.fn :as fn])
  (:import [com.jogamp.opengl GL2]
           [editor.buffers BufferData]
           [editor.graphics.types ElementType]
           [java.nio FloatBuffer IntBuffer ShortBuffer]
           [javax.vecmath Matrix4d Matrix4f Quat4d Quat4f Tuple4d Vector4d Vector4f]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- clear-attribute!
  [vector-type ^GL2 gl ^long base-location]
  (let [attribute-count (graphics.types/vector-type-attribute-count vector-type)]
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
    (assign-attribute-from-floats! value-array (.-vector-type element-type) gl base-location)))

(defn- assign-attribute-from-matrix-4d!
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

(defn- assign-attribute-from-tuple-4d!
  [^Tuple4d tuple ^GL2 gl ^long base-location]
  (.glVertexAttrib4f gl base-location (.x tuple) (.y tuple) (.z tuple) (.w tuple)))

(defn- assign-attribute-from-value!
  [value ^ElementType element-type ^GL2 gl ^long base-location]
  (if (nil? value)
    (clear-attribute! (.-vector-type element-type) gl base-location)
    (condp instance? value
      Tuple4d (assign-attribute-from-tuple-4d! value gl base-location)
      Matrix4d (assign-attribute-from-matrix-4d! value (.-vector-type element-type) gl base-location)
      (assign-attribute-from-array! value element-type gl base-location))))

;; -----------------------------------------------------------------------------
;; AttributeRenderArg
;; -----------------------------------------------------------------------------

(defonce/record AttributeRenderArgBinding
  [render-arg-key
   ^ElementType element-type
   ^int base-location]

  gl.types/GLBinding
  (bind! [this gl render-args]
    (let [render-arg-value (get render-args render-arg-key ::not-found)]
      (if (= ::not-found render-arg-value)
        (throw
          (ex-info
            (str "Key not found in render-args: " render-arg-key)
            (assoc this :render-args render-args)))
        (assign-attribute-from-value! render-arg-value element-type gl base-location))))

  (unbind! [_this gl _render-args]
    (clear-attribute! (.-vector-type element-type) gl base-location)))

(defn- make-attribute-render-arg-binding-raw
  ^AttributeRenderArgBinding [render-arg-key ^ElementType element-type ^long base-location]
  (ensure/argument render-arg-key keyword? "%s must be a keyword")
  (ensure/argument-type element-type ElementType)
  (ensure/argument base-location graphics.types/location? "%s must be a non-negative integer")
  (->AttributeRenderArgBinding render-arg-key element-type base-location))

(def ^{:doc
       "Returns a GLBinding that binds a shader attribute location to a value
       associated with a key in the render-args map. The value in render-arg may
       be a primitive array, a Tuple4d, or a Matrix4d, which could be produced
       from one of the keywords in the the `math/render-transform-keys` set."
       :arglists '([render-arg-key ^ElementType element-type base-location])}
  make-attribute-render-arg-binding
  (fn/memoize make-attribute-render-arg-binding-raw))

;; -----------------------------------------------------------------------------
;; AttributeValue
;; -----------------------------------------------------------------------------

(defonce/record AttributeValueBinding
  [value-array
   ^ElementType element-type
   ^int base-location]

  graphics.types/ValueBinding
  (with-value [this new-value]
    (graphics.types/ensure-element-type-value-array element-type new-value)
    (assoc this :value-array new-value))

  gl.types/GLBinding
  (bind! [_this gl _render-args]
    (assign-attribute-from-array! value-array element-type gl base-location))

  (unbind! [_this gl _render-args]
    (clear-attribute! (.-vector-type element-type) gl base-location)))

(defn make-attribute-value-binding
  "Returns a GLBinding that binds a shader attribute location to a primitive
  array value."
  ^AttributeValueBinding [value-array ^ElementType element-type ^long base-location]
  ;; TODO(instancing): Support Tuple4d and Matrix4d values as well?
  (ensure/argument-type element-type ElementType)
  (ensure/argument base-location graphics.types/location? "%s must be a non-negative integer")
  (graphics.types/ensure-element-type-value-array element-type value-array)
  (->AttributeValueBinding value-array element-type base-location))

;; -----------------------------------------------------------------------------
;; TransformedAttributeValue
;; -----------------------------------------------------------------------------

(defn- matrix-transform-vector
  ^Vector4d [^Vector4d untransformed-vector ^Matrix4d transform-matrix ^double w-component]
  (let [transformed-vector (doto (Vector4d. untransformed-vector)
                             (.setW w-component))]
    (.transform transform-matrix transformed-vector)
    (.setW transformed-vector (.getW untransformed-vector))
    transformed-vector))

(defn- quat-transform-vector
  ^Vector4d [^Vector4d untransformed-vector ^Quat4d transform-quat ^double w-component]
  (let [transform-quat-conjugate (doto (Quat4d.) (.conjugate transform-quat))
        transformed-quat (doto (Quat4d.)
                           (.set untransformed-vector)
                           (.setW w-component))]
    (.mul transformed-quat transform-quat transformed-quat)
    (.mul transformed-quat transform-quat-conjugate)
    (doto (Vector4d.)
      (.set transformed-quat)
      (.setW (.getW untransformed-vector)))))

(defonce/record TransformedAttributeValueBinding
  [^Vector4d untransformed-vector
   transform-render-arg-key
   ^double w-component
   ^int base-location]

  graphics.types/ValueBinding
  (with-value [this new-value]
    (ensure/argument-type new-value Vector4d)
    (assoc this :untransformed-vector new-value))

  gl.types/GLBinding
  (bind! [_this gl render-args]
    (let [transform (get render-args transform-render-arg-key)

          transformed-vector
          (condp instance? transform
            Matrix4d (matrix-transform-vector untransformed-vector transform w-component)
            Quat4d (quat-transform-vector untransformed-vector transform w-component)
            (throw
              (ex-info
                (format "render-args value for %s is not a Matrix4d or Quat4d" transform-render-arg-key)
                {:transform-render-arg-key transform-render-arg-key
                 :render-args render-args})))]

      (assign-attribute-from-tuple-4d! transformed-vector gl base-location)))

  (unbind! [_this gl _render-args]
    (clear-attribute! :vector-type-vec4 gl base-location)))

(defn make-transformed-attribute-value-binding
  "Returns a GLBinding that binds a shader attribute location to a vector that
  will be transformed by the Matrix4d or Quat4d associated with the specified
  key in the render-args map. The specified w-component will be used during
  transformation, but the W value in the output vector will be that of the input
  vector."
  ^TransformedAttributeValueBinding [untransformed-vector-or-value-array ^ElementType element-type transform-render-arg-key w-component base-location]
  (ensure/argument math/render-transform-key? transform-render-arg-key)
  (ensure/argument base-location graphics.types/location? "%s must be a non-negative integer")
  (let [w-component (double w-component)

        untransformed-vector
        (if (instance? Vector4d untransformed-vector-or-value-array)
          untransformed-vector-or-value-array
          (doto (Vector4d.)
            (.setW w-component)
            (graphics.types/assign-vector-components! untransformed-vector-or-value-array element-type)))]

    (->TransformedAttributeValueBinding untransformed-vector transform-render-arg-key w-component base-location)))

;; -----------------------------------------------------------------------------
;; AttributeBuffer
;; -----------------------------------------------------------------------------

(defonce/record AttributeBufferData
  [^BufferData buffer-data
   usage])

(defn- make-attribute-buffer-data
  ^AttributeBufferData [^BufferData buffer-data usage]
  (ensure/argument buffer-data gl.types/gl-compatible-buffer-data?)
  (ensure/argument usage graphics.types/usage?)
  (if (instance? FloatBuffer (.-data buffer-data))
    (->AttributeBufferData buffer-data usage)
    (throw (IllegalArgumentException. "buffer-data must use a FloatBuffer"))))

(defonce/record AttributeBufferLifecycle
  [request-id
   ^AttributeBufferData attribute-buffer-data
   ^ElementType element-type]

  graphics.types/ElementBuffer
  (buffer-data [_this] (.-buffer-data attribute-buffer-data))
  (element-type [_this] element-type)

  gl.types/GLBinding
  (bind! [_this gl _render-args]
    (let [gl-buffer (scene-cache/request-object! ::attribute-buffer request-id gl attribute-buffer-data)]
      (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER gl-buffer)
      gl-buffer))

  (unbind! [_this gl _render-args]
    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER 0)))

(defn make-attribute-buffer
  "Creates an attribute buffer from the provided data. The returned object is a
  GLBinding and an ElementBuffer."
  ^AttributeBufferLifecycle [request-id ^BufferData buffer-data vector-type usage]
  (ensure/argument request-id graphics.types/request-id?)
  (let [element-type (graphics.types/make-element-type vector-type :type-float false)
        attribute-buffer-data (make-attribute-buffer-data buffer-data usage)]
    (->AttributeBufferLifecycle request-id attribute-buffer-data element-type)))

(defonce/record AttributeBufferBinding
  [attribute-buffer-lifecycle
   ^int base-location]

  gl.types/GLBinding
  (bind! [_this gl render-args]
    (let [element-type (graphics.types/element-type attribute-buffer-lifecycle)
          vector-type (.-vector-type element-type)
          data-type (.-data-type element-type)
          normalize (.-normalize element-type)
          component-count (graphics.types/vector-type-component-count vector-type)
          item-gl-type (gl.types/data-type-gl-type data-type)
          item-byte-size (graphics.types/data-type-byte-size data-type)
          vertex-byte-size (* component-count item-byte-size)
          row-column-count (graphics.types/vector-type-row-column-count vector-type)]
      (gl/with-gl-bindings gl render-args [attribute-buffer-lifecycle]
        (if (neg? row-column-count)
          (do
            (gl/gl-enable-vertex-attrib-array gl base-location)
            (gl/gl-vertex-attrib-pointer gl base-location component-count item-gl-type normalize vertex-byte-size 0))
          (loop [column-index 0]
            (when (< column-index row-column-count)
              (let [location (+ base-location column-index)
                    byte-offset (* column-index row-column-count item-byte-size)]
                (gl/gl-enable-vertex-attrib-array gl location)
                (gl/gl-vertex-attrib-pointer gl location row-column-count item-gl-type normalize vertex-byte-size byte-offset)
                (recur (inc column-index)))))))))

  (unbind! [_this gl _render-args]
    (let [element-type (graphics.types/element-type attribute-buffer-lifecycle)
          vector-type (.-vector-type element-type)
          attribute-count (graphics.types/vector-type-attribute-count vector-type)]
      (gl/disable-vertex-attrib-arrays! gl base-location attribute-count))))

(defn make-attribute-buffer-binding
  "Returns a GLBinding that binds a shader attribute location to an attribute
  buffer. An attribute buffer can be created using the `make-attribute-buffer`
  or `make-transformed-attribute-buffer` functions."
  ^AttributeBufferBinding [attribute-buffer-lifecycle ^long base-location]
  (ensure/argument-satisfies attribute-buffer-lifecycle graphics.types/ElementBuffer)
  (ensure/argument-satisfies attribute-buffer-lifecycle gl.types/GLBinding)
  (ensure/argument base-location graphics.types/location? "%s must be a non-negative integer")
  (->AttributeBufferBinding attribute-buffer-lifecycle base-location))

(defn- update-attribute-buffer!
  [^GL2 gl ^long gl-buffer ^AttributeBufferData attribute-buffer-data]
  (let [gl-usage (gl.types/usage-gl-usage (.-usage attribute-buffer-data))
        buffer-data ^BufferData (.-buffer-data attribute-buffer-data)
        data (.-data buffer-data)
        data-byte-size (buffers/total-byte-size data)]
    (assert (buffers/flipped? data) "data Buffer must be flipped before use")
    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER gl-buffer)
    (gl/gl-buffer-data gl GL2/GL_ARRAY_BUFFER data-byte-size data gl-usage)
    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER 0)
    gl-buffer))

(defn- create-attribute-buffer!
  [^GL2 gl ^AttributeBufferData attribute-buffer-data]
  (let [gl-buffer (gl/gl-gen-buffer gl)
        gl-buffer (update-attribute-buffer! gl gl-buffer attribute-buffer-data)]
    gl-buffer))

(defn- destroy-attribute-buffers!
  [^GL2 gl gl-buffers _attribute-buffer-datas]
  (gl/gl-delete-buffers gl gl-buffers))

(scene-cache/register-object-cache!
  ::attribute-buffer
  create-attribute-buffer!
  update-attribute-buffer!
  destroy-attribute-buffers!)

;; -----------------------------------------------------------------------------
;; TransformedAttributeBuffer
;; -----------------------------------------------------------------------------

(def ^:private transformed-attribute-buffer-element-type
  (graphics.types/make-element-type :vector-type-vec4 :type-float false))

(defonce/record TransformedAttributeBufferData
  [^BufferData untransformed-buffer-data
   transform
   ^float w-component])

(defonce/record TransformedAttributeBufferLifecycle
  [request-id
   ^BufferData untransformed-buffer-data
   transform-render-arg-key
   ^float w-component]

  graphics.types/ElementBuffer
  (buffer-data [_this] untransformed-buffer-data)
  (element-type [_this] transformed-attribute-buffer-element-type)

  gl.types/GLBinding
  (bind! [_this gl render-args]
    (let [transform (get render-args transform-render-arg-key)]
      (if (or (instance? Matrix4d transform)
              (instance? Quat4d transform))
        (let [transformed-attribute-buffer-data (->TransformedAttributeBufferData untransformed-buffer-data transform w-component)
              [gl-buffer] (scene-cache/request-object! ::transformed-attribute-buffer request-id gl transformed-attribute-buffer-data)]
          (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER gl-buffer)
          gl-buffer)
        (throw
          (ex-info
            (format "render-args value for %s is not a Matrix4d or Quat4d" transform-render-arg-key)
            {:transform-render-arg-key transform-render-arg-key
             :render-args render-args})))))

  (unbind! [_this gl _render-args]
    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER 0)))

(defn make-transformed-attribute-buffer
  "Creates an attribute buffer that will apply a render-arg transform to the
  provided data. You should create individual transformed attribute buffers for
  each renderable so the managed GPU-side buffer can be reused between frames.
  The GPU-side buffer will be updated whenever the Matrix4d or Quat4d associated
  with the specified render-arg key differs for the request-id. The specified
  w-component will be used during transformation, but the W value in the output
  vectors will be copied from the input vectors. The returned object is a
  GLBinding and an ElementBuffer."
  ^TransformedAttributeBufferLifecycle [request-id ^BufferData untransformed-buffer-data transform-render-arg-key w-component]
  (ensure/argument request-id graphics.types/request-id?)
  (ensure/argument transform-render-arg-key math/render-transform-key?)
  (ensure/argument untransformed-buffer-data gl.types/gl-compatible-buffer-data?)
  (ensure/argument w-component float?)
  (let [data (.-data untransformed-buffer-data)]
    (if (and (instance? FloatBuffer data)
             (zero? (rem (buffers/item-count data) 4)))
      (->TransformedAttributeBufferLifecycle request-id untransformed-buffer-data transform-render-arg-key w-component)
      (throw (IllegalArgumentException. "untransformed-buffer-data must use a FloatBuffer of tightly-packed four-element vectors")))))

(defn- matrix-transform-into!
  ^FloatBuffer [^FloatBuffer target ^FloatBuffer source ^Matrix4d transform-matrix-4d w-component]
  (assert (buffers/flipped? target) "target Buffer must be flipped before use")
  (assert (buffers/flipped? source) "source Buffer must be flipped before use")
  (let [transform-matrix (Matrix4f. transform-matrix-4d)
        w-component (float w-component)
        float-count (buffers/item-count source)
        _ (assert (zero? (rem float-count 4)))
        temp-floats (float-array 4)
        temp-vector (Vector4f.)]
    (loop [offset 0]
      (when (< offset float-count)
        (.get source offset temp-floats)
        (.set temp-vector (aget temp-floats 0) (aget temp-floats 1) (aget temp-floats 2) w-component)
        (.transform transform-matrix temp-vector)
        (aset temp-floats 0 (.-x temp-vector))
        (aset temp-floats 1 (.-y temp-vector))
        (aset temp-floats 2 (.-z temp-vector))
        (.put target offset temp-floats)
        (recur (+ offset 4))))
    (assert (buffers/flipped? target))
    target))

(defn- quat-transform-into!
  ^FloatBuffer [^FloatBuffer target ^FloatBuffer source ^Quat4d transform-quat-4d ^double w-component]
  (assert (buffers/flipped? target) "target Buffer must be flipped before use")
  (assert (buffers/flipped? source) "source Buffer must be flipped before use")
  (let [transform-quat (Quat4f. transform-quat-4d)
        transform-quat-conjugate (doto (Quat4f.) (.conjugate transform-quat))
        w-component (float w-component)
        float-count (buffers/item-count source)
        _ (assert (zero? (rem float-count 4)))
        temp-floats (float-array 4)
        temp-quat (Quat4f.)]
    (loop [offset 0]
      (when (< offset float-count)
        (.get source offset temp-floats)
        (.set temp-quat (aget temp-floats 0) (aget temp-floats 1) (aget temp-floats 2) w-component)
        (.mul temp-quat transform-quat temp-quat)
        (.mul temp-quat transform-quat-conjugate)
        (aset temp-floats 0 (.-x temp-quat))
        (aset temp-floats 1 (.-y temp-quat))
        (aset temp-floats 2 (.-z temp-quat))
        (.put target offset temp-floats)
        (recur (+ offset 4))))
    (assert (buffers/flipped? target))
    target))

(defn- update-transformed-attribute-buffer!
  [^GL2 gl [gl-buffer ^FloatBuffer transformed-data] ^TransformedAttributeBufferData transformed-attribute-buffer-data]
  (let [untransformed-buffer-data ^BufferData (.-untransformed-buffer-data transformed-attribute-buffer-data)
        untransformed-data (.data untransformed-buffer-data)
        _ (assert (instance? FloatBuffer untransformed-data))
        _ (assert (buffers/flipped? untransformed-data) "untransformed-data Buffer must be flipped before use")
        float-count (buffers/item-count untransformed-data)
        transform (.-transform transformed-attribute-buffer-data)
        w-component (.-w-component transformed-attribute-buffer-data)

        buffer
        (or (when (some? transformed-data)
              (assert (instance? FloatBuffer transformed-data))
              (assert (buffers/flipped? transformed-data))
              (when (= float-count (buffers/item-count transformed-data))
                transformed-data))
            (buffers/new-float-buffer float-count :byte-order/native))

        data-byte-size (buffers/total-byte-size buffer)

        transformed-data
        (condp instance? transform
          Matrix4d (matrix-transform-into! buffer untransformed-data transform w-component)
          Quat4d (quat-transform-into! buffer untransformed-data transform w-component))]

    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER gl-buffer)
    (gl/gl-buffer-data gl GL2/GL_ARRAY_BUFFER data-byte-size transformed-data GL2/GL_STATIC_DRAW)
    (gl/gl-bind-buffer gl GL2/GL_ARRAY_BUFFER 0)
    (pair gl-buffer transformed-data)))

(defn- create-transformed-attribute-buffer!
  [^GL2 gl ^TransformedAttributeBufferData transformed-attribute-buffer-data]
  (let [gl-buffer (gl/gl-gen-buffer gl)
        gl-buffer+transformed-data (update-transformed-attribute-buffer! gl (pair gl-buffer nil) transformed-attribute-buffer-data)]
    gl-buffer+transformed-data))

(defn- destroy-transformed-attribute-buffers!
  [^GL2 gl gl-buffer+transformed-data-pairs _transformed-attribute-buffer-datas]
  (let [gl-buffers (mapv first gl-buffer+transformed-data-pairs)]
    (gl/gl-delete-buffers gl gl-buffers)))

(scene-cache/register-object-cache!
  ::transformed-attribute-buffer
  create-transformed-attribute-buffer!
  update-transformed-attribute-buffer!
  destroy-transformed-attribute-buffers!)

;; -----------------------------------------------------------------------------
;; IndexBuffer
;; -----------------------------------------------------------------------------

(defonce/record IndexBufferData
  [^BufferData buffer-data
   usage])

(defonce/record IndexBufferLifecycle
  [request-id
   ^IndexBufferData index-buffer-data
   ^ElementType element-type]

  graphics.types/ElementBuffer
  (buffer-data [_this] (.-buffer-data index-buffer-data))
  (element-type [_this] element-type)

  gl.types/GLBinding
  (bind! [_this gl _render-args]
    (let [gl-buffer (scene-cache/request-object! ::index-buffer request-id gl index-buffer-data)]
      (gl/gl-bind-buffer gl GL2/GL_ELEMENT_ARRAY_BUFFER gl-buffer)
      gl-buffer))

  (unbind! [_this gl _render-args]
    (gl/gl-bind-buffer gl GL2/GL_ELEMENT_ARRAY_BUFFER 0)))

(defn make-index-buffer
  "Creates an index buffer from the indices in the provided data. The
  buffer-data must use either a ShortBuffer or an IntBuffer. The returned object
  is a GLBinding and an ElementBuffer."
  ^IndexBufferLifecycle [request-id ^BufferData buffer-data usage]
  (ensure/argument request-id graphics.types/request-id?)
  (ensure/argument buffer-data gl.types/gl-compatible-buffer-data?)
  (ensure/argument usage graphics.types/usage?)
  (let [data (.-data buffer-data)
        data-type (condp instance? data
                    ShortBuffer :type-unsigned-short
                    IntBuffer :type-unsigned-int
                    (throw (IllegalArgumentException. "buffer-data must use either a ShortBuffer or an IntBuffer")))
        element-type (graphics.types/make-element-type :vector-type-scalar data-type false)
        index-buffer-data (->IndexBufferData buffer-data usage)]
    (->IndexBufferLifecycle request-id index-buffer-data element-type)))

(defn- update-index-buffer!
  [^GL2 gl ^long gl-buffer ^IndexBufferData index-buffer-data]
  (let [gl-usage (gl.types/usage-gl-usage (.-usage index-buffer-data))
        buffer-data ^BufferData (.-buffer-data index-buffer-data)
        data (.-data buffer-data)
        data-byte-size (buffers/total-byte-size data)]
    (assert (buffers/flipped? data) "data Buffer must be flipped before use")
    (gl/gl-bind-buffer gl GL2/GL_ELEMENT_ARRAY_BUFFER gl-buffer)
    (gl/gl-buffer-data gl GL2/GL_ELEMENT_ARRAY_BUFFER data-byte-size data gl-usage)
    (gl/gl-bind-buffer gl GL2/GL_ELEMENT_ARRAY_BUFFER 0)
    gl-buffer))

(defn- create-index-buffer!
  [^GL2 gl ^IndexBufferData index-buffer-data]
  (let [gl-buffer (gl/gl-gen-buffer gl)
        gl-buffer (update-index-buffer! gl gl-buffer index-buffer-data)]
    gl-buffer))

(defn- destroy-index-buffers!
  [^GL2 gl gl-buffers _index-buffer-datas]
  (gl/gl-delete-buffers gl gl-buffers))

(scene-cache/register-object-cache!
  ::index-buffer
  create-index-buffer!
  update-index-buffer!
  destroy-index-buffers!)

;; -----------------------------------------------------------------------------
;; Claiming requests
;; -----------------------------------------------------------------------------

(defn- claim-request
  [request new-request-id-fn]
  (let [old-request-id (:request-id request)]
    (if (graphics.types/request-id? old-request-id)
      (let [new-request-id (new-request-id-fn old-request-id)]
        (cond
          (not (graphics.types/request-id? new-request-id))
          (throw
            (ex-info
              "new-request-id-fn must return a valid request-id"
              {:old-request-id old-request-id
               :new-request-id new-request-id}))

          (= old-request-id new-request-id)
          (throw
            (ex-info
              "new-request-id-fn must return a new request-id that differs from the old one"
              {:old-request-id old-request-id
               :new-request-id new-request-id}))

          :else
          (assoc request :request-id new-request-id)))
      (throw
        (ex-info
          "request must be an Associative with a valid :request-id"
          {:request request})))))

(defn- transformed-attribute-buffer-binding-claim-fn
  [attribute-binding new-request-id-fn]
  (if (instance? AttributeBufferBinding attribute-binding)
    (let [attribute-buffer-lifecycle (.-attribute-buffer-lifecycle ^AttributeBufferBinding attribute-binding)]
      (if (instance? TransformedAttributeBufferLifecycle attribute-buffer-lifecycle)
        (let [claimed-attribute-buffer-lifecycle (claim-request attribute-buffer-lifecycle new-request-id-fn)]
          (assoc attribute-binding :attribute-buffer-lifecycle claimed-attribute-buffer-lifecycle))
        attribute-binding))
    attribute-binding))

(defn- claim-bindings
  [bindings claim-fn new-request-id-fn & args]
  (let [new-request-id-fn #(apply new-request-id-fn % args)]
    (reduce-kv
      (fn [bindings key binding]
        (let [claimed-binding (claim-fn binding new-request-id-fn)]
          (if (identical? binding claimed-binding)
            bindings
            (assoc bindings key claimed-binding))))
      bindings
      bindings)))

(defn claim-transformed-attribute-buffer-bindings
  "Takes a collection of bindings (which may be a map) and returns a new
  collection of the same type where all the values that are transformed
  attribute buffers have been assigned new request-ids. The specified
  new-request-id-fn will be called with the old request-id from each transformed
  attribute buffer and is expected to return a new unique request-id."
  [bindings new-request-id-fn & args]
  (apply claim-bindings bindings transformed-attribute-buffer-binding-claim-fn new-request-id-fn args))

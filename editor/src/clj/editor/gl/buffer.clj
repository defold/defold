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

(ns editor.gl.buffer
  (:require [editor.buffers :as buffers]
            [editor.gl :as gl]
            [editor.gl.protocols :refer [GlBind]]
            [editor.graphics.types :as types]
            [editor.scene-cache :as scene-cache]
            [util.defonce :as defonce]
            [util.ensure :as ensure])
  (:import [com.jogamp.opengl GL2]
           [editor.buffers BufferData]
           [editor.graphics.types ElementType]
           [java.nio Buffer FloatBuffer IntBuffer ShortBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record ^:private GlBufferData
  [^BufferData buffer-data
   ^int gl-target
   ^int gl-usage])

(defn- data-type->gl-type
  ^long [data-type]
  (case data-type
    :type-byte GL2/GL_BYTE
    :type-unsigned-byte GL2/GL_UNSIGNED_BYTE
    :type-short GL2/GL_SHORT
    :type-unsigned-short GL2/GL_UNSIGNED_SHORT
    :type-int GL2/GL_INT
    :type-unsigned-int GL2/GL_UNSIGNED_INT
    :type-float GL2/GL_FLOAT))

(defn- target->gl-target
  ^long [target]
  (case target
    :array-buffer GL2/GL_ARRAY_BUFFER
    :element-array-buffer GL2/GL_ELEMENT_ARRAY_BUFFER))

(defn- usage->gl-usage
  ^long [usage]
  (case usage
    :static GL2/GL_STATIC_DRAW
    :dynamic GL2/GL_DYNAMIC_DRAW))

(defn- make-gl-buffer-data
  ^GlBufferData [^Buffer data target usage]
  (assert (or (.isDirect data) (.hasArray data)) "The Buffer must either be direct or backed by an array.")
  (let [buffer-data (buffers/make-buffer-data data)
        gl-target (target->gl-target target)
        gl-usage (usage->gl-usage usage)]
    (->GlBufferData buffer-data gl-target gl-usage)))

(defn- invalidate-gl-buffer-data
  ^GlBufferData [^GlBufferData gl-buffer-data]
  (->GlBufferData
    (buffers/invalidate-buffer-data (.-buffer-data gl-buffer-data))
    (.-gl-target gl-buffer-data)
    (.-gl-usage gl-buffer-data)))

(defn- request-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData gl-buffer-data]
  (let [gl-buffer (scene-cache/request-object! ::gl-buffer request-id gl gl-buffer-data)]
    gl-buffer))

(defn- bind-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData gl-buffer-data]
  (let [gl-target (.-gl-target gl-buffer-data)
        gl-buffer (request-gl-buffer! gl request-id gl-buffer-data)]
    (gl/gl-bind-buffer gl gl-target gl-buffer)
    gl-buffer))

(defn- unbind-gl-buffer!
  [^GL2 gl ^GlBufferData gl-buffer-data]
  (let [gl-target (.-gl-target gl-buffer-data)]
    (gl/gl-bind-buffer gl gl-target 0)))

(defonce/record BufferLifecycle
  [request-id
   ^GlBufferData gl-buffer-data
   ^ElementType buffer-element-type]

  GlBind
  (bind [_this gl _render-args]
    (bind-gl-buffer! gl request-id gl-buffer-data)
    nil)

  (unbind [_this gl _render-args]
    (unbind-gl-buffer! gl gl-buffer-data)))

(defn- make-buffer
  ^BufferLifecycle [request-id ^Buffer data ^ElementType buffer-element-type target usage]
  (ensure/argument request-id types/request-id?)
  (ensure/argument buffer-element-type types/element-type?)
  (let [gl-buffer-data (make-gl-buffer-data data target usage)]
    (->BufferLifecycle request-id gl-buffer-data buffer-element-type)))

(defn make-attribute-buffer
  ^BufferLifecycle [request-id ^FloatBuffer data vector-type usage]
  (if (instance? FloatBuffer data)
    (let [buffer-element-type (types/make-element-type vector-type :type-float false)]
      (make-buffer request-id data buffer-element-type :array-buffer usage))
    (throw (IllegalArgumentException. "data must be a FloatBuffer"))))

(defn attribute-buffer? [^BufferLifecycle value]
  (and (instance? BufferLifecycle value)
       (= GL2/GL_ARRAY_BUFFER (.-gl-target ^GlBufferData (.-gl-buffer-data value)))))

(defn make-index-buffer
  ^BufferLifecycle [request-id ^Buffer data usage]
  (let [data-type (condp instance? data
                    ShortBuffer :type-unsigned-short
                    IntBuffer :type-unsigned-int
                    (throw (IllegalArgumentException. "data must be either a ShortBuffer or an IntBuffer")))
        buffer-element-type (types/make-element-type :vector-type-scalar data-type false)]
    (make-buffer request-id data buffer-element-type :element-array-buffer usage)))

(defn index-buffer? [^BufferLifecycle value]
  (and (instance? BufferLifecycle value)
       (= GL2/GL_ELEMENT_ARRAY_BUFFER (.-gl-target ^GlBufferData (.-gl-buffer-data value)))))

(defn data
  ^Buffer [^BufferLifecycle buffer-lifecycle]
  (.-data ^BufferData (.-buffer-data ^GlBufferData (.-gl-buffer-data buffer-lifecycle))))

(defn invalidate
  ^BufferLifecycle [^BufferLifecycle buffer-lifecycle]
  (->BufferLifecycle (.-request-id buffer-lifecycle)
                     (invalidate-gl-buffer-data (.-gl-buffer-data buffer-lifecycle))
                     (.-buffer-element-type buffer-lifecycle)))

(defn vertex-count
  ^long [^BufferLifecycle buffer-lifecycle]
  (if (nil? buffer-lifecycle)
    0
    (let [buffer (data buffer-lifecycle)
          item-count (buffers/item-count buffer)]
      (if (zero? item-count)
        0
        (let [buffer-element-type ^ElementType (.-buffer-element-type buffer-lifecycle)
              vector-type (.-vector-type buffer-element-type)
              component-count (types/vector-type-component-count vector-type)]
          (quot item-count component-count))))))

(defn assign-attribute!
  [^BufferLifecycle buffer-lifecycle ^GL2 gl ^long base-location]
  ;; Note: Assumes both the BufferLifecycle and the ShaderLifecycle are bound.
  (let [buffer-element-type ^ElementType (.-buffer-element-type buffer-lifecycle)
        vector-type (.-vector-type buffer-element-type)
        data-type (.-data-type buffer-element-type)
        normalize (.-normalize buffer-element-type)
        items-per-vertex (types/vector-type-component-count vector-type)
        item-gl-type (data-type->gl-type data-type)
        item-byte-size (types/data-type-byte-size data-type)
        vertex-byte-size (* items-per-vertex item-byte-size)
        row-column-count (types/vector-type-row-column-count vector-type)]
    (if (neg? row-column-count)
      (do
        (gl/gl-enable-vertex-attrib-array gl base-location)
        (gl/gl-vertex-attrib-pointer gl base-location items-per-vertex item-gl-type normalize vertex-byte-size 0))
      (loop [column-index 0]
        (when (< column-index row-column-count)
          (let [location (+ base-location column-index)
                byte-offset (* column-index row-column-count item-byte-size)]
            (gl/gl-enable-vertex-attrib-array gl location)
            (gl/gl-vertex-attrib-pointer gl location row-column-count item-gl-type normalize vertex-byte-size byte-offset)
            (recur (inc column-index))))))))

(defn clear-attribute!
  [^BufferLifecycle buffer-lifecycle ^GL2 gl ^long base-location]
  (let [buffer-element-type ^ElementType (.-buffer-element-type buffer-lifecycle)
        vector-type (.-vector-type buffer-element-type)
        attribute-count (types/vector-type-attribute-count vector-type)]
    (gl/clear-attributes! gl base-location attribute-count)))

(defn- update-gl-buffer!
  [^GL2 gl ^long gl-buffer ^GlBufferData gl-buffer-data]
  (let [gl-target (.-gl-target gl-buffer-data)
        gl-usage (.-gl-usage gl-buffer-data)
        buffer-data ^BufferData (.-buffer-data gl-buffer-data)
        data (.-data buffer-data)
        data-byte-size (buffers/total-byte-size data)]
    (assert (buffers/flipped? data) "data buffer must be flipped before use")
    (gl/gl-bind-buffer gl gl-target gl-buffer)
    (gl/gl-buffer-data gl gl-target data-byte-size data gl-usage)
    gl-buffer))

(defn- create-gl-buffer!
  [^GL2 gl ^GlBufferData gl-buffer-data]
  (let [gl-buffer (gl/gl-gen-buffer gl)
        gl-buffer (update-gl-buffer! gl gl-buffer gl-buffer-data)]
    gl-buffer))

(defn- destroy-gl-buffers!
  [^GL2 gl gl-buffers _gl-buffer-datas]
  (gl/gl-delete-buffers gl gl-buffers))

(scene-cache/register-object-cache! ::gl-buffer create-gl-buffer! update-gl-buffer! destroy-gl-buffers!)

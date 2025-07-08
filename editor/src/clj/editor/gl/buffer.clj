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
            [internal.java :as java]
            [util.defonce :as defonce])
  (:import [clojure.lang IHashEq]
           [com.jogamp.opengl GL2]
           [java.nio Buffer FloatBuffer IntBuffer ShortBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/type ^:private GlBufferData
  [^Buffer data
   ^int data-version
   ^int gl-target
   ^int gl-usage
   ^int partial-hash]

  Comparable
  (compareTo [_this that]
    (let [^GlBufferData that that]
      (java/combine-comparisons
        (Integer/compare data-version (.-data-version that))
        (Integer/compare partial-hash (.-partial-hash that)))))

  IHashEq
  (hasheq [_this]
    (java/combine-hashes partial-hash data-version))

  Object
  (hashCode [this]
    (.hasheq this))
  (equals [this that]
    (or (identical? this that)
        (let [^GlBufferData that that]
          (and (instance? GlBufferData that)
               (= data-version (.-data-version that))
               (= partial-hash (.-partial-hash that)))))))

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

(defn- make-buffer-data
  ^GlBufferData [^Buffer data data-version target usage]
  (cond
    (nil? data)
    (throw (IllegalArgumentException. "data Buffer cannot be nil"))

    (not (buffers/flipped? data))
    (throw (IllegalArgumentException. "data Buffer must be flipped"))

    :else
    (let [gl-target (target->gl-target target)
          gl-usage (usage->gl-usage usage)
          data-topology-hash (buffers/topology-hash data)
          partial-hash (java/combine-hashes gl-target gl-usage data-topology-hash)]
      (->GlBufferData data data-version gl-target gl-usage partial-hash))))

(defn- invalidate-buffer-data
  ^GlBufferData [^GlBufferData buffer-data]
  (->GlBufferData
    (.-data buffer-data)
    (unchecked-inc-int (.-data-version buffer-data))
    (.-gl-target buffer-data)
    (.-gl-usage buffer-data)
    (.-partial-hash buffer-data)))

(defn- request-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData buffer-data]
  (let [gl-buffer (scene-cache/request-object! ::gl-buffer request-id gl buffer-data)]
    gl-buffer))

(defn- bind-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)
        gl-buffer (request-gl-buffer! gl request-id buffer-data)]
    (gl/gl-bind-buffer gl gl-target gl-buffer)
    gl-buffer))

(defn- unbind-gl-buffer!
  [^GL2 gl ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)]
    (gl/gl-bind-buffer gl gl-target 0)))

(defonce/record BufferLifecycle
  [request-id
   ^GlBufferData buffer-data
   vector-type
   data-type
   ^boolean normalize]

  GlBind
  (bind [_this gl _render-args]
    (bind-gl-buffer! gl request-id buffer-data)
    nil)

  (unbind [_this gl _render-args]
    (unbind-gl-buffer! gl buffer-data)))

(defn- make-buffer
  ^BufferLifecycle [request-id ^Buffer data vector-type data-type normalize target usage]
  (cond
    (not (types/request-id? request-id))
    (throw (IllegalArgumentException. (str "Invalid request-id argument: " request-id)))

    (not (types/vector-type? vector-type))
    (throw (IllegalArgumentException. (str "Invalid vector-type argument: " vector-type)))

    (not (types/data-type? data-type))
    (throw (IllegalArgumentException. (str "Invalid data-type argument: " data-type)))

    (not (boolean? normalize))
    (throw (IllegalArgumentException. "normalize must be a boolean"))

    :else
    (let [data-version (System/identityHashCode data)
          buffer-data (make-buffer-data data data-version target usage)]
      (->BufferLifecycle request-id buffer-data vector-type data-type normalize))))

(defn make-attribute-buffer
  ^BufferLifecycle [request-id ^FloatBuffer data vector-type usage]
  (if (instance? FloatBuffer data)
    (make-buffer request-id data vector-type :type-float false :array-buffer usage)
    (throw (IllegalArgumentException. "data must be a FloatBuffer"))))

(defn attribute-buffer? [^BufferLifecycle value]
  (and (instance? BufferLifecycle value)
       (= GL2/GL_ARRAY_BUFFER (.-gl-target ^GlBufferData (.-buffer-data value)))))

(defn make-index-buffer
  ^BufferLifecycle [request-id ^Buffer data usage]
  (let [data-type (condp instance? data
                    ShortBuffer :type-unsigned-short
                    IntBuffer :type-unsigned-int
                    (throw (IllegalArgumentException. "data must be either a ShortBuffer or an IntBuffer")))]
    (make-buffer request-id data :vector-type-scalar data-type false :element-array-buffer usage)))

(defn index-buffer? [^BufferLifecycle value]
  (and (instance? BufferLifecycle value)
       (= GL2/GL_ELEMENT_ARRAY_BUFFER (.-gl-target ^GlBufferData (.-buffer-data value)))))

(defn data
  ^Buffer [^BufferLifecycle buffer-lifecycle]
  (.-data ^GlBufferData (.-buffer-data buffer-lifecycle)))

(defn invalidate
  ^BufferLifecycle [^BufferLifecycle buffer-lifecycle]
  (->BufferLifecycle (.-request-id buffer-lifecycle)
                     (invalidate-buffer-data (.-buffer-data buffer-lifecycle))
                     (.-vector-type buffer-lifecycle)
                     (.-data-type buffer-lifecycle)
                     (.-normalize buffer-lifecycle)))

(defn vertex-count
  ^long [^BufferLifecycle buffer-lifecycle]
  (if (nil? buffer-lifecycle)
    0
    (let [buffer (data buffer-lifecycle)
          item-count (buffers/item-count buffer)]
      (if (zero? item-count)
        0
        (let [vector-type (.-vector-type buffer-lifecycle)
              component-count (types/vector-type-component-count vector-type)]
          (quot item-count component-count))))))

(defn assign-attribute!
  [^BufferLifecycle buffer-lifecycle ^GL2 gl ^long base-location]
  ;; Note: Assumes both the BufferLifecycle and the ShaderLifecycle are bound.
  (let [vector-type (.-vector-type buffer-lifecycle)
        data-type (.-data-type buffer-lifecycle)
        normalize (.-normalize buffer-lifecycle)
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
                byte-offset (* location row-column-count item-byte-size)]
            (gl/gl-enable-vertex-attrib-array gl location)
            (gl/gl-vertex-attrib-pointer gl location row-column-count item-gl-type normalize vertex-byte-size byte-offset)
            (recur (inc location))))))))

(defn clear-attribute!
  [^BufferLifecycle buffer-lifecycle ^GL2 gl ^long base-location]
  (let [vector-type (.-vector-type buffer-lifecycle)
        row-column-count (types/vector-type-row-column-count vector-type)]
    (if (neg? row-column-count)
      (gl/gl-disable-vertex-attrib-array gl base-location)
      (loop [column-index 0]
        (when (< column-index row-column-count)
          (let [location (+ base-location column-index)]
            (gl/gl-disable-vertex-attrib-array gl location)
            (recur (inc location))))))))

(defn- update-gl-buffer!
  [^GL2 gl ^long gl-buffer ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)
        gl-usage (.-gl-usage buffer-data)
        data (.-data buffer-data)
        data-byte-size (buffers/total-byte-size data)]
    (assert (buffers/flipped? data) "data buffer must be flipped before use")
    (gl/gl-bind-buffer gl gl-target gl-buffer)
    (gl/gl-buffer-data gl gl-target data-byte-size data gl-usage)
    gl-buffer))

(defn- create-gl-buffer!
  [^GL2 gl ^GlBufferData buffer-data]
  (let [gl-buffer (gl/gl-gen-buffer gl)
        gl-buffer (update-gl-buffer! gl gl-buffer buffer-data)]
    gl-buffer))

(defn- destroy-gl-buffers!
  [^GL2 gl gl-buffers _buffer-datas]
  (gl/gl-delete-buffers gl gl-buffers))

(scene-cache/register-object-cache! ::gl-buffer create-gl-buffer! update-gl-buffer! destroy-gl-buffers!)

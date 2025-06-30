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
            [editor.scene-cache :as scene-cache]
            [internal.java :as java]
            [util.defonce :as defonce])
  (:import [clojure.lang IHashEq]
           [com.jogamp.opengl GL2]
           [java.nio Buffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/type GlBufferData
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

(defn make-buffer-data
  (^GlBufferData [^Buffer data target usage]
   (make-buffer-data data (System/identityHashCode data) target usage))
  (^GlBufferData [^Buffer data data-version target usage]
   (if (nil? data)
     (throw (IllegalArgumentException. "data cannot be nil"))
     (let [gl-target (target->gl-target target)
           gl-usage (usage->gl-usage usage)
           data-topology-hash (buffers/topology-hash data)
           partial-hash (java/combine-hashes gl-target gl-usage data-topology-hash)]
       (->GlBufferData data data-version gl-target gl-usage partial-hash)))))

(defn invalidate-buffer-data
  ^GlBufferData [^GlBufferData buffer-data]
  (->GlBufferData
    (.-data buffer-data)
    (unchecked-inc-int (.-data-version buffer-data))
    (.-gl-target buffer-data)
    (.-gl-usage buffer-data)
    (.-partial-hash buffer-data)))

(defn request-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData buffer-data]
  (let [gl-buffer (scene-cache/request-object! ::gl-buffer request-id gl buffer-data)]
    gl-buffer))

(defn bind-gl-buffer!
  ^long [^GL2 gl request-id ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)
        gl-buffer (request-gl-buffer! gl request-id buffer-data)]
    (gl/gl-bind-buffer gl gl-target gl-buffer)
    gl-buffer))

(defn unbind-gl-buffer!
  [^GL2 gl ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)]
    (gl/gl-bind-buffer gl gl-target 0)))

(defonce/type BufferBinding
  [request-id
   ^GlBufferData buffer-data]

  GlBind
  (bind [_this gl _render-args]
    (bind-gl-buffer! gl request-id buffer-data)
    nil)

  (unbind [_this gl _render-args]
    (unbind-gl-buffer! gl buffer-data)))

(defn make-buffer-binding
  ^BufferBinding [request-id ^GlBufferData buffer-data]
  {:pre [(instance? IHashEq request-id)
         (instance? GlBufferData buffer-data)]}
  (->BufferBinding request-id buffer-data))

(defn- update-gl-buffer!
  [^GL2 gl ^long gl-buffer ^GlBufferData buffer-data]
  (let [gl-target (.-gl-target buffer-data)
        gl-usage (.-gl-usage buffer-data)
        data ^Buffer (.-data buffer-data)
        data-byte-size (buffers/total-byte-size data)]
    (assert (zero? (.position data)) "data buffer must be flipped before use")
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

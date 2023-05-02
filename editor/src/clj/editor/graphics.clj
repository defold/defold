;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.graphics
  (:require [editor.buffers :as buffers]
            [editor.gl.vertex2 :as vtx]
            [util.murmur :as murmur])
  (:import [java.nio ByteBuffer]))

(def ^:private attribute-data-type-infos
  [{:data-type :type-byte
    :value-keyword :int-values
    :byte-size 1
    :vtx-type :byte}
   {:data-type :type-unsigned-byte
    :value-keyword :uint-values
    :byte-size 1
    :vtx-type :ubyte}
   {:data-type :type-short
    :value-keyword :int-values
    :byte-size 2
    :vtx-type :short}
   {:data-type :type-unsigned-short
    :value-keyword :uint-values
    :byte-size 2
    :vtx-type :ushort}
   {:data-type :type-int
    :value-keyword :int-values
    :byte-size 4
    :vtx-type :int}
   {:data-type :type-unsigned-int
    :value-keyword :uint-values
    :byte-size 4
    :vtx-type :uint}
   {:data-type :type-float
    :value-keyword :float-values
    :byte-size 4
    :vtx-type :float}])

(def attribute-data-type->attribute-value-keyword
  (into {}
        (map (juxt :data-type :value-keyword))
        attribute-data-type-infos))

(defn attribute->values [attribute]
  (let [attribute-value-keyword (attribute-data-type->attribute-value-keyword (:data-type attribute))]
    (get attribute attribute-value-keyword)))

(def attribute-data-type->byte-size
  (into {}
        (map (juxt :data-type :byte-size))
        attribute-data-type-infos))

(def attribute-data-type->vtx-attribute-type
  (into {}
        (map (juxt :data-type :vtx-type))
        attribute-data-type-infos))

(defn attribute->vtx-attribute [attribute]
  {:pre [(map? attribute)]} ; Graphics$VertexAttribute in map format.
  {:name (:name attribute)
   :type (attribute-data-type->vtx-attribute-type (:data-type attribute))
   :components (:element-count attribute)
   :normalized? (:normalize attribute false)})

(defn make-vertex-description [attributes]
  (let [vtx-attributes (mapv attribute->vtx-attribute attributes)]
    (vtx/make-vertex-description nil vtx-attributes)))

(defmulti put-attribute-values!
  (fn [^ByteBuffer bb attribute-values attribute-data-type] attribute-data-type))

(defmethod put-attribute-values! :type-float
  [^ByteBuffer bb attribute-values _]
  (doseq [v attribute-values]
    (.putFloat bb v)))

(defn attribute->byte-data [attribute]
  (let [attribute-data-type (:data-type attribute)
        attribute-values (:v (attribute->values attribute))
        attribute-value-byte-count (* (count attribute-values) (attribute-data-type->byte-size attribute-data-type))
        bb ^ByteBuffer (buffers/little-endian (buffers/new-byte-buffer attribute-value-byte-count))]
    (put-attribute-values! bb attribute-values attribute-data-type)
    (.rewind bb)
    (buffers/byte-pack bb)))

(defn attributes->build-target [attributes]
  (mapv (fn [attr]
          (assoc attr
            :name-hash (murmur/hash64 (:name attr))
            :float-values nil
            :uint-values nil
            :int-values nil
            :byte-values (attribute->byte-data attr)))
        attributes))

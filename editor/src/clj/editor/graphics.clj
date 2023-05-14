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
    :byte-size 1}
   {:data-type :type-unsigned-byte
    :value-keyword :uint-values
    :byte-size 1}
   {:data-type :type-short
    :value-keyword :int-values
    :byte-size 2}
   {:data-type :type-unsigned-short
    :value-keyword :uint-values
    :byte-size 2}
   {:data-type :type-int
    :value-keyword :int-values
    :byte-size 4}
   {:data-type :type-unsigned-int
    :value-keyword :uint-values
    :byte-size 4}
   {:data-type :type-float
    :value-keyword :float-values
    :byte-size 4}])

(def attribute-data-type->attribute-value-keyword
  (into {}
        (map (juxt :data-type :value-keyword))
        attribute-data-type-infos))

(defn attribute->values [attribute]
  (let [attribute-value-keyword (attribute-data-type->attribute-value-keyword (:data-type attribute))]
    (:v (get attribute attribute-value-keyword))))

(def attribute-data-type->byte-size
  (into {}
        (map (juxt :data-type :byte-size))
        attribute-data-type-infos))

(defn attribute->vtx-attribute [attribute]
  {:pre [(map? attribute)]} ; Graphics$VertexAttribute in map format.
  {:name (:name attribute)
   :name-key (:name-key attribute)
   :semantic-type (:semantic-type attribute)
   :type (vtx/attribute-data-type->type (:data-type attribute))
   :components (:element-count attribute)
   :normalize (:normalize attribute false)})

(defn make-vertex-description [attributes]
  (let [vtx-attributes (mapv attribute->vtx-attribute attributes)]
    (vtx/make-vertex-description nil vtx-attributes)))

(defn make-attribute-byte-buffer
  ^ByteBuffer [attribute-data-type attribute-values]
  (let [attribute-value-byte-count (* (count attribute-values) (attribute-data-type->byte-size attribute-data-type))
        vtx-attribute-type (vtx/attribute-data-type->type attribute-data-type)
        ^ByteBuffer byte-buffer (buffers/little-endian (ByteBuffer/allocate attribute-value-byte-count))]
    (vtx/buf-push! byte-buffer vtx-attribute-type false attribute-values)
    (.rewind byte-buffer)))

(defn attribute->byte-buffer
  ^ByteBuffer [attribute]
  (let [attribute-data-type (:data-type attribute)
        attribute-values (attribute->values attribute)]
    (make-attribute-byte-buffer attribute-data-type attribute-values)))

(defn attributes->build-target [attributes]
  (mapv (fn [attr]
          (-> attr
              (dissoc :float-values :uint-values :int-values)
              (assoc :name-hash (murmur/hash64 (:name attr))
                     :binary-values (buffers/byte-pack (attribute->byte-buffer attr)))))
        attributes))

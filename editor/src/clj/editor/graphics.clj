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
  (:require
    [dynamo.graph :as g]
    [editor.buffers :as buffers]
    [util.murmur :as murmur])
  (:import [java.nio ByteBuffer]))

(defn attribute-data-type->attribute-value-keyword [data-type]
  (case data-type
    :type-byte           :byte-values
    :type-unsigned-byte  :uint-values
    :type-short          :int-values
    :type-unsigned-short :uint-values
    :type-int            :int-values
    :type-unsigned-int   :uint-values
    :type-float          :float-values))

(defn attribute->values [attribute]
  (case (:data-type attribute)
    :type-byte           (:int-values attribute) ; TODO: This probably needs some extra work for raw byte values (i.e :byte-values)
    :type-unsigned-byte  (:uint-values attribute)
    :type-short          (:int-values attribute)
    :type-unsigned-short (:uint-values attribute)
    :type-int            (:int-values attribute)
    :type-unsigned-int   (:int-values attribute)
    :type-float          (:float-values attribute)))

(defn attribute-data-type->byte-size [data-type-key]
  (case data-type-key
    :type-byte 1
    :type-unsigned-byte 1
    :type-short 2
    :type-unsigned-short 2
    :type-int 4
    :type-unsigned-int 4
    :type-float 4))

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


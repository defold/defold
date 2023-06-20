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
  (:require [editor.gl.vertex2 :as vtx]
            [potemkin.namespaces :as namespaces]
            [util.coll :refer [pair]]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.google.protobuf ByteString]))

(namespaces/import-vars [editor.gl.vertex2 attribute-name->key])

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

(def ^:private attribute-data-type->attribute-value-keyword
  (into {}
        (map (juxt :data-type :value-keyword))
        attribute-data-type-infos))

(defn attribute-value-keyword [attribute-data-type normalize]
  (if normalize
    :float-values
    (attribute-data-type->attribute-value-keyword attribute-data-type)))

(def ^:private attribute-data-type->byte-size
  (into {}
        (map (juxt :data-type :byte-size))
        attribute-data-type-infos))

(defn attribute->doubles [{:keys [data-type normalize] :as attribute}]
  (if (or normalize
          (= :type-float data-type))
    (into (vector-of :double)
          (map double)
          (:v (:float-values attribute)))
    (case data-type
      (:type-byte :type-short :type-int)
      (into (vector-of :double)
            (map double)
            (:v (:int-values attribute)))

      (:type-unsigned-byte :type-unsigned :type-unsigned-int)
      (into (vector-of :double)
            (map num/uint->double)
            (:v (:uint-values attribute))))))

(defn attribute->any-doubles [{:keys [data-type normalize] :as attribute}]
  (if (or normalize
          (= :type-float data-type))
    (into (vector-of :double)
          (map double)
          (:v (:float-values attribute)))
    (some (fn [[attribute-value-keyword coerce-fn]]
            (when-some [values (some-> (attribute-value-keyword attribute) :v not-empty)]
              (into (vector-of :double)
                    (map coerce-fn)
                    values)))
          [[:int-values double]
           [:uint-values num/uint->double]])))

(defn- doubles->stored-values [double-values attribute-value-keyword]
  (case attribute-value-keyword
    :float-values
    (into (vector-of :float)
          (map unchecked-float)
          double-values)

    :int-values
    (into (vector-of :int)
          (map unchecked-int)
          double-values)

    :uint-values
    (into (vector-of :int)
          (map num/unchecked-uint)
          double-values)))

(defn doubles->storage [double-values attribute-data-type normalize]
  (let [attribute-value-keyword (attribute-value-keyword attribute-data-type normalize)
        stored-values (doubles->stored-values double-values attribute-value-keyword)]
    (pair attribute-value-keyword stored-values)))

(defn make-attribute-bytes
  ^bytes [attribute-data-type normalize attribute-values]
  (let [attribute-value-byte-count (* (count attribute-values) (attribute-data-type->byte-size attribute-data-type))
        attribute-bytes (byte-array attribute-value-byte-count)
        byte-buffer (vtx/wrap-buf attribute-bytes)
        vtx-attribute-type (vtx/attribute-data-type->type attribute-data-type)]
    (vtx/buf-push! byte-buffer vtx-attribute-type normalize attribute-values)
    attribute-bytes))

(defn attribute-info->build-target-attribute [attribute-info]
  {:pre [(map? attribute-info)
         (keyword? (:name-key attribute-info))]}
  (let [^bytes attribute-bytes (:bytes attribute-info)]
    (-> attribute-info
        (dissoc :bytes :name-key :values)
        (assoc :name-hash (murmur/hash64 (:name attribute-info))
               :binary-values (ByteString/copyFrom attribute-bytes)))))

(defn- attribute-info->vtx-attribute [attribute-info]
  {:pre [(map? attribute-info)
         (keyword? (:name-key attribute-info))]}
  {:name (:name attribute-info)
   :name-key (:name-key attribute-info)
   :semantic-type (:semantic-type attribute-info)
   :coordinate-space (:coordinate-space attribute-info)
   :type (vtx/attribute-data-type->type (:data-type attribute-info))
   :components (:element-count attribute-info)
   :normalize (:normalize attribute-info false)})

(defn make-vertex-description [attribute-infos]
  (let [vtx-attributes (mapv attribute-info->vtx-attribute attribute-infos)]
    (vtx/make-vertex-description nil vtx-attributes)))

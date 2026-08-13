;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.collection-string-data
  (:require [editor.protobuf :as protobuf]
            [editor.resource :as resource])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc GameObjectSource$EmbeddedComponentDesc GameObjectSource$PrototypeDesc]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Validation
;; -----------------------------------------------------------------------------

(defn verify-string-decoded-embedded-component-desc!
  "Throws an informative exception if the supplied value is not an embedded
  component descriptor in canonical map format."
  [embedded-component-desc owning-resource]
  (let [component-data (:data embedded-component-desc)]
    (when-not (map? component-data)
      (let [owning-proj-path (resource/resource->proj-path owning-resource)
            component-id (:id embedded-component-desc)
            component-type (:type embedded-component-desc)]
        (throw (ex-info (format "Invalid embedded component '%s' of type '%s' in '%s'."
                                component-id
                                component-type
                                owning-proj-path)
                        {:proj-path owning-proj-path
                         :id component-id
                         :type component-type
                         :data component-data}))))))

(defn verify-string-decoded-embedded-instance-desc!
  "Throws an informative exception if the supplied value is not an embedded
  instance descriptor in canonical map format."
  [embedded-instance-desc owning-resource]
  (let [prototype-desc (:data embedded-instance-desc)]
    (when-not (map? prototype-desc)
      (let [owning-proj-path (resource/resource->proj-path owning-resource)
            game-object-instance-id (:id embedded-instance-desc)]
        (throw (ex-info (format "Invalid embedded game object instance '%s' in '%s'."
                                game-object-instance-id
                                owning-proj-path)
                        {:proj-path owning-proj-path
                         :id game-object-instance-id
                         :data prototype-desc}))))))

;; -----------------------------------------------------------------------------
;; Source format conversion
;; -----------------------------------------------------------------------------

(def ^:private embedded-component-payload-field-infos
  (into {}
        (filter (comp :is-oneof-field val))
        (protobuf/field-infos GameObjectSource$EmbeddedComponentDesc)))

(def ^:private embedded-component-payload-keys
  (vec (sort (keys embedded-component-payload-field-infos))))

(defn- component-type->payload-key [^String component-type]
  (keyword (.replace component-type "_" "-")))

(defn- selected-payload-key [payload-keys desc desc-kind]
  (let [selected-payload-keys (into []
                                    (filter #(contains? desc %))
                                    payload-keys)]
    (case (count selected-payload-keys)
      0 (throw (ex-info (str "Missing " desc-kind " payload.")
                        {:desc desc}))
      1 (nth selected-payload-keys 0)
      (throw (ex-info (str "Multiple " desc-kind " payloads are selected.")
                      {:desc desc
                       :payload-keys selected-payload-keys})))))

(defn- typed-payload-field-info [component-type component-resource-type]
  (let [payload-key (component-type->payload-key component-type)]
    (when-let [field-info (embedded-component-payload-field-infos payload-key)]
      (when (= :message (:value-type-kw field-info))
        (let [resource-ddf-type (:ddf-type component-resource-type)
              payload-ddf-type (:value-class field-info)]
          (when-not (= resource-ddf-type payload-ddf-type)
            (throw (ex-info (format "Embedded component type '%s' uses '%s', but its typed payload expects '%s'."
                                    component-type
                                    (.getName ^Class resource-ddf-type)
                                    (.getName ^Class payload-ddf-type))
                            {:component-type component-type
                             :resource-ddf-type resource-ddf-type
                             :payload-ddf-type payload-ddf-type})))
          field-info)))))

(defn- strip-embedded-component-payload [embedded-component-desc]
  (apply dissoc embedded-component-desc embedded-component-payload-keys))

(defn source-decode-embedded-component-desc
  "Converts a legacy string or typed source payload to canonical :data."
  [ext->embedded-component-resource-type source-embedded-component-desc]
  (let [component-type (:type source-embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-type)
        payload-key (selected-payload-key embedded-component-payload-keys
                                          source-embedded-component-desc
                                          "embedded component")
        component-data
        (if (= :data payload-key)
          (let [source-component-data (:data source-embedded-component-desc)]
            (if (map? source-component-data)
              source-component-data
              (let [component-read-fn (:read-fn component-resource-type)]
                (with-open [reader (StringReader. source-component-data)]
                  (component-read-fn reader)))))
          (let [expected-payload-key (component-type->payload-key component-type)]
            (when-not (= expected-payload-key payload-key)
              (throw (ex-info (format "Embedded component type '%s' does not match its '%s' payload."
                                      component-type
                                      (name payload-key))
                              {:component-type component-type
                               :expected-payload-key expected-payload-key
                               :payload-key payload-key})))
            (typed-payload-field-info component-type component-resource-type)
            ((:sanitize-pb-map-fn component-resource-type) (payload-key source-embedded-component-desc))))]
    (assoc (strip-embedded-component-payload source-embedded-component-desc)
      :data component-data)))

(def ^:private embedded-instance-payload-keys [:data :prototype])

(defn source-decode-embedded-instance-desc
  "Converts a legacy string or typed source prototype to canonical :data."
  [source-embedded-instance-desc]
  (let [payload-key (selected-payload-key embedded-instance-payload-keys
                                          source-embedded-instance-desc
                                          "embedded instance")
        prototype-desc
        (case payload-key
          :data
          (let [source-prototype-desc (:data source-embedded-instance-desc)]
            (if (map? source-prototype-desc)
              source-prototype-desc
              (protobuf/str->map-without-defaults-strict GameObjectSource$PrototypeDesc
                                                         source-prototype-desc)))

          :prototype
          (:prototype source-embedded-instance-desc))]
    (assoc (dissoc source-embedded-instance-desc :data :prototype)
      :data prototype-desc)))

(defn source-decode-prototype-desc
  [ext->embedded-component-resource-type source-prototype-desc]
  (let [decode-embedded-component-desc (partial source-decode-embedded-component-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated source-prototype-desc :embedded-components decode-embedded-component-desc)))

(defn source-decode-collection-desc
  [ext->embedded-component-resource-type source-collection-desc]
  (let [decode-prototype-desc (partial source-decode-prototype-desc ext->embedded-component-resource-type)
        decode-embedded-instance-desc (comp #(update % :data decode-prototype-desc)
                                            source-decode-embedded-instance-desc)]
    (protobuf/sanitize-repeated source-collection-desc :embedded-instances decode-embedded-instance-desc)))

(defn source-encode-embedded-component-desc
  "Projects canonical :data to a typed source payload when the source schema
  declares one for the component type. Otherwise, writes the legacy fallback."
  [ext->embedded-component-resource-type embedded-component-desc]
  (let [component-type (:type embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-type)
        typed-field-info (typed-payload-field-info component-type component-resource-type)
        source-embedded-component-desc (strip-embedded-component-payload embedded-component-desc)]
    (if typed-field-info
      (assoc source-embedded-component-desc
        (component-type->payload-key component-type) ((:encode-pb-map-fn component-resource-type) (:data embedded-component-desc)))
      (assoc source-embedded-component-desc
        :data ((:write-fn component-resource-type) (:data embedded-component-desc))))))

(defn source-encode-prototype-desc
  [ext->embedded-component-resource-type prototype-desc]
  (let [encode-embedded-component-desc (partial source-encode-embedded-component-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated prototype-desc :embedded-components encode-embedded-component-desc)))

(defn source-encode-embedded-instance-desc
  [ext->embedded-component-resource-type embedded-instance-desc]
  (let [source-prototype-desc (source-encode-prototype-desc ext->embedded-component-resource-type
                                                            (:data embedded-instance-desc))]
    (assoc (dissoc embedded-instance-desc :data :prototype)
      :prototype source-prototype-desc)))

(defn source-encode-collection-desc
  [ext->embedded-component-resource-type collection-desc]
  (let [encode-embedded-instance-desc (partial source-encode-embedded-instance-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated collection-desc :embedded-instances encode-embedded-instance-desc)))

;; -----------------------------------------------------------------------------
;; Legacy string format compatibility
;; -----------------------------------------------------------------------------

(defn string-decode-embedded-component-desc
  [ext->embedded-component-resource-type string-encoded-embedded-component-desc]
  (let [component-ext (:type string-encoded-embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-read-fn (:read-fn component-resource-type)]
    (update string-encoded-embedded-component-desc
            :data
            (fn [^String embedded-component-string]
              (with-open [reader (StringReader. embedded-component-string)]
                (component-read-fn reader))))))

(defn string-decode-prototype-desc
  [ext->embedded-component-resource-type string-encoded-prototype-desc]
  (let [decode-embedded-component-desc (partial string-decode-embedded-component-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-encoded-prototype-desc :embedded-components decode-embedded-component-desc)))

(defn string-decode-embedded-instance-desc
  [ext->embedded-component-resource-type string-encoded-embedded-instance-desc]
  (let [decode-prototype-desc (partial string-decode-prototype-desc ext->embedded-component-resource-type)
        decode-embedded-prototype-desc (comp decode-prototype-desc
                                             (partial protobuf/str->map-without-defaults GameObject$PrototypeDesc))]
    (update string-encoded-embedded-instance-desc :data decode-embedded-prototype-desc)))

(defn string-decode-collection-desc
  [ext->embedded-component-resource-type string-encoded-collection-desc]
  (let [decode-embedded-instance-desc (partial string-decode-embedded-instance-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-encoded-collection-desc :embedded-instances decode-embedded-instance-desc)))

(defn string-encode-embedded-component-desc
  [ext->embedded-component-resource-type string-decoded-embedded-component-desc]
  (let [component-ext (:type string-decoded-embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-write-fn (:write-fn component-resource-type)]
    (update string-decoded-embedded-component-desc :data component-write-fn)))

(defn string-encode-prototype-desc
  [ext->embedded-component-resource-type string-decoded-prototype-desc]
  (let [encode-embedded-component-desc (partial string-encode-embedded-component-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-decoded-prototype-desc :embedded-components encode-embedded-component-desc)))

(defn string-encode-embedded-instance-desc
  [ext->embedded-component-resource-type string-decoded-embedded-instance-desc]
  (let [encode-prototype-desc (partial string-encode-prototype-desc ext->embedded-component-resource-type)
        encode-embedded-prototype-desc (comp #(protobuf/map->str GameObject$PrototypeDesc % false)
                                             encode-prototype-desc)]
    (update string-decoded-embedded-instance-desc :data encode-embedded-prototype-desc)))

(defn string-encode-collection-desc
  [ext->embedded-component-resource-type string-decoded-collection-desc]
  (let [encode-embedded-instance-desc (partial string-encode-embedded-instance-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-decoded-collection-desc :embedded-instances encode-embedded-instance-desc)))

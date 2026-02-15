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
  (:require [clojure.set :as set]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Embedded component payload mapping.
;; -----------------------------------------------------------------------------

(def ^:private embedded-component-payload-field-key-by-ext
  {"collisionobject" :collisionobject
   "label" :label
   "sprite" :sprite
   "sound" :sound
   "model" :model
   "factory" :factory
   "collectionfactory" :collectionfactory
   "particlefx" :particlefx
   "camera" :camera
   "collectionproxy" :collectionproxy})

(def ^:private embedded-component-ext-by-payload-field-key
  (set/map-invert embedded-component-payload-field-key-by-ext))

(def ^:private embedded-component-payload-field-keys
  (vec (vals embedded-component-payload-field-key-by-ext)))

(defn- clear-default-component-data [component-resource-type component-data]
  (let [ddf-type (:ddf-type component-resource-type)]
    (if (and (map? component-data) (class? ddf-type))
      (protobuf/clear-defaults ddf-type component-data)
      component-data)))

(defn- typed-embedded-component-payload-entry [embedded-component-desc]
  (let [component-ext (:type embedded-component-desc)
        payload-field-key-for-type (embedded-component-payload-field-key-by-ext component-ext)]
    (or (when-some [payload-field-key payload-field-key-for-type]
          (when-some [payload (get embedded-component-desc payload-field-key)]
            [payload-field-key payload]))
        (some (fn [payload-field-key]
                (when-some [payload (get embedded-component-desc payload-field-key)]
                  [payload-field-key payload]))
              embedded-component-payload-field-keys))))

(defn- strip-typed-embedded-component-payload [embedded-component-desc]
  (apply dissoc embedded-component-desc embedded-component-payload-field-keys))

;; -----------------------------------------------------------------------------
;; Validation
;; -----------------------------------------------------------------------------

(defn verify-string-decoded-embedded-component-desc!
  "Throws an informative exception if the supplied value is not a
  GameObject$EmbeddedComponentDesc in map format with the :data field converted
  to a map."
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
  "Throws an informative exception if the supplied value is not a
  GameObject$EmbeddedInstanceDesc in map format with the :data field converted
  to a map. Does not ensure GameObject$EmbeddedComponentDescs inside the
  embedded game object have been string decoded. You'll need to call
  verify-string-decoded-embedded-component-desc! separately on the embedded
  components."
  [embedded-instance-desc owning-resource]
  (let [prototype-desc (or (:data embedded-instance-desc)
                           (:prototype embedded-instance-desc))]
    (when-not (map? prototype-desc)
      (let [owning-proj-path (resource/resource->proj-path owning-resource)
            game-object-instance-id (:id embedded-instance-desc)]
        (throw (ex-info (format "Invalid embedded game object instance '%s' in '%s'."
                                game-object-instance-id
                                owning-proj-path)
                        {:proj-path owning-proj-path
                         :id game-object-instance-id
                         :data prototype-desc}))))))

(defn normalize-embedded-instance-desc-payload
  "Normalizes typed oneof payloads to legacy :data map shape used by editor
  internals."
  [embedded-instance-desc]
  (if (and (map? (:prototype embedded-instance-desc))
           (not (map? (:data embedded-instance-desc))))
    (-> embedded-instance-desc
        (assoc :data (:prototype embedded-instance-desc))
        (dissoc :prototype))
    embedded-instance-desc))

;; -----------------------------------------------------------------------------
;; Decoding embedded strings to maps.
;; -----------------------------------------------------------------------------

(defn string-decode-embedded-component-desc
  "Takes a GameObject$EmbeddedComponentDesc in map format with string :data and
  returns a GameObject$EmbeddedComponentDesc in map format with the :data field
  converted to a map."
  [ext->embedded-component-resource-type string-encoded-embedded-component-desc]
  (let [component-ext (:type string-encoded-embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-read-fn (:read-fn component-resource-type)
        component-data (:data string-encoded-embedded-component-desc)
        [payload-field-key payload] (typed-embedded-component-payload-entry string-encoded-embedded-component-desc)]
    (cond
      (map? component-data)
      (-> string-encoded-embedded-component-desc
          (strip-typed-embedded-component-payload)
          (assoc :data (clear-default-component-data component-resource-type component-data)))

      (string? component-data)
      (if (some? component-read-fn)
        (let [string->component-data (fn [^String embedded-component-string]
                                       (with-open [reader (StringReader. embedded-component-string)]
                                         (component-read-fn reader)))]
          (-> string-encoded-embedded-component-desc
              (strip-typed-embedded-component-payload)
              (update :data string->component-data)
              (update :data (partial clear-default-component-data component-resource-type))))
        string-encoded-embedded-component-desc)

      (map? payload)
      (let [inferred-component-ext (or component-ext
                                       (embedded-component-ext-by-payload-field-key payload-field-key))
            inferred-component-resource-type (ext->embedded-component-resource-type inferred-component-ext)]
        (-> string-encoded-embedded-component-desc
            (strip-typed-embedded-component-payload)
            (assoc :type inferred-component-ext
                   :data (clear-default-component-data inferred-component-resource-type payload))))

      :else
      string-encoded-embedded-component-desc)))

(defn string-decode-prototype-desc
  "Takes a GameObject$PrototypeDesc in map format with string :data in all its
  GameObject$EmbeddedComponentDescs and returns a GameObject$PrototypeDesc in
  map format with the :data fields in each GameObject$EmbeddedComponentDesc
  converted to a map."
  [ext->embedded-component-resource-type string-encoded-prototype-desc]
  (let [string-decode-embedded-component-desc (partial string-decode-embedded-component-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-encoded-prototype-desc :embedded-components string-decode-embedded-component-desc)))

(defn string-decode-embedded-instance-desc
  "Takes a GameObject$EmbeddedInstanceDesc in map format with string :data and
  returns a GameObject$EmbeddedInstanceDesc in map format with the :data field
  converted to a map."
  [ext->embedded-component-resource-type string-encoded-embedded-instance-desc]
  (let [string-encoded-embedded-instance-desc (normalize-embedded-instance-desc-payload string-encoded-embedded-instance-desc)
        game-object-read-fn (partial protobuf/str->map-without-defaults GameObject$PrototypeDesc)
        string-decode-prototype-desc (partial string-decode-prototype-desc ext->embedded-component-resource-type)
        string-decode-embedded-prototype-desc (comp string-decode-prototype-desc game-object-read-fn)
        prototype-desc-data (:data string-encoded-embedded-instance-desc)
        prototype-desc-payload (:prototype string-encoded-embedded-instance-desc)]
    (cond
      (map? prototype-desc-data)
      string-encoded-embedded-instance-desc

      (map? prototype-desc-payload)
      (-> string-encoded-embedded-instance-desc
          (assoc :data (string-decode-prototype-desc prototype-desc-payload))
          (dissoc :prototype))

      (string? prototype-desc-data)
      (if (empty? prototype-desc-data)
        ;; Legacy empty embedded game object data payload is encoded as `data: ""`.
        ;; In oneof form, this may also be omitted as an unset payload.
        (assoc string-encoded-embedded-instance-desc :data {})
        (update string-encoded-embedded-instance-desc :data string-decode-embedded-prototype-desc))

      ;; If oneof payload is completely absent (neither :data nor :prototype),
      ;; preserve legacy behavior and treat as empty embedded game object.
      (and (nil? prototype-desc-data) (nil? prototype-desc-payload))
      (assoc string-encoded-embedded-instance-desc :data {})

      (some? prototype-desc-data)
      (update string-encoded-embedded-instance-desc :data string-decode-embedded-prototype-desc)

      :else
      string-encoded-embedded-instance-desc)))

(defn string-decode-collection-desc
  "Takes a GameObject$CollectionDesc in map format with string :data in all its
  GameObject$EmbeddedInstanceDescs and returns a GameObject$CollectionDesc in
  map format with the :data fields in each GameObject$EmbeddedInstanceDescs
  converted to a map."
  [ext->embedded-component-resource-type string-encoded-collection-desc]
  (let [string-decode-embedded-instance-desc (partial string-decode-embedded-instance-desc ext->embedded-component-resource-type)]
    (protobuf/sanitize-repeated string-encoded-collection-desc :embedded-instances string-decode-embedded-instance-desc)))

;; -----------------------------------------------------------------------------
;; Encoding embedded maps to strings.
;; -----------------------------------------------------------------------------

(declare string-encode-prototype-desc)

(defn string-encode-embedded-component-desc
  "Takes a GameObject$EmbeddedComponentDesc in map format with map :data and
  returns a GameObject$EmbeddedComponentDesc in map format with the :data field
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-embedded-component-desc]
  (let [component-ext (:type string-decoded-embedded-component-desc)
        payload-field-key (embedded-component-payload-field-key-by-ext component-ext)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-write-fn (:write-fn component-resource-type)
        component-data (:data string-decoded-embedded-component-desc)
        component-data (clear-default-component-data component-resource-type component-data)]
    (if (and (keyword? payload-field-key) (map? component-data))
      (if (empty? component-data)
        (-> string-decoded-embedded-component-desc
            (strip-typed-embedded-component-payload)
            (dissoc :data))
        (-> string-decoded-embedded-component-desc
            (strip-typed-embedded-component-payload)
            (dissoc :data)
            (assoc payload-field-key component-data)))
      (if (and (ifn? component-write-fn) (map? component-data))
        (-> string-decoded-embedded-component-desc
            (strip-typed-embedded-component-payload)
            (update :data component-write-fn))
        (-> string-decoded-embedded-component-desc
            (strip-typed-embedded-component-payload)
            (update :type #(or % (embedded-component-ext-by-payload-field-key payload-field-key))))))))

(defn- empty-prototype-desc? [prototype-desc]
  (or (nil? prototype-desc)
      (and (map? prototype-desc) (empty? prototype-desc))))

(defn string-encode-embedded-instance-desc
  "Takes a GameObject$EmbeddedInstanceDesc in map format with map :data and
  returns a GameObject$EmbeddedInstanceDesc in map format with the :data field
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-embedded-instance-desc]
  (let [string-encode-prototype-desc (partial string-encode-prototype-desc ext->embedded-component-resource-type)
        prototype-desc (:data string-decoded-embedded-instance-desc)]
    (if (map? prototype-desc)
      ;; Keep truly empty embedded game objects on legacy `data: ""` representation
      ;; to avoid introducing noisy oneof `prototype {}` payloads.
      (if (empty-prototype-desc? prototype-desc)
        (-> string-decoded-embedded-instance-desc
            (dissoc :prototype)
            (assoc :data ""))
        (-> string-decoded-embedded-instance-desc
            (dissoc :data)
            (assoc :prototype (string-encode-prototype-desc prototype-desc))))
      string-decoded-embedded-instance-desc)))

(defn string-encode-prototype-desc
  "Takes a GameObject$PrototypeDesc in map format with map :data in all its
  GameObject$EmbeddedComponentDescs and returns a GameObject$PrototypeDesc in
  map format with the :data fields in each GameObject$EmbeddedComponentDesc
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-prototype-desc]
  (let [string-encode-embedded-component-desc (partial string-encode-embedded-component-desc ext->embedded-component-resource-type)
        string-encode-embedded-component-descs (partial mapv string-encode-embedded-component-desc)]
    (update string-decoded-prototype-desc :embedded-components string-encode-embedded-component-descs)))

(defn string-encode-collection-desc
  "Takes a GameObject$CollectionDesc in map format with map :data in all its
  GameObject$EmbeddedInstanceDescs and returns a GameObject$CollectionDesc in
  map format with the :data fields in each GameObject$EmbeddedInstanceDescs
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-collection-desc]
  (let [string-encode-embedded-instance-desc (partial string-encode-embedded-instance-desc ext->embedded-component-resource-type)
        string-encode-embedded-instance-descs (partial mapv string-encode-embedded-instance-desc)]
    (update string-decoded-collection-desc :embedded-instances string-encode-embedded-instance-descs)))

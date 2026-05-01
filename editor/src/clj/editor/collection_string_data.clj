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
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

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
        string->component-data (fn [^String embedded-component-string]
                                 (with-open [reader (StringReader. embedded-component-string)]
                                   (component-read-fn reader)))]
    (update string-encoded-embedded-component-desc :data string->component-data)))

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
  (let [game-object-read-fn (partial protobuf/str->map-without-defaults GameObject$PrototypeDesc)
        string-decode-prototype-desc (partial string-decode-prototype-desc ext->embedded-component-resource-type)
        string-decode-embedded-prototype-desc (comp string-decode-prototype-desc game-object-read-fn)]
    (update string-encoded-embedded-instance-desc :data string-decode-embedded-prototype-desc)))

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

(defn string-encode-embedded-component-desc
  "Takes a GameObject$EmbeddedComponentDesc in map format with map :data and
  returns a GameObject$EmbeddedComponentDesc in map format with the :data field
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-embedded-component-desc]
  (let [component-ext (:type string-decoded-embedded-component-desc)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-write-fn (:write-fn component-resource-type)]
    (update string-decoded-embedded-component-desc :data component-write-fn)))

(defn string-encode-prototype-desc
  "Takes a GameObject$PrototypeDesc in map format with map :data in all its
  GameObject$EmbeddedComponentDescs and returns a GameObject$PrototypeDesc in
  map format with the :data fields in each GameObject$EmbeddedComponentDesc
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-prototype-desc]
  (let [string-encode-embedded-component-desc (partial string-encode-embedded-component-desc ext->embedded-component-resource-type)
        string-encode-embedded-component-descs (partial mapv string-encode-embedded-component-desc)]
    (update string-decoded-prototype-desc :embedded-components string-encode-embedded-component-descs)))

(defn string-encode-embedded-instance-desc
  "Takes a GameObject$EmbeddedInstanceDesc in map format with map :data and
  returns a GameObject$EmbeddedInstanceDesc in map format with the :data field
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-embedded-instance-desc]
  (let [game-object-write-fn #(protobuf/map->str GameObject$PrototypeDesc % false)
        string-encode-prototype-desc (partial string-encode-prototype-desc ext->embedded-component-resource-type)
        string-encode-embedded-prototype-desc (comp game-object-write-fn string-encode-prototype-desc)]
    (update string-decoded-embedded-instance-desc :data string-encode-embedded-prototype-desc)))

(defn string-encode-collection-desc
  "Takes a GameObject$CollectionDesc in map format with map :data in all its
  GameObject$EmbeddedInstanceDescs and returns a GameObject$CollectionDesc in
  map format with the :data fields in each GameObject$EmbeddedInstanceDescs
  converted to a string."
  [ext->embedded-component-resource-type string-decoded-collection-desc]
  (let [string-encode-embedded-instance-desc (partial string-encode-embedded-instance-desc ext->embedded-component-resource-type)
        string-encode-embedded-instance-descs (partial mapv string-encode-embedded-instance-desc)]
    (update string-decoded-collection-desc :embedded-instances string-encode-embedded-instance-descs)))

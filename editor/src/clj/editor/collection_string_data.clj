(ns editor.collection-string-data
  (:require [editor.protobuf :as protobuf])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

(defn string-decode-embedded-component-data [ext->embedded-component-resource-type string-encoded-embedded-component-data]
  (let [component-ext (:type string-encoded-embedded-component-data)
        component-resource-type (ext->embedded-component-resource-type component-ext)
        component-read-fn (:read-fn component-resource-type)
        string->component-data (fn [^String embedded-component-string]
                                 (with-open [reader (StringReader. embedded-component-string)]
                                   (component-read-fn reader)))]
    (update string-encoded-embedded-component-data :data string->component-data)))

(defn string-decode-game-object-data [ext->embedded-component-resource-type string-encoded-game-object-data]
  (let [string-decode-embedded-component-data (partial string-decode-embedded-component-data ext->embedded-component-resource-type)
        string-decode-embedded-components-data (partial mapv string-decode-embedded-component-data)]
    (update string-encoded-game-object-data :embedded-components string-decode-embedded-components-data)))

(defn string-decode-embedded-instance-data [ext->embedded-component-resource-type string-encoded-embedded-instance-data]
  (let [game-object-read-fn (partial protobuf/str->map GameObject$PrototypeDesc)
        string-decode-game-object-data (partial string-decode-game-object-data ext->embedded-component-resource-type)
        string-decode-embedded-game-object-data (comp string-decode-game-object-data game-object-read-fn)]
    (update string-encoded-embedded-instance-data :data string-decode-embedded-game-object-data)))

(defn string-decode-collection-data [ext->embedded-component-resource-type string-encoded-collection-data]
  (let [string-decode-embedded-instance-data (partial string-decode-embedded-instance-data ext->embedded-component-resource-type)
        string-decode-embedded-instances-data (partial mapv string-decode-embedded-instance-data)]
    (update string-encoded-collection-data :embedded-instances string-decode-embedded-instances-data)))

(defn string-encoded-data [ext->embedded-component-resource-type string-decoded-pb-map]
  (let [resource-ext (:type string-decoded-pb-map)
        resource-type (ext->embedded-component-resource-type resource-ext)
        resource-write-fn (:write-fn resource-type)]
    (resource-write-fn (:data string-decoded-pb-map))))

(defn string-encode-embedded-component-data [ext->embedded-component-resource-type string-decoded-embedded-component-data]
  (assoc string-decoded-embedded-component-data
    :data (string-encoded-data ext->embedded-component-resource-type string-decoded-embedded-component-data)))

(defn string-encode-game-object-data [ext->embedded-component-resource-type string-decoded-game-object-data]
  (let [string-encode-embedded-component-data (partial string-encode-embedded-component-data ext->embedded-component-resource-type)
        string-encode-embedded-components-data (partial mapv string-encode-embedded-component-data)]
    (update string-decoded-game-object-data :embedded-components string-encode-embedded-components-data)))

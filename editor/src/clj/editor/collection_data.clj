(ns editor.collection-data
  (:import [java.io StringReader]))

(set! *warn-on-reflection* true)

(defn string-decode-embedded-component-data [ext->resource-type string-encoded-embedded-component-data]
  (let [component-ext (:type string-encoded-embedded-component-data)
        component-resource-type (ext->resource-type component-ext)
        component-read-fn (:read-fn component-resource-type)
        string->component-data (fn [^String embedded-component-string]
                                 (with-open [reader (StringReader. embedded-component-string)]
                                   (component-read-fn reader)))]
    (update string-encoded-embedded-component-data :data string->component-data)))

(defn string-decode-game-object-data [ext->resource-type string-encoded-game-object-data]
  (let [string-decode-embedded-component-data (partial string-decode-embedded-component-data ext->resource-type)
        string-decode-embedded-components-data (partial mapv string-decode-embedded-component-data)]
    (update string-encoded-game-object-data :embedded-components string-decode-embedded-components-data)))

(defn string-decode-embedded-instance-data [ext->resource-type string-encoded-embedded-instance-data]
  (let [game-object-resource-type (ext->resource-type "go")
        game-object-read-fn (:read-fn game-object-resource-type)
        string-decode-game-object-data (partial string-decode-game-object-data ext->resource-type)
        string-decode-embedded-game-object-data (comp string-decode-game-object-data
                                                      (fn [^String embedded-game-object-string]
                                                        (with-open [reader (StringReader. embedded-game-object-string)]
                                                          (game-object-read-fn reader))))]
    (update string-encoded-embedded-instance-data :data string-decode-embedded-game-object-data)))

(defn string-decode-collection-data [ext->resource-type string-encoded-collection-data]
  (let [string-decode-embedded-instance-data (partial string-decode-embedded-instance-data ext->resource-type)
        string-decode-embedded-instances-data (partial mapv string-decode-embedded-instance-data)]
    (update string-encoded-collection-data :embedded-instances string-decode-embedded-instances-data)))

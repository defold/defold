(ns dynamo.file.protobuf
  (:require [clojure.string :as str]
            [dynamo.node :as n]
            [dynamo.file :as f]
            [dynamo.system :as ds]
            [camel-snake-kebab :refer :all])
  (:import [com.google.protobuf Message TextFormat]))

(set! *warn-on-reflection* true)

(defn getter
  [fld]
  (symbol (->camelCase (str "get-" (name fld)))))

(defn message-mapper
  [cls props]
  `(fn [~'protobuf]
     (hash-map
       ~@(mapcat (fn [k getter] [k (list '. 'protobuf (symbol getter))])
           props (map getter props)))))

(defn connection
  [[source-label _ target-label]]
  (list `ds/connect 'node source-label 'this target-label))

(defn subordinate-mapper
  [[from-property connections]]
  `(doseq [~'node (map f/message->node (. ~'protobuf ~(getter from-property)))]
     ~@(map connection (partition-all 3 connections))))

(defmacro protocol-buffer-converter
  [class spec]
  `(defmethod f/message->node ~class
     [~'protobuf & {:as ~'overrides}]
     (let [~'message-mapper ~(message-mapper class (:basic-properties spec))
           ~'basic-props    (~'message-mapper ~'protobuf)
           ~'this           (apply n/construct ~(:node-type spec) (mapcat identity (merge ~'basic-props ~'overrides)))]
       (ds/add ~'this)
       ~@(map subordinate-mapper (:node-properties spec))
       ~'this)))

(defmacro protocol-buffer-converters
  [& class+specs]
  (let [converters (map (partial cons `protocol-buffer-converter) (partition 2 class+specs))]
    (list* 'do converters)))

(defn pb->str
  [^Message pb]
  (TextFormat/printToString pb))

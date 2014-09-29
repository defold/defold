(ns dynamo.file.protobuf
  (:require [clojure.string :as str]
            [internal.java :as j]
            [dynamo.file :as f]
            [dynamo.project :as p]
            [camel-snake-kebab :refer :all])
  (:import [com.google.protobuf Message TextFormat]))

(set! *warn-on-reflection* true)

(defn mapper-name [cls]
  (symbol (str (name cls) "->map")))

(defn getter-name [fld]
  (->camelCase (str "get-" (name fld))))

(defn message-mapper
  [cls props]
  (let [cname    (mapper-name cls)
        gs       (map (comp symbol getter-name) props)
        msg-symb (symbol "msg")]
    `(defn ~cname
       [~msg-symb]
       (hash-map
        ~@(mapcat (fn [k getter]
                    [k (list '. msg-symb (symbol getter))])
               props gs)))))

(defn adder-fn
  [parent-sym connections]
  (let [child 'child
        cxors (map (fn [[from _ to]]
                     (list 'dynamo.project/connect child from parent-sym to))
                   (partition 3 connections))]
    `(fn [~child]
       ~@cxors)))

(defn subordinate-mapper
  [parent-sym msg-sym from-property connections]
  (let [getter (symbol (getter-name from-property))]
    `(mapcat #(f/message->node % ~(adder-fn parent-sym connections)) (. ~msg-sym ~getter))))

(defmacro protocol-buffer-converter
  [class spec]
  (let [nested-properties (:node-properties spec)
        node-sym          (symbol "this")
        msg-sym           (symbol "msg")
        incoming-adder    (symbol "adder")
        converter-name    (mapper-name class)
        ctor              (:constructor spec)
        mapper-sexps      (message-mapper class (:basic-properties spec))
        nested-mappers    (mapcat (fn [[prop conns]]
                                    [(gensym (name prop))
                                     (subordinate-mapper node-sym msg-sym prop conns)])
                               nested-properties)]
    `(do
       ~mapper-sexps
       (defmethod f/message->node ~class
         [~msg-sym ~incoming-adder & {:as overrides#}]
         (let [basic-props# (~converter-name ~msg-sym)
               ~node-sym    (apply ~ctor (mapcat identity (merge basic-props# overrides#)))
               ~@nested-mappers]
           (concat (p/new-resource ~node-sym)
                   ~@(take-nth 2 nested-mappers)
                   (~incoming-adder ~node-sym)))))))

(defmacro protocol-buffer-converters
  [& class+specs]
  (let [converters (map (partial cons `protocol-buffer-converter) (partition 2 class+specs))]
    (list* 'do converters)))

(defn pb->str
  [^Message pb]
  (TextFormat/printToString pb))
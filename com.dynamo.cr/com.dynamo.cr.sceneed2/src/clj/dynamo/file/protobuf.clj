(ns dynamo.file.protobuf
  (:require [clojure.string :as str]
            [internal.java :as j]
            [dynamo.file :as f]
            [dynamo.project :as p]
            [camel-snake-kebab :refer :all]))

(defn mapper-name [cls]
  (symbol (str (last (str/split (.getName cls) #"[\.\$]")) "->map")))

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

(defmacro protocol-buffer-converter
  [class spec]
  (let [nested-properties (:node-properties spec)
        node              `~'node
        msg               `~'msg
        converter-name    (mapper-name class)
        ctor              (:constructor spec)
        mapper-sexps      (message-mapper class (:basic-properties spec))]
    `(do ~mapper-sexps
         (defmethod f/message->node ~class
            [~msg container# container-target# desired-output#]
            (let [~node (apply ~ctor (mapcat identity (~converter-name ~msg)))]
              [(p/new-resource ~node)
               (if container#
                 (p/connect ~node desired-output# container# container-target#)
                 [])
               ~@(for [[node-prop [from _ to]] (:node-properties spec)]
                   `(for [i# (. ~msg ~(symbol (getter-name node-prop)))]
                      (f/message->node i# ~node ~to ~from)))])))))

(defmacro protocol-buffer-converters
  [& class+specs]
  (let [converters (map (partial cons `protocol-buffer-converter) (partition 2 class+specs))]
    (list* 'do converters)))

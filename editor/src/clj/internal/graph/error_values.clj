(ns internal.graph.error-values
  (:require [internal.util :as util]))

(set! *warn-on-reflection* true)

(def ^:private severity-levels {:info 0
                                :warning 10
                                :fatal 20})

(defrecord ErrorValue [_node-id _label severity value message causes user-data])

(defn error-value
  ([severity message]
    (error-value severity message {}))
  ([severity message user-data]
    (map->ErrorValue {:severity severity :message message :user-data user-data})))

(def error-info    (partial error-value :info))
(def error-warning (partial error-value :warning))
(def error-fatal   (partial error-value :fatal))

(defn ->error
  ([node-id label severity value message]
    (->error node-id label severity value message nil {}))
  ([node-id label severity value message user-data]
    (->ErrorValue node-id label severity value message nil user-data)))

(defn error?
  [x]
  (cond
    (sequential? x)          (some error? x)
    (instance? ErrorValue x) x
    :else                    nil))

(defn- sev? [level x] (< (or level 0) (or (severity-levels (:severity x)) 0)))

(defn worse-than
  [severity x]
  (when (instance? ErrorValue x) (sev? (severity-levels severity) x)))

(defn severity? [severity e] (= (severity-levels severity) (severity-levels (:severity e))))

(def  error-info?    (partial severity? :info))
(def  error-warning? (partial severity? :warning))
(def  error-fatal?   (partial severity? :fatal))

(defn error-aggregate
  ([es]
   (let [max-severity (reduce (fn [result severity] (if (> (severity-levels result) (severity-levels severity))
                                                      result
                                                      severity))
                              :info (keep :severity es))]
     (map->ErrorValue {:severity max-severity :causes es})))
  ([es & kvs]
   (apply assoc (error-aggregate es) kvs)))

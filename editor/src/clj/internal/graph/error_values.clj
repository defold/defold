(ns internal.graph.error-values
  (:require [internal.util :as util]))

(set! *warn-on-reflection* true)

(def INFO     0)
(def WARNING 10)
(def SEVERE  20)
(def FATAL   30)

(defrecord ErrorValue [_node-id _label severity value causes user-data])

(defn error-value [severity user-data] (map->ErrorValue {:severity severity :user-data user-data}))

(def error-info    (partial error-value INFO))
(def error-warning (partial error-value WARNING))
(def error-severe  (partial error-value SEVERE))
(def error-fatal   (partial error-value FATAL))

(defn error?
  [x]
  (cond
    (sequential? x)          (some error? x)
    (instance? ErrorValue x) x
    :else                    nil))

(defn- sev? [threshold x] (< (or threshold 0) (or (:severity x) 0)))

(defn worse-than
  [severity x]
  (cond
    (sequential? x)          (filter #(sev? severity %) x)
    (instance? ErrorValue x) (when (sev? severity x) x)
    :else                    nil))

(defn use-original-value
  [x]
  (cond
    (sequential? x)          (mapv use-original-value x)
    (instance? ErrorValue x) (:value x)
    :else                    x))

(defn severity? [level e]  (= level (:severity e)))

(def  error-info?    (partial severity? INFO))
(def  error-warning? (partial severity? WARNING))
(def  error-severe?  (partial severity? SEVERE))
(def  error-fatal?   (partial severity? FATAL))

(defn most-serious    [es] (first (reverse (sort-by :severity es))))

(defn error-aggregate
  ([es]
   (map->ErrorValue {:severity (apply max 0 (keep :severity es)) :causes es}))
  ([es & kvs]
   (apply assoc (error-aggregate es) kvs)))

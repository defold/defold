(ns internal.graph.error-values)

(def INFO     0)
(def WARNING 10)
(def SEVERE  20)
(def FATAL   30)

(defrecord ErrorValue [_node-id _label severity value causes user-data])

(defn error-value [severity user-data] (map->ErrorValue {:severity severity :user-data user-data}))

(def info    (partial error-value INFO))
(def warning (partial error-value WARNING))
(def severe  (partial error-value SEVERE))
(def fatal   (partial error-value FATAL))

(defn error?
  [x]
  (cond
    (vector? x)              (some error? x)
    (instance? ErrorValue x) x
    :else                    nil))

(defn worse-than
  [severity x]
  (cond
    (vector? x)                              (seq (filter #(worse-than severity %) x))
    (< (or severity 0) (or (:severity x) 0)) x
    :else                                    nil))

(defn use-original-value
  [x]
  (cond
    (vector? x)              (mapv use-original-value x)
    (instance? ErrorValue x) (:value x)
    :else                    x))

(defn severity? [level e]  (= level (:severity e)))

(def  info?    (partial severity? INFO))
(def  warning? (partial severity? WARNING))
(def  severe?  (partial severity? SEVERE))
(def  fatal?   (partial severity? FATAL))

(defn most-serious    [es] (first (reverse (sort-by :severity es))))

(defn error-aggregate
  [es]
  ;; TODO - this is too simplistic. needs to account for errors in
  ;; vectors in values of an input map.
  (map->ErrorValue {:severity (apply max 0 (keep :severity es)) :causes es}))

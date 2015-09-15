(ns internal.graph.error-values)

(def INFO     0)
(def WARNING 10)
(def SEVERE  20)
(def FATAL   30)

(defrecord ErrorValue [severity root-cause error-data])

(defn error-value [severity root-cause error-data] (->ErrorValue severity root-cause error-data))

(def info    (partial error-value INFO    []))
(def warning (partial error-value WARNING []))
(def severe  (partial error-value SEVERE  []))
(def fatal   (partial error-value FATAL   []))

(defn error?
  [x]
  (cond
    (vector? x)              (some error? x)
    (instance? ErrorValue x) (:severity x)
    :else                    false))

(defn severity? [level e]  (= level (:severity e)))

(def  info?    (partial severity? INFO))
(def  warning? (partial severity? WARNING))
(def  severe?  (partial severity? SEVERE))
(def  fatal?   (partial severity? FATAL))

(defn worse-than [left right] (< (:severity left) (:severity right)))
(defn most-serious    [es] (first (reverse (sort-by :severity es))))
(defn error-aggregate [es] (->ErrorValue (apply max (map :severity es)) es nil))

(ns dynamo.util)

(defn map-keys
  [f m]
  (zipmap
    (map f (keys m))
    (vals m)))

(defn map-vals
  [f m]
  (zipmap
    (keys m)
    (map f (vals m))))
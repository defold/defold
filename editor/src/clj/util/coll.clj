(ns util.coll
  (:import [clojure.lang MapEntry]))

(defn pair
  "Returns a two-element collection that implements IPersistentVector."
  [a b]
  (MapEntry. a b))

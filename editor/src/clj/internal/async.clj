(ns internal.async
  (:require [clojure.core.async :as a]))

(defn take-all
  "Gather all messages currently available from in.
   Return a collection of them. Returns as soon as `in`
   is empty. No waiting."
  [in]
  (loop [batch []]
    (let [[v _] (a/alts!! [in] :default nil)]
      (if-not v
        batch
        (recur (conj batch v))))))

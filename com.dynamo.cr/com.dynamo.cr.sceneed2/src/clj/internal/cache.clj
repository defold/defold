(ns internal.cache
  (:require [clojure.core.cache :as cache]))

(defn make-cache
  []
  (cache/lru-cache-factory {} :threshold 1000))

(defn caching
  "Like memoize, but uses an LRU cache with a finite size"
  [f]
  (let [cache (atom (make-cache))]
    (fn [& args]
      (if-let [v (get @cache args)]
        v
        (let [ret (apply f args)]
          (swap! cache assoc args ret)
          ret)))))

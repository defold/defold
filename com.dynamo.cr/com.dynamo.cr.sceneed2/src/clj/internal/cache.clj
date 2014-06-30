(ns internal.cache
  (:require [clojure.core.cache :as cache]))

(defn make-cache []
  (cache/lru-cache-factory {} :threshold 1000))
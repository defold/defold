(ns internal.disposal
  (:require [internal.async :as ia]
            [internal.graph.types :as gt]
            [service.log :as log :refer [logging-exceptions]]))

(defn- dispose-one [value]
  (logging-exceptions "disposal-loop"
    (when (gt/disposable? value)
      (gt/dispose value))))

(defn dispose-pending
  [queue]
  (logging-exceptions "disposal"
    (doseq [v (ia/take-all queue)]
      (dispose-one v))))

(defn disposal-message [v] v)

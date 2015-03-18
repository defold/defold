(ns internal.disposal
  (:require [internal.async :as ia]
            [dynamo.types :as t]
            [service.log :as log :refer [logging-exceptions]]))

(defn- dispose-one [value]
  (logging-exceptions "disposal-loop"
    (when (t/disposable? value)
      (t/dispose value))))

(defn dispose-pending
  [queue]
  (logging-exceptions "disposal"
    (doseq [v (ia/take-all queue)]
      (dispose-one v))))

(defn disposal-message [v] v)

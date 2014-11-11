(ns internal.disposal
  (:require [internal.async :as ia]
            [dynamo.types :as t]
            [service.log :as log :refer [logging-exceptions]]))

(defn- dispose-one [value]
  (logging-exceptions "disposal-loop"
    (when (t/disposable? value)
      (t/dispose value))))

(defn dispose-pending
  [world-ref]
  (logging-exceptions "disposal"
    (doseq [v (ia/take-all (:disposal-queue @world-ref))]
      (dispose-one v))))

(defn disposal-message [v] v)

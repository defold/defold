(ns internal.disposal
  (:require [clojure.core.async :as a]
            [com.stuartsierra.component :as component]
            [dynamo.resource :as r]
            [service.log :as log :refer [logging-exceptions]]))

(defn- dispose-one [value]
  (logging-exceptions "disposal-loop"
    (when (r/disposable? value)
      (r/dispose value))))

(defn- disposal-loop
  [in]
  (let [ctrl (a/chan 1)]
    (a/go-loop []
       (let [[v ch] (a/alts! [in ctrl])]
         (when v
           (dispose-one v)
           (recur))))
    ctrl))

(defrecord Disposal [queue control-chan]
  component/Lifecycle
  (start [this]
    (if control-chan
      this
      (assoc this :control-chan (disposal-loop queue))))

  (stop [this]
    (if control-chan
      (do
        (a/close! control-chan)
        (assoc this :control-chan nil)))))

(defn disposal-message [v] v)

(defn disposal-subsystem [disposal-queue] (->Disposal disposal-queue nil))

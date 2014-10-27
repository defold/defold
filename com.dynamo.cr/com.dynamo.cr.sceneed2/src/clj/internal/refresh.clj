(ns internal.refresh
  (:require [clojure.core.async :as a]
            [com.stuartsierra.component :as component]
            [dynamo.resource :as r]
            [dynamo.types :as t]
            [service.log :as log :refer [logging-exceptions]]))

(defn- refresh-one
  [{:keys [graph node output]}]
  (logging-exceptions "refresh-loop"
     (t/get-value node graph output)))

(defn refresh-loop
  [in]
  (let [ctrl (a/chan 1)]
    (a/go-loop []
      (let [[v ch] (a/alts! [in ctrl])]
        (when v
          (refresh-one v)
          (recur))))
    ctrl))

(defrecord Refresh
  [queue control-chan]
  component/Lifecycle
  (start [this]
    (if control-chan
      this
      (assoc this :control-chan (refresh-loop queue))))

  (stop [this]
    (if control-chan
      (do
        (a/close! control-chan)
        (assoc this :control-chan nil)))))

(defn refresh-message
  [node g output]
  {:graph   g
   :node    node
   :output  output})

(defn refresh-subsystem
  [refresh-queue]
  (->Refresh refresh-queue nil))

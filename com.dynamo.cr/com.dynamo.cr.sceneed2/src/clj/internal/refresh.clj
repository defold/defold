(ns internal.refresh
  (:require [clojure.core.async :as a]
            [com.stuartsierra.component :as component]
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
  [queue control-chans refresh-parallelism]
  component/Lifecycle
  (start [this]
    (if control-chans
      this
      (assoc this :control-chans (mapv (fn [_] (refresh-loop queue)) (range refresh-parallelism)))))

  (stop [this]
    (if control-chans
      (do
        (doseq [ch control-chans]
          (a/close! ch))
        (assoc this :control-chans nil)))))

(defn refresh-message
  [node g output]
  {:graph   g
   :node    node
   :output  output})

(defn refresh-subsystem
  [refresh-queue parallelism]
  (->Refresh refresh-queue nil parallelism))

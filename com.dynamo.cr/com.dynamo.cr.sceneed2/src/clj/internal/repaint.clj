(ns internal.repaint
  (:require [com.stuartsierra.component :as component]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [internal.graph.dgraph :as dg]
            [internal.query :as iq]
            [service.log :as log])
  (:import [org.eclipse.swt.widgets Display]))

(def expected-frame-rate 60)
(def default-frame-interval (int (/ 1000 expected-frame-rate)))

(defn schedule-display-tick
  [world-ref waiters delay]
  (ui/tick
    delay
    (fn []
      (dosync
        (let [g (:graph @world-ref)]
          (doseq [w @waiters]
            (try
              (t/frame (iq/node-by-id world-ref w))
              (catch Throwable t
                (log/error :exception t :message "Error sending frame message to " w))))
          (alter waiters empty))))))

(defrecord Repaint [waiters delay paint-loop]
  component/Lifecycle
  (start [this]
    (when-not (-> this :world :state)
      (log/error :message "World-ref is not set in repaint loop. Repaint errors are likely."))
    (if (:paint-loop this)
      this
      (assoc this :paint-loop (schedule-display-tick (-> this :world :state) waiters delay))))

  (stop [this]
    (if (:paint-loop this)
      (do
        (ui/untick paint-loop)
        (assoc this :paint-loop nil))
      this)))

(defn repaint-subsystem
  ([repaint-needed]
    (repaint-subsystem repaint-needed default-frame-interval))
  ([repaint-needed delay]
    (Repaint. repaint-needed delay nil)))

(defn schedule-repaint
  [repaint-needed node-or-nodes]
  (dosync
    (alter repaint-needed into (if (coll? node-or-nodes) (mapv :_id node-or-nodes) (:_id node-or-nodes)))))

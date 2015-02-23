(ns internal.repaint
  (:require [com.stuartsierra.component :as component]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [internal.disposal :as disp]
            [internal.graph.dgraph :as dg]
            [service.log :as log]))

(def expected-frame-rate 60)
(def default-frame-interval (int (/ 1000 expected-frame-rate)))

(defn repaint-waiting
  [_ world-ref waiters]
  (dosync
   (doseq [w @waiters]
     (try
       (t/frame (ds/node world-ref w))
       (catch Throwable t
         (log/error :exception t :message "Error sending frame message to " w))))
   (ref-set waiters #{})))

(defrecord Repaint [waiters]
  component/Lifecycle
  (start [this]
    (when-not (-> this :world :state)
      (log/error :message "World-ref is not set in repaint loop. Repaint errors are likely."))
    (if (:timer this)
      this
      (assoc this :timer (.start (ui/make-animation-timer repaint-waiting (-> this :world :state) waiters)))))

  (stop [this]
    (if (:timer this)
      (do
        (.stop (:timer this))
        (assoc this :timer this))
      this)))

(defn repaint-subsystem
  [repaint-needed]
  (Repaint. repaint-needed))

(defn schedule-repaint
  [repaint-needed node-or-nodes]
  (dosync
    (alter repaint-needed into (if (coll? node-or-nodes) (mapv :_id node-or-nodes) (:_id node-or-nodes)))))

(ns internal.bus
  (:require [clojure.core.async :as a]))

(defn address-to
  [node body]
  (merge body {::node-id (:_id node)}))

(defn publish
  [{publish-to :publish-to} msg]
  (a/put! publish-to msg))

(defn publish-all
  [bus msgs]
  (doseq [s msgs]
    (publish bus s)))

(defn subscribe
  [{subscribe-to :subscribe-to} node-id]
  (a/sub subscribe-to node-id (a/chan 100)))

(defn make-bus
  []
  (let [pubch (a/chan 100)]
    {:publish-to   pubch
     :subscribe-to (a/pub pubch ::node-id (fn [_] (a/dropping-buffer 100)))}))
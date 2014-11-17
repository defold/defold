(ns internal.metrics)

(def counters (agent {}))

(def ^:private safe-inc (fnil inc 1))

(defn reset-counters!   []    (send counters (constantly {})))
(defn increment-counter [ks]  (send counters update-in ks safe-inc))
(defn node-value        [n v] (increment-counter [:get-node-value (:_id n) v]))

(def paint-request  (partial increment-counter [:paint-request]))
(def paint          (partial increment-counter [:paint]))
(def paint-complete (partial increment-counter [:paint-complete]))

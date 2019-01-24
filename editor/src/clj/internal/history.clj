(ns internal.history
  (:require [clojure.set :as set]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.transaction :as it]
            [internal.util :as util])
  (:import (java.util.concurrent.atomic AtomicLong)))

(set! *warn-on-reflection* true)

(defmulti ^:private replay-in-graph?
  (fn [_graph-id transaction-step]
    (case (:type transaction-step)
      (:label :sequence-label)
      :always-repeated-transaction-step

      :callback
      :always-ignored-transaction-step

      :update-graph-value
      :graph-id-transaction-step

      (:become :clear-property :delete-node :invalidate :update-property)
      :node-id-transaction-step

      (:new-override :override)
      :root-id-transaction-step

      (:connect :disconnect)
      :connection-transaction-step

      :create-node
      :create-node-transaction-step

      :override-node
      :override-node-transaction-step

      :transfer-overrides
      :transfer-overrides-transaction-step)))

(defmethod replay-in-graph? :always-repeated-transaction-step [_graph-id _transaction-step]
  true)

(defmethod replay-in-graph? :always-ignored-transaction-step [_graph-id _transaction-step]
  false)

(defmethod replay-in-graph? :graph-id-transaction-step [graph-id transaction-step]
  (= graph-id (:graph-id transaction-step)))

(defmethod replay-in-graph? :node-id-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:node-id transaction-step))))

(defmethod replay-in-graph? :root-id-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:root-id transaction-step))))

(defmethod replay-in-graph? :connection-transaction-step [graph-id transaction-step]
  (= graph-id
     (gt/node-id->graph-id (:source-id transaction-step))
     (gt/node-id->graph-id (:target-id transaction-step))))

(defmethod replay-in-graph? :create-node-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (gt/node-id (:node transaction-step)))))

(defmethod replay-in-graph? :override-node-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:original-node-id transaction-step))))

(defmethod replay-in-graph? :transfer-overrides-transaction-step [graph-id transaction-step]
  (= graph-id (-> transaction-step :from-id->to-id ffirst gt/node-id->graph-id)))

;; -----------------------------------------------------------------------------

(defrecord HistoryEntry [^long undo-group graph-after outputs-modified transaction-steps])

(defn make-history [graph]
  (assert (ig/graph? graph))
  [(->HistoryEntry 0 graph #{} [])])

(defn history-sequence-label [history]
  (let [^HistoryEntry history-entry (peek history)]
    (when (zero? (.undo-group history-entry))
      (:tx-sequence-label (.graph-after history-entry)))))

(defonce ^:private ^AtomicLong undo-group-counter (AtomicLong. 0))

(defn find-undoable-entry [history]
  (loop [history-entry-index (dec (count history))
         current-undo-group 0]
    (when (pos? history-entry-index)
      (let [^HistoryEntry history-entry (history history-entry-index)
            history-entry-undo-group (.undo-group history-entry)]
        (if (zero? history-entry-undo-group)
          {history-entry-index (if (zero? current-undo-group)
                                 (.incrementAndGet undo-group-counter)
                                 current-undo-group)}
          (recur (dec history-entry-index)
                 history-entry-undo-group))))))

(defn find-redoable-entry [history]
  (loop [history-entry-index (dec (count history))
         current-undo-group 0]
    (when-not (neg? history-entry-index)
      (let [^HistoryEntry history-entry (history history-entry-index)
            history-entry-undo-group (.undo-group history-entry)]
        (if (zero? current-undo-group)
          (when-not (zero? history-entry-undo-group)
            (recur (dec history-entry-index)
                   history-entry-undo-group))
          (if (not= current-undo-group history-entry-undo-group)
            {(inc history-entry-index) 0}
            (recur (dec history-entry-index)
                   history-entry-undo-group)))))))

(defn write-history [history graph-after outputs-modified tx-data]
  (let [graph-id (:_graph-id graph-after)
        merge-into-previous-history-entry? (some-> graph-after :tx-sequence-label (= (history-sequence-label history)))
        xform-tx-data->transaction-steps (comp (mapcat flatten)
                                               (filter (partial replay-in-graph? graph-id)))]
    (if merge-into-previous-history-entry?
      (let [^HistoryEntry previous-history-entry (peek history)
            merged-transaction-steps (into (:transaction-steps previous-history-entry) xform-tx-data->transaction-steps tx-data)
            merged-outputs-modified (into (:outputs-modified previous-history-entry) outputs-modified)
            merged-history-entry (->HistoryEntry (.undo-group previous-history-entry) graph-after merged-outputs-modified merged-transaction-steps)]
        (assoc history (dec (count history)) merged-history-entry))
      (let [transaction-steps (into [] xform-tx-data->transaction-steps tx-data)
            history-entry (->HistoryEntry 0 graph-after outputs-modified transaction-steps)]
        (conj history history-entry)))))

(defn rewind-history [history basis history-entry-index]
  (let [rewound-history-end-index (inc history-entry-index)
        rewound-history (into [] (subvec history 0 rewound-history-end-index))
        rewound-graph (:graph-after (peek rewound-history))
        rewound-outputs (transduce (map :outputs-modified) set/union (subvec history rewound-history-end-index))
        {hydrated-basis :basis hydrated-outputs :outputs-to-refresh} (ig/hydrate-after-undo basis rewound-graph)
        outputs-to-refresh (into rewound-outputs hydrated-outputs)
        invalidated-labels-by-node-id (util/group-into #{} first second outputs-to-refresh)
        rewound-basis (ig/update-successors hydrated-basis invalidated-labels-by-node-id)]
    {:basis rewound-basis
     :history rewound-history
     :outputs-to-refresh outputs-to-refresh}))

(defn- replay-history [rewound-history rewound-basis history undo-group-by-history-entry-index node-id-generators override-id-generator]
  (loop [altered-basis rewound-basis
         altered-history (transient rewound-history)
         outputs-to-refresh (transient #{})
         history-entry-index (count rewound-history)]
    (if-some [^HistoryEntry history-entry (get history history-entry-index)]
      (let [undo-group (get undo-group-by-history-entry-index history-entry-index (.undo-group history-entry))
            graph-id (:_graph-id (:graph-after history-entry))
            transaction-steps (:transaction-steps history-entry)]
        (if (zero? undo-group)
          (let [transaction-context (it/new-transaction-context altered-basis node-id-generators override-id-generator)
                {:keys [basis outputs-modified]} (it/transact* transaction-context transaction-steps)
                altered-graph (-> basis :graphs (get graph-id))
                altered-history-entry (->HistoryEntry 0 altered-graph outputs-modified transaction-steps)]
            (recur basis
                   (conj! altered-history altered-history-entry)
                   (reduce conj! outputs-to-refresh outputs-modified)
                   (inc history-entry-index)))
          (let [altered-graph (-> altered-basis :graphs (get graph-id))
                altered-history-entry (->HistoryEntry undo-group altered-graph #{} transaction-steps)]
            (recur altered-basis
                   (conj! altered-history altered-history-entry)
                   outputs-to-refresh
                   (inc history-entry-index)))))
      {:basis altered-basis
       :history (persistent! altered-history)
       :outputs-to-refresh (persistent! outputs-to-refresh)})))

(defn alter-history [history basis undo-group-by-history-entry-index node-id-generators override-id-generator]
  (let [min-history-entry-index (reduce min (keys undo-group-by-history-entry-index))]
    (assert (pos? min-history-entry-index) "Cannot alter the beginning of history.")
    (let [{rewound-basis :basis
           rewound-history :history
           rewound-outputs :outputs-to-refresh} (rewind-history history basis (dec min-history-entry-index))
          {altered-basis :basis
           altered-history :history
           altered-outputs :outputs-to-refresh} (replay-history rewound-history rewound-basis history undo-group-by-history-entry-index node-id-generators override-id-generator)]
      {:basis altered-basis
       :history altered-history
       :outputs-to-refresh (set/union rewound-outputs altered-outputs)})))

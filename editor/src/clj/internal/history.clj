(ns internal.history
  (:require [clojure.set :as set]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.util :as util]))

(defmulti ^:private replay-in-graph?
  (fn [_graph-id transaction-step]
    (case (:type transaction-step)
      (:callback :label :sequence-label)
      :ignored-transaction-step

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
      :transfer-overrides-transaction-step

      :update-graph-value
      :update-graph-value-transaction-step)))

(defmethod replay-in-graph? :ignored-transaction-step [_graph-id _transaction-step]
  false)

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

(defmethod replay-in-graph? :update-graph-value-transaction-step [graph-id transaction-step]
  (= graph-id (:graph-id transaction-step)))

;; -----------------------------------------------------------------------------

(defrecord HistoryEntry [enabled? graph-after outputs-modified tx-data])

(defn make-history [graph]
  [(->HistoryEntry true graph #{} [])])

(defn add-history-entry [history graph-after outputs-modified tx-data]
  (let [graph-id (:_graph-id graph-after)
        replay-in-graph? (partial replay-in-graph? graph-id)
        filtered-tx-data (filterv replay-in-graph? tx-data)
        history-entry (->HistoryEntry true graph-after outputs-modified filtered-tx-data)]
    (conj history history-entry)))

(defn- rewind [basis history history-entry-index]
  (assert (pos? history-entry-index) "Cannot rewind past the beginning of history.")
  (let [rewound-history (subvec history 0 history-entry-index)
        rewound-graph (:graph-after (peek rewound-history))
        rewound-outputs (transduce (map :outputs-modified) set/union (subvec history history-entry-index))
        {hydrated-basis :basis hydrated-outputs :outputs-to-refresh} (ig/hydrate-after-undo basis rewound-graph)
        outputs-to-refresh (into rewound-outputs hydrated-outputs)
        invalidated-labels-by-node-id (util/group-into #{} first second outputs-to-refresh)
        rewound-basis (ig/update-successors hydrated-basis invalidated-labels-by-node-id)]
    {:basis rewound-basis
     :history rewound-history
     :outputs-to-refresh outputs-to-refresh}))

(defn- replay [rewound-basis rewound-history history enabled-by-history-entry-index graph-id node-id-generators override-id-generator]
  (loop [altered-basis rewound-basis
         altered-history (transient (into [] rewound-history))
         outputs-to-refresh (transient #{})
         history-entry-index (count altered-history)]
    (if-some [history-entry (get history history-entry-index)]
      (let [enabled? (get (enabled-by-history-entry-index history-entry-index) (:enabled? history-entry))
            tx-data (:tx-data history-entry)]
        (if enabled?
          (let [transaction-context (it/new-transaction-context altered-basis node-id-generators override-id-generator)
                {:keys [basis outputs-modified]} (it/transact* transaction-context tx-data)
                altered-history-entry (->HistoryEntry true (-> basis :graphs graph-id) outputs-modified tx-data)]
            (recur basis
                   (conj! altered-history altered-history-entry)
                   (reduce conj! outputs-to-refresh outputs-modified)
                   (inc history-entry-index)))
          (let [altered-history-entry (->HistoryEntry false (-> altered-basis :graphs graph-id) #{} tx-data)]
            (recur altered-basis
                   (conj! altered-history altered-history-entry)
                   outputs-to-refresh
                   (inc history-entry-index)))))
      {:basis altered-basis
       :history (persistent! altered-history)
       :outputs-to-refresh (persistent! outputs-to-refresh)})))

(defn- alter-history [graphs history enabled-by-history-entry-index graph-id node-id-generators override-id-generator]
  (assert (util/map-of? util/natural-number? ig/graph? graphs))
  (let [min-history-entry-index (reduce min (keys enabled-by-history-entry-index))
        basis (ig/multigraph-basis graphs)
        {rewound-basis :basis
         rewound-history :history
         rewound-outputs :outputs-to-refresh} (rewind basis history min-history-entry-index)
        {altered-basis :basis
         altered-history :history
         altered-outputs :outputs-to-refresh} (replay rewound-basis rewound-history history enabled-by-history-entry-index graph-id node-id-generators override-id-generator)]
    {:basis altered-basis
     :history altered-history
     :outputs-to-refresh (set/union rewound-outputs altered-outputs)}))

(defn alter-history! [system graph-id enabled-by-history-entry-index]
  (let [graph-refs (is/graphs system)
        history-ref (is/graph-history system graph-id)
        node-id-generators (is/id-generators system)
        override-id-generator (is/override-id-generator system)]
    (dosync
      (let [history (deref history-ref)
            graphs (util/map-vals deref graph-refs)
            {altered-basis :basis
             altered-history :history
             altered-outputs :outputs-to-refresh} (alter-history graphs history enabled-by-history-entry-index graph-id node-id-generators override-id-generator)
            altered-graph (-> altered-basis :graphs graph-id)
            graph-ref (graph-refs graph-id)]
        (ref-set graph-ref altered-graph)
        (ref-set history-ref altered-history)
        (is/invalidate-outputs! system altered-outputs)
        altered-outputs))))

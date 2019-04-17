(ns internal.history
  (:refer-clojure :exclude [alter])
  (:require [clojure.set :as set]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.transaction :as it]
            [internal.util :as util])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(defmulti replay-in-graph?
  (fn [_graph-id transaction-step]
    (let [type (:type transaction-step)]
      (case type
        (:become :clear-property :delete-node :invalidate :update-property)
        :node-id-transaction-step

        (:connect :disconnect)
        :connection-transaction-step

        (:new-override :override)
        :root-id-transaction-step

        type))))

(defmethod replay-in-graph? :default [_graph-id _transaction-step]
  false)

(defmethod replay-in-graph? :node-id-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:node-id transaction-step))))

(defmethod replay-in-graph? :root-id-transaction-step [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:root-id transaction-step))))

(defmethod replay-in-graph? :connection-transaction-step [graph-id transaction-step]
  (= graph-id
     (gt/node-id->graph-id (:source-id transaction-step))
     (gt/node-id->graph-id (:target-id transaction-step))))

(defmethod replay-in-graph? :create-node [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (gt/node-id (:node transaction-step)))))

(defmethod replay-in-graph? :override-node [graph-id transaction-step]
  (= graph-id (gt/node-id->graph-id (:original-node-id transaction-step))))

(defmethod replay-in-graph? :transfer-overrides [graph-id transaction-step]
  (= graph-id (-> transaction-step :from-id->to-id ffirst gt/node-id->graph-id)))

(defmethod replay-in-graph? :update-graph-value [graph-id transaction-step]
  (= graph-id (:graph-id transaction-step)))

;; -----------------------------------------------------------------------------
;; Internals
;; -----------------------------------------------------------------------------

(defonce ^:private ^AtomicLong undo-group-counter (AtomicLong. 0))

(defrecord ^:private HistoryEntry [^long undo-group label sequence-label context-before context-after graph-after outputs-modified transaction-steps])

(defn- history-sequence-label [history]
  (let [^HistoryEntry history-entry (peek history)]
    (when (zero? (.undo-group history-entry))
      (.sequence-label history-entry))))

(defn- find-undoable-entry
  "Finds the first undoable entry whose context matches the supplied predicate.
  The predicate should return true for contexts that are conceptually part of
  the undo queue for the current state of the application. Returns a single-
  entry map in undo-group-by-history-entry-index format suitable for use with
  the alter function, or nil if there was no matching entry."
  [history context-pred]
  (loop [history-entry-index (dec (count history))
         current-undo-group nil]
    (when (pos? history-entry-index)
      (let [^HistoryEntry history-entry (history history-entry-index)]
        (if-not (context-pred (.context-after history-entry))
          (recur (dec history-entry-index)
                 current-undo-group)
          (let [history-entry-undo-group (.undo-group history-entry)]
            (if (zero? history-entry-undo-group)
              {history-entry-index current-undo-group}
              (recur (dec history-entry-index)
                     (or current-undo-group history-entry-undo-group)))))))))

(defn- find-redoable-entry
  "Finds the first redoable entry whose context matches the supplied predicate.
  The predicate should return true for contexts that are conceptually part of
  the undo queue for the current state of the application. Returns a single-
  entry map in undo-group-by-history-entry-index format suitable for use with
  the alter function, or nil if there was no matching entry."
  [history context-pred]
  (let [history-entry-in-context? #(context-pred (.context-after ^HistoryEntry %))]
    (when-some [^HistoryEntry last-history-entry-in-context (util/first-where history-entry-in-context? (rseq (subvec history 1)))]
      (let [current-undo-group (.undo-group last-history-entry-in-context)]
        (when-not (zero? current-undo-group)
          (let [history-entry-index (util/first-index-where (fn [^HistoryEntry history-entry]
                                                              (and (= current-undo-group (.undo-group history-entry))
                                                                   (history-entry-in-context? history-entry)))
                                                            history)]
            {history-entry-index 0}))))))

(defn- process-tx-data [tx-data graph-id]
  (loop [unfiltered-transaction-steps (flatten tx-data)
         filtered-transaction-steps (transient [])
         label nil
         sequence-label nil]
    (if-some [{:keys [type] :as transaction-step} (first unfiltered-transaction-steps)]
      (recur (next unfiltered-transaction-steps)
             (if (replay-in-graph? graph-id transaction-step)
               (conj! filtered-transaction-steps transaction-step)
               filtered-transaction-steps)
             (if (= :label type)
               (:label transaction-step)
               label)
             (if (= :sequence-label type)
               (:label transaction-step)
               sequence-label))
      {:label label
       :sequence-label sequence-label
       :transaction-steps (persistent! filtered-transaction-steps)})))

(defn- rewind-history [history basis history-entry-index]
  (let [rewound-history-end-index (inc history-entry-index)
        rewound-history (into [] (subvec history 0 rewound-history-end-index))
        rewound-graph (:graph-after (peek rewound-history))
        rewound-outputs (transduce (map :outputs-modified) set/union (subvec history rewound-history-end-index))
        {hydrated-basis :basis hydrated-outputs :outputs-to-refresh} (ig/hydrate-after-undo basis rewound-graph)
        outputs-to-refresh (into rewound-outputs hydrated-outputs)
        invalidated-labels-by-node-id (util/group-into {} #{} first second outputs-to-refresh)
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
      (let [graph-id (:_graph-id (.graph-after history-entry))
            label (.label history-entry)
            sequence-label (.sequence-label history-entry)
            context-before (.context-before history-entry)
            context-after (.context-after history-entry)
            transaction-steps (.transaction-steps history-entry)
            ^long undo-group (or (get undo-group-by-history-entry-index history-entry-index
                                      (.undo-group history-entry))
                                 (.incrementAndGet undo-group-counter))]
        (if (zero? undo-group)
          (let [transaction-context (it/new-transaction-context altered-basis node-id-generators override-id-generator)
                {:keys [basis outputs-modified]} (it/transact* transaction-context transaction-steps)
                altered-graph (-> basis :graphs (get graph-id))
                altered-history-entry (->HistoryEntry 0 label sequence-label context-before context-after altered-graph outputs-modified transaction-steps)]
            (recur basis
                   (conj! altered-history altered-history-entry)
                   (reduce conj! outputs-to-refresh outputs-modified)
                   (inc history-entry-index)))
          (let [altered-graph (-> altered-basis :graphs (get graph-id))
                altered-history-entry (->HistoryEntry undo-group label sequence-label context-before context-after altered-graph #{} transaction-steps)]
            (recur altered-basis
                   (conj! altered-history altered-history-entry)
                   outputs-to-refresh
                   (inc history-entry-index)))))
      {:basis altered-basis
       :history (persistent! altered-history)
       :outputs-to-refresh (persistent! outputs-to-refresh)})))

;; -----------------------------------------------------------------------------
;; Public interface
;; -----------------------------------------------------------------------------

(defn history? [value]
  (and (vector? value)
       (not-empty value)
       (instance? HistoryEntry (first value))))

(defn make [graph label]
  (assert (ig/graph? graph))
  (assert (string? label))
  [(map->HistoryEntry {:undo-group 0
                       :graph-after graph
                       :label label
                       :outputs-modified #{}
                       :transaction-steps []})])

(defn write [history context-before context-after graph-after outputs-modified tx-data]
  (assert (ig/graph? graph-after))
  (assert (set? outputs-modified))
  (let [{:keys [label sequence-label transaction-steps]} (process-tx-data tx-data (:_graph-id graph-after))
        merge-into-previous-history-entry? (some-> sequence-label (= (history-sequence-label history)))]
    (if merge-into-previous-history-entry?
      (let [last-history-entry-index (dec (count history))
            last-history-entry (history last-history-entry-index)
            merged-history-entry (cond-> (assoc last-history-entry :context-after context-after)
                                         (some? label) (assoc :label label)
                                         (seq outputs-modified) (update :outputs-modified into outputs-modified)
                                         (seq transaction-steps) (update :transaction-steps into transaction-steps))]
        (assert (zero? (:undo-group last-history-entry)))
        (assoc history last-history-entry-index merged-history-entry))
      (let [history-entry (->HistoryEntry 0 label sequence-label context-before context-after graph-after outputs-modified transaction-steps)]
        (conj history history-entry)))))

(defn alter [history basis undo-group-by-history-entry-index node-id-generators override-id-generator]
  (let [history-entry-indices (keys undo-group-by-history-entry-index)
        max-history-entry-index (reduce max history-entry-indices)
        min-history-entry-index (reduce min history-entry-indices)]
    (assert (pos? min-history-entry-index) "Cannot alter the beginning of history.")
    (let [{rewound-basis :basis
           rewound-history :history
           rewound-outputs :outputs-to-refresh} (rewind-history history basis (dec min-history-entry-index))
          {altered-basis :basis
           altered-history :history
           altered-outputs :outputs-to-refresh} (replay-history rewound-history rewound-basis history undo-group-by-history-entry-index node-id-generators override-id-generator)]
      {:basis altered-basis
       :context-after (:context-after (history max-history-entry-index))
       :context-before (:context-before (history min-history-entry-index))
       :history altered-history
       :outputs-to-refresh (set/union rewound-outputs altered-outputs)})))

(defn reset [history basis history-entry-index]
  (let [{:keys [context-after context-before]} (history history-entry-index)]
    (assoc (rewind-history history basis history-entry-index)
      :context-after context-after
      :context-before context-before)))

(defn cancel [history basis sequence-label]
  (assert (some? sequence-label))
  (when (= sequence-label (history-sequence-label history))
    (let [history-length (count history)]
      (reset history basis (- history-length 2)))))

(defn first-undoable [history context-pred]
  (when-some [[history-entry-index undo-group] (first (find-undoable-entry history context-pred))]
    (let [{:keys [context-after context-before]} (history history-entry-index)]
      [history-entry-index undo-group context-after context-before])))

(defn first-redoable [history context-pred]
  (when-some [[history-entry-index undo-group] (first (find-redoable-entry history context-pred))]
    (let [{:keys [context-after context-before]} (history history-entry-index)]
      [history-entry-index undo-group context-before context-after])))

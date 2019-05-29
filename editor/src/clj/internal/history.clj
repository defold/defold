(ns internal.history
  (:refer-clojure :exclude [alter])
  (:require [clojure.set :as set]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.transaction :as it]
            [internal.util :as util :refer [pair]])
  (:import [internal.graph.types Arc]
           [java.util.concurrent.atomic AtomicLong]))

;; Notes:
;; :sarcs and :tarcs should be in sync.
;; Should contain the same Arcs, but grouped differently.
;; The graph with the source node owns the :sarcs entry.
;; The graph with the target node owns the :tarcs entry.
;;
;; (get-in basis [:graphs graph-id :successors]) => successors-by-node-id
;; successors-by-node-id => {node-id {label {successor-node-id #{successor-label}}}}
;; (get-in basis [:graphs graph-id :successors node-id label]) => {successor-node-id #{successor-label}}
;;
;; node-id above is a node owned by the graph.
;; successor-node-id can refer to a node in another graph.
;; When we awake a graph from history, we should clear out external successors before applying transaction steps.
;;
;; When reattaching, we must update successors for all the connections that
;; were added or removed during the transaction steps, as well as all the
;; connections that we reattach to other graphs. I.e. detached-arcs where
;; the source node still exists after the transaction steps.
;;
;; Investigate:
;; What is :successors used for? Can it affect the outcome of a property-setter,
;; for example by altering the outcome of the override traverse-fn?

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

(declare sequence-label? undo-group?)

(defn- make-history-entry
  ^HistoryEntry [undo-group label sequence-label context-before context-after graph-after outputs-modified transaction-steps]
  {:pre [(not (neg? undo-group))
         (or (nil? label) (string? label))
         (or (nil? sequence-label) (sequence-label? sequence-label))
         (ig/graph? graph-after)
         (set? outputs-modified)
         (vector? transaction-steps)]}
  (->HistoryEntry undo-group label sequence-label context-before context-after graph-after outputs-modified transaction-steps))

(defn- history-graph-id
  ^long [history]
  (let [^HistoryEntry history-entry (peek history)
        graph (.graph-after history-entry)]
    (:_graph-id graph)))

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

#_(defn- finalize [{:keys [basis history outputs-to-refresh] :as altered-things}]
    (let [graph-id (history-graph-id history)
          {hydrated-basis :basis hydrated-outputs :outputs-to-refresh} (ig/hydrate-after-undo basis graph-id)
          finalized-outputs (into outputs-to-refresh hydrated-outputs)
          invalidated-labels-by-node-id (util/group-into {} #{} first second finalized-outputs)
          finalized-basis (ig/update-successors hydrated-basis invalidated-labels-by-node-id)]
      (assoc altered-things
        :basis finalized-basis
        :outputs-to-refresh finalized-outputs)))

(defn- replace-group! [transient-groups-by-key key group]
  (if (seq group)
    (assoc! transient-groups-by-key key group)
    (dissoc! transient-groups-by-key key)))

(defn- remove-grouped-arcs-where [arc-pred unfiltered-arcs-by-label-by-node-id]
  (let [[filtered-arcs-by-label-by-node-id removed-arcs]
        (reduce-kv
          (fn [[filtered-arcs-by-label-by-node-id removed-arcs] node-id unfiltered-arcs-by-label]
            (let [[filtered-arcs-by-label removed-arcs]
                  (reduce-kv
                    (fn [[filtered-arcs-by-label removed-arcs] label unfiltered-arcs]
                      (let [[filtered-arcs removed-arcs]
                            (reduce
                              (fn [[filtered-arcs removed-arcs] arc]
                                (if (arc-pred arc)
                                  (pair filtered-arcs (conj! removed-arcs arc))
                                  (pair (conj! filtered-arcs arc) removed-arcs)))
                              (pair (transient [])
                                    removed-arcs)
                              unfiltered-arcs)]
                        (pair (replace-group! filtered-arcs-by-label label (persistent! filtered-arcs))
                              removed-arcs)))
                    (pair (transient unfiltered-arcs-by-label)
                          removed-arcs)
                    unfiltered-arcs-by-label)]
              (pair (replace-group! filtered-arcs-by-label-by-node-id node-id (persistent! filtered-arcs-by-label))
                    removed-arcs)))
          (pair (transient unfiltered-arcs-by-label-by-node-id)
                (transient []))
          unfiltered-arcs-by-label-by-node-id)]
    (pair (persistent! filtered-arcs-by-label-by-node-id)
          (persistent! removed-arcs))))

(defn- detach-graph [basis detached-graph-id]
  (assert (gt/basis? basis))
  (assert (gt/graph-id? detached-graph-id))
  (let [arc-from-graph? #(= detached-graph-id (gt/node-id->graph-id (.source-id ^Arc %)))
        other-graphs (dissoc (:graphs basis) graph-id)

        [detached-graphs detached-arcs]
        (reduce-kv
          (fn [[detached-graphs detached-arcs] graph-id graph]
            (let [[filtered-tarcs removed-arcs] (remove-grouped-arcs-where arc-from-graph? (:tarcs graph))]
              (pair (assoc-in detached-graphs [graph-id :tarcs] filtered-tarcs)
                    (into detached-arcs removed-arcs))))
          (pair other-graphs [])
          other-graphs)

        detached-basis (assoc basis :graphs detached-graphs)
        detached-outputs (into #{}
                               (map (fn [^Arc arc]
                                      [(.source-id arc) (.source-label arc)]))
                               detached-arcs)]
    {:basis detached-basis
     :detached-arcs detached-arcs
     :evicted-outputs detached-outputs})) ; TODO: Move out of here?


(defn- reattach-graph [basis graph detached-arcs]
  ;; TODO!
  )

(defn- remove-external-sarcs [graph known-external-arcs]
  ;; Remove all :sarcs that target external nodes and update :successors to
  ;; reflect this new state of affairs. The known-external-arcs should be a seq
  ;; of Arcs that will be used to limit the search.
  (let [^long graph-id (:_graph-id graph)
        node-id-in-graph? #(= graph-id (gt/node-id->graph-id %))
        arc-from-graph? #(node-id-in-graph? (.source-id ^Arc %))
        source-node-id->source-labels (util/group-into {} #{}
                                                       #(.source-id ^Arc %)
                                                       #(.source-label ^Arc %)
                                                       known-external-arcs)]
    (-> graph
        (update :sarcs
                (fn [source-node-id->source-label->arcs]
                  (reduce-kv
                    (fn [source-node-id->source-label->arcs source-node-id source-labels]
                      (update source-node-id->source-label->arcs
                              source-node-id
                              (fn [source-label->arcs]
                                (reduce
                                  (fn [source-label->arcs source-label]
                                    (update source-label->arcs
                                            source-label
                                            (fn [arcs]
                                              (into (empty arcs)
                                                    (filter arc-from-graph?)
                                                    arcs))))
                                  source-label->arcs
                                  source-labels))))
                    source-node-id->source-label->arcs
                    source-node-id->source-labels)))
        (update :successors
                (fn [source-node-id->source-label->successor-node-id->successor-labels]
                  (reduce-kv
                    (fn [source-node-id->source-label->successor-node-id->successor-labels source-node-id source-labels]
                      (update source-node-id->source-label->successor-node-id->successor-labels
                              source-node-id
                              (fn [source-label->successor-node-id->successor-labels]
                                (reduce
                                  (fn [source-label->successor-node-id->successor-labels source-label]
                                    (update source-label->successor-node-id->successor-labels
                                            source-label
                                            (fn [successor-node-id->successor-labels]
                                              (transduce
                                                (remove node-id-in-graph?)
                                                (completing dissoc! persistent!)
                                                (transient successor-node-id->successor-labels)
                                                (keys successor-node-id->successor-labels)))))
                                  source-label->successor-node-id->successor-labels
                                  source-labels))))
                    source-node-id->source-label->successor-node-id->successor-labels
                    source-node-id->source-labels))))))

(defn- rewind-history [history history-entry-index detached-arcs]
  ;; The returned graph will not have any external connections to other graphs.
  ;; Its successors will also be updated to reflect this. We must still evict
  ;; the original successors from the cache, though.
  ;; TODO! This should probably happen in detach-graph above?
  (let [rewound-history-end-index (inc history-entry-index)
        rewound-history (into [] (subvec history 0 rewound-history-end-index))
        ;; TODO: We cannot use detached-arcs here, since the historic :sarcs aren't in sync with the present :tarcs.
        rewound-history (update-in rewound-history [(dec (count rewound-history)) :graph-after] remove-external-sarcs detached-arcs)
        rewound-graph (:graph-after (peek rewound-history))
        rewound-outputs (transduce (map :outputs-modified) set/union (subvec history rewound-history-end-index))]
    {:graph rewound-graph
     :history rewound-history
     :outputs-to-refresh rewound-outputs}))

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
            ^long undo-group (or (get undo-group-by-history-entry-index
                                      history-entry-index
                                      (.undo-group history-entry))       ; Not specified - use existing undo group.
                                 (.incrementAndGet undo-group-counter))] ; Nil specified - use next available undo group.
        (if (zero? undo-group)
          (let [transaction-context (it/new-transaction-context altered-basis node-id-generators override-id-generator)
                {:keys [basis outputs-modified]} (it/transact* transaction-context transaction-steps)
                altered-graph (-> basis :graphs (get graph-id))
                altered-history-entry (make-history-entry 0 label sequence-label context-before context-after altered-graph outputs-modified transaction-steps)]
            (recur basis
                   (conj! altered-history altered-history-entry)
                   (reduce conj! outputs-to-refresh outputs-modified)
                   (inc history-entry-index)))
          (let [altered-graph (-> altered-basis :graphs (get graph-id))
                altered-history-entry (make-history-entry undo-group label sequence-label context-before context-after altered-graph #{} transaction-steps)]
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

(def sequence-label? symbol?)

(defn undo-group? [value]
  (and (integer? value)
       (not (neg? value))))

(defn make [graph label]
  (assert (and (string? label) (not-empty label)))
  [(make-history-entry 0 label nil nil nil graph #{} [])])

(defn matches-sequence-label? [history sequence-label]
  (= sequence-label (history-sequence-label history)))

(defn process-tx-data [tx-data history-graph-ids]
  (loop [transaction-steps (flatten tx-data)
         transaction-steps-by-graph-id (into {}
                                             (map (fn [graph-id]
                                                    [graph-id (transient [])]))
                                             history-graph-ids)
         label nil
         sequence-label nil]
    (if-some [{:keys [type] :as transaction-step} (first transaction-steps)]
      (recur (next transaction-steps)
             (reduce (fn [transaction-steps-by-graph-id graph-id]
                       (if (replay-in-graph? graph-id transaction-step)
                         (update transaction-steps-by-graph-id graph-id conj! transaction-step)
                         transaction-steps-by-graph-id))
                     transaction-steps-by-graph-id
                     history-graph-ids)
             (if (= :label type)
               (:label transaction-step)
               label)
             (if (= :sequence-label type)
               (:label transaction-step)
               sequence-label))
      (let [transaction-steps-by-graph-id (into {}
                                                (map (juxt key (comp persistent! val)))
                                                transaction-steps-by-graph-id)]
        {:label label
         :sequence-label sequence-label
         :transaction-steps-by-graph-id transaction-steps-by-graph-id}))))

(defn write [history context-before context-after graph-after outputs-modified transaction-steps label sequence-label]
  (assert (ig/graph? graph-after))
  (let [merge-into-previous-history-entry? (some-> sequence-label (= (history-sequence-label history)))]
    (cond
      merge-into-previous-history-entry?
      (let [last-history-entry-index (dec (count history))
            last-history-entry (history last-history-entry-index)
            merged-history-entry (cond-> (assoc last-history-entry
                                           :context-after context-after
                                           :graph-after graph-after)
                                         (some? label) (assoc :label label)
                                         (seq outputs-modified) (update :outputs-modified into outputs-modified)
                                         (seq transaction-steps) (update :transaction-steps into transaction-steps))]
        (assert (zero? (:undo-group last-history-entry)))
        (assoc history last-history-entry-index merged-history-entry))

      (seq transaction-steps)
      (let [history-entry (make-history-entry 0 label sequence-label context-before context-after graph-after (or outputs-modified #{}) transaction-steps)]
        (conj history history-entry))

      :else
      history)))

(defn alter [history basis undo-group-by-history-entry-index node-id-generators override-id-generator]
  (let [history-entry-indices (keys undo-group-by-history-entry-index)
        max-history-entry-index (reduce max history-entry-indices)
        min-history-entry-index (reduce min history-entry-indices)]
    (assert (pos? min-history-entry-index) "Cannot alter the beginning of history.")
    (let [graph-id (history-graph-id history)
          {detached-basis :basis
           detached-arcs :detached-arcs} (detach-graph basis graph-id)
          {rewound-graph :graph
           rewound-history :history
           rewound-outputs :outputs-to-refresh} (rewind-history history (dec min-history-entry-index) detached-arcs)
          {replayed-graph :graph
           replayed-history :history
           replayed-outputs :outputs-to-refresh} (replay-history rewound-history rewound-basis history undo-group-by-history-entry-index node-id-generators override-id-generator)
          {reattached-basis :basis
           reattached-outputs :outputs-to-refresh} (reattach-graph basis graph detached-arcs)]
      (finalize
        {:basis altered-basis
         :context-after (:context-after (history max-history-entry-index))
         :context-before (:context-before (history min-history-entry-index))
         :history altered-history
         :outputs-to-refresh (set/union rewound-outputs altered-outputs)}))))

(defn reset [history basis history-entry-index]
  (let [{detached-basis :basis
         detached-arcs :detached-arcs
         detached-outputs :outputs-to-refresh} (detach-graph basis graph-id)
        {rewound-graph :graph
         rewound-history :history
         rewound-outputs :outputs-to-refresh} (rewind-history history history-entry-index detached-arcs)
        rewound-graph-id (:_graph-id rewound-graph)
        rewound-basis (assoc-in basis [:graphs rewound-graph-id] rewound-graph)
        {:keys [context-after context-before]} (peek rewound-history)]
    (-> history
        (rewind-history basis history-entry-index)
        (assoc
          :context-after context-after
          :context-before context-before)
        finalize)))

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

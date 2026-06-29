;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.transaction
  "Internal functions that implement the transactional behavior."
  (:require [clojure.set :as set]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [internal.graph.types Arc]
           [java.util.concurrent.atomic AtomicInteger]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/interface TransactionStep
  (step_type []) ; Returns a keyword uniquely identifying the type of transaction step.
  (metrics_key []) ; Returns a key which identifies the subject of the transaction step in metrics reports.
  (realize [ctx undoable-changes])) ; Returns [ctx undoable-changes], where ctx now has the step applied and the TransactionChanges have been appended to the undoable-changes transient vector if supplied.

(defonce/interface TransactionChange
  (perform [ctx]) ; Returns a new ctx with the change applied.
  (revert [ctx])) ; Returns a new ctx with the change reverted.

(defonce/type NonUndoable [tx-data])

;; ---------------------------------------------------------------------------
;; Internal state
;; ---------------------------------------------------------------------------
(def ^:dynamic *tx-debug* nil)

(def ^:private ^AtomicInteger next-txid (AtomicInteger. 1))
(defn- new-txid [] (.getAndIncrement next-txid))

(defmacro txerrstr [ctx & rest]
  `(str (:txid ~ctx) ": " ~@(interpose " " rest)))

(defn non-undoable
  [tx-data]
  (->NonUndoable tx-data))

(defn non-undoable?
  [value]
  (instance? NonUndoable value))

(defn non-undoable-tx-data
  [^NonUndoable txs]
  (.tx_data txs))

(definline perform-change
  [ctx ^TransactionChange change]
  `(.perform ~(with-meta change {:tag `TransactionChange}) ~ctx))

(definline revert-change
  [ctx ^TransactionChange change]
  `(.revert ~(with-meta change {:tag `TransactionChange}) ~ctx))

(definline ^:private conj-change
  [undoable-changes ^TransactionChange transaction-change]
  `(when-let [undoable-changes# ~undoable-changes]
     (conj! undoable-changes# ~transaction-change)))

(definline ^:private perform-and-conj-change
  [ctx undoable-changes ^TransactionChange transaction-change]
  `(let [transaction-change# ~transaction-change]
     (pair (perform-change ~ctx transaction-change#)
           (conj-change ~undoable-changes transaction-change#))))

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------
(defn- mark-input-activated
  [ctx node-id input-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [basis (:basis ctx)
          dirty-deps (-> (gt/node-by-id-at basis node-id)
                         gt/node-type
                         in/input-dependencies
                         (get input-label))
          nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (into nodes-affected
              (map #(gt/endpoint node-id %))
              dirty-deps)))))

(defn- mark-output-activated
  [ctx node-id output-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (conj nodes-affected (gt/endpoint node-id output-label))))))

(defn- mark-outputs-activated
  [ctx node-id output-labels]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (into nodes-affected
              (map #(gt/endpoint node-id %))
              output-labels)))))

(defn- mark-all-outputs-activated
  [ctx node-id]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [basis (:basis ctx)
          output-labels (-> (gt/node-by-id-at basis node-id)
                            gt/node-type
                            in/output-labels)
          nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (into nodes-affected
              (map #(gt/endpoint node-id %))
              output-labels)))))

(defn- mark-arc-targets-activated
  [ctx arcs]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (into nodes-affected
              (map #(gt/endpoint (gt/target-id %) (gt/target-label %)))
              arcs)))))

(defn- next-node-id [ctx graph-id]
  (gt/next-node-id (:node-id-generators ctx) graph-id))

(defn- next-override-id [ctx graph-id]
  (gt/next-override-id (:override-id-generator ctx) graph-id))

(declare ^:private ctx-disconnect
         ^:private mark-override-originals-changed)

(defn- ctx-disconnect-arc [ctx ^Arc arc]
  (ctx-disconnect ctx (.source-id arc) (.source-label arc) (.target-id arc) (.target-label arc)))

(defn- disconnect-inputs [ctx target-id target-label]
  (reduce ctx-disconnect-arc ctx (ig/explicit-arcs-by-target (:basis ctx) target-id target-label)))

(defn- disconnect-all-inputs [ctx target-id]
  (reduce ctx-disconnect-arc ctx (ig/explicit-arcs-by-target (:basis ctx) target-id)))

(defn- disconnect-outputs [ctx source-id source-label]
  (reduce ctx-disconnect-arc ctx (ig/explicit-arcs-by-source (:basis ctx) source-id source-label)))

(defn- disconnect-stale [ctx node-id old-node new-node labels-fn disconnect-fn]
  (let [stale-labels (set/difference
                       (-> old-node gt/node-type labels-fn)
                       (-> new-node gt/node-type labels-fn))]
    (loop [ctx ctx
           labels stale-labels]
      (if-let [label (first labels)]
        (recur (disconnect-fn ctx node-id label) (rest labels))
        ctx))))

(defn- disconnect-stale-inputs [ctx node-id old-node new-node]
  (disconnect-stale ctx node-id old-node new-node in/input-labels disconnect-inputs))

(defn- disconnect-stale-outputs [ctx node-id old-node new-node]
  (disconnect-stale ctx node-id old-node new-node in/output-labels disconnect-outputs))

(defn- delete-single
  [ctx node-id]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if (nil? node) ; nil if node was deleted in this transaction
      ctx
      (let [original-id (gt/original node)
            target-arcs (ig/explicit-arcs-by-source basis node-id)]
        (-> ctx
            (mark-arc-targets-activated target-arcs)
            (disconnect-all-inputs node-id)
            (mark-all-outputs-activated node-id)
            (update :basis gt/delete-node node-id)
            (assoc-in [:nodes-deleted node-id] node)
            (update :nodes-added (partial filterv #(not= node-id %)))
            (cond-> (and original-id (gt/node-by-id-at basis original-id))
              (mark-override-originals-changed basis original-id)))))))

(defn- ctx-delete-node [ctx node-id]
  (when *tx-debug*
    (println (txerrstr ctx "deleting " node-id)))
  (let [to-delete (ig/pre-traverse (:basis ctx) [node-id] ig/cascade-delete-sources)]
    (when (and *tx-debug* (not (empty? to-delete)))
      (println (txerrstr ctx "cascading delete of " (pr-str to-delete))))
    (reduce delete-single ctx to-delete)))

(defn- ctx-perform-add-override
  [ctx override-id root-id traverse-fn init-props-fn]
  (let [override (ig/make-override root-id traverse-fn init-props-fn)]
    (update ctx :basis gt/add-override override-id override)))

(defn- ctx-revert-add-override
  [ctx override-id]
  (update ctx :basis gt/delete-override override-id))

(defn- flag-all-successors-changed [ctx node-ids]
  (let [successors-changed (:successors-changed ctx)
        affected-node-ids (filterv #(get successors-changed % ::not-found) node-ids)]
    (if (coll/empty? affected-node-ids)
      ctx
      (assoc ctx
        :successors-changed
        (persistent!
          (reduce
            (fn [successors-changed node-id]
              (assoc! successors-changed node-id nil))
            (transient successors-changed)
            affected-node-ids))))))

(defn- flag-successors-changed [ctx changes]
  (let [successors-changed (:successors-changed ctx)

        affected-node-id+label-pairs
        (filterv (fn [[node-id label]]
                   (let [old-affected-node-labels (get successors-changed node-id ::not-found)]
                     (case old-affected-node-labels
                       nil false ; Found nil - all node labels already flagged as changed. We can skip this pair.
                       ::not-found true ; Nothing is flagged for this node yet. We should process this pair.
                       (not (contains? old-affected-node-labels label))))) ; Process this pair if the label has not been flagged for the node yet.
                 changes)]

    (if (coll/empty? affected-node-id+label-pairs)
      ctx
      (assoc ctx
        :successors-changed
        (persistent!
          (reduce
            (fn [successors-changed [node-id label]]
              (assoc! successors-changed
                node-id (if-let [old-affected-node-labels (get successors-changed node-id)]
                          (conj old-affected-node-labels label)
                          #{label})))
            (transient successors-changed)
            affected-node-id+label-pairs))))))

(defn- mark-override-originals-changed [ctx basis original-node-id]
  (if (:full-invalidation ctx)
    ctx
    (let [all-originals (ig/override-originals basis original-node-id)]
      (-> ctx
          ;; Any property, input or output on any original nodes must now take
          ;; the updated override node set into account.
          (flag-all-successors-changed all-originals)

          ;; Similarly, so must the source outputs of any arcs that target any
          ;; of the original nodes.
          (flag-successors-changed (e/mapcat #(gt/sources basis %) all-originals))))))

(defn- ctx-perform-override-node [ctx original-node-id override-node-id]
  (assert (= (gt/node-id->graph-id original-node-id) (gt/node-id->graph-id override-node-id))
          "Override nodes must belong to the same graph as the original")
  (let [basis (:basis ctx)
        ctx (assoc ctx :basis (gt/override-node basis original-node-id override-node-id))]
    (mark-override-originals-changed ctx basis original-node-id)))

(defn- ctx-revert-override-node [ctx original-node-id override-node-id]
  (let [basis (:basis ctx)]
    (-> ctx
        (update-in [:basis :graphs (gt/node-id->graph-id original-node-id) :node->overrides original-node-id]
                   #(coll/not-empty (filterv (partial not= override-node-id) %)))
        (mark-override-originals-changed basis original-node-id))))

(declare ^:private make-delete-nodes-change)

(declare ->DeleteNodesTXC
         add-node
         realize-tx)

(defonce/type AddOverrideTXC [override-id root-id traverse-fn init-props-fn]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-add-override ctx override-id root-id traverse-fn init-props-fn))

  (revert [_this ctx]
    (ctx-revert-add-override ctx override-id)))

(defonce/type AddOverrideTXS [override-id root-id traverse-fn init-props-fn]
  TransactionStep
  (step-type [_this]
    :tx-step/add-override)

  (metrics-key [_this]
    root-id)

  (realize [_this ctx undoable-changes]
    (perform-and-conj-change ctx undoable-changes (->AddOverrideTXC override-id root-id traverse-fn init-props-fn))))

(defn- add-override
  [override-id root-id traverse-fn init-props-fn]
  [(->AddOverrideTXS override-id root-id traverse-fn init-props-fn)])

(defonce/type OverrideNodeTXC [original-node-id override-node-id]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-override-node ctx original-node-id override-node-id))

  (revert [_this ctx]
    (ctx-revert-override-node ctx original-node-id override-node-id)))

(defonce/type OverrideNodeTXS [original-node-id override-node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/override-node)

  (metrics-key [_this]
    original-node-id)

  (realize [_this ctx undoable-changes]
    (perform-and-conj-change ctx undoable-changes (->OverrideNodeTXC original-node-id override-node-id))))

(defn- override-node
  [original-node-id override-node-id]
  [(->OverrideNodeTXS original-node-id override-node-id)])

(defn- realize-override
  [ctx undoable-changes root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  (let [basis (:basis ctx)
        graph-id (gt/node-id->graph-id root-id)
        node-ids (ig/pre-traverse basis [root-id] traverse-fn)
        override-id (next-override-id ctx graph-id)
        override-nodes (mapv (fn [original-node-id]
                               (let [override-node-id (next-node-id ctx graph-id)
                                     original-node (gt/node-by-id-at basis original-node-id)
                                     node-type (gt/node-type original-node)
                                     init-props (when init-props-fn
                                                  (init-props-fn basis original-node-id node-type))
                                     properties (coll/merge
                                                  init-props
                                                  (properties-by-node-id original-node-id))]
                                 (in/make-override-node override-id override-node-id node-type original-node-id properties)))
                             node-ids)
        override-node-ids (map gt/node-id override-nodes)
        original-node-id->override-node-id (zipmap node-ids override-node-ids)
        add-override-nodes-tx-data (map add-node override-nodes)
        add-override-tx-data (concat
                               (add-override override-id root-id traverse-fn init-props-fn)
                               (map
                                 (fn [node-id override-node-id]
                                   (override-node node-id override-node-id))
                                 node-ids
                                 override-node-ids))
        creation-tx-data (concat add-override-nodes-tx-data add-override-tx-data)
        [ctx undoable-changes] (realize-tx ctx creation-tx-data undoable-changes)
        init-fn-tx-data (init-fn (in/custom-evaluation-context {:basis (:basis ctx) :tx-data-context (:tx-data-context ctx)})
                                 original-node-id->override-node-id)]
    (realize-tx ctx init-fn-tx-data undoable-changes)))

(defn- node-id->override-id [basis node-id]
  (->> node-id
       (gt/node-by-id-at basis)
       gt/override-id))

(defn- realize-make-override-nodes [ctx undoable-changes override-id node-ids init-props-fn]
  (coll/reduce-> node-ids (pair ctx undoable-changes)
    (fn [[ctx undoable-changes] node-id]
      (let [basis (:basis ctx)]
        (if (coll/some #(= override-id (node-id->override-id basis %))
                       (ig/get-overrides basis node-id))
          (pair ctx undoable-changes)
          (let [graph-id (gt/node-id->graph-id node-id)
                original-node (gt/node-by-id-at basis node-id)
                node-type (gt/node-type original-node)
                properties (when init-props-fn
                             (init-props-fn basis node-id node-type))
                new-override-node-id (next-node-id ctx graph-id)
                new-override-node (in/make-override-node
                                    override-id
                                    new-override-node-id
                                    node-type
                                    node-id
                                    properties)
                tx-data [(add-node new-override-node)
                         (override-node node-id new-override-node-id)]]
            (realize-tx ctx tx-data undoable-changes)))))))

(defn- realize-populate-overrides [ctx undoable-changes node-id]
  (let [basis (:basis ctx)
        override-node-ids (ig/get-overrides basis node-id)
        override-node-count (count override-node-ids)]
    (loop [override-node-index 0
           ctx ctx
           undoable-changes undoable-changes
           prev-traverse-fn nil
           prev-traverse-result nil]
      (if (>= override-node-index override-node-count)
        (coll/reduce-> override-node-ids (pair ctx undoable-changes)
          (fn [[ctx undoable-changes] override-node-id]
            (realize-populate-overrides ctx undoable-changes override-node-id)))
        (let [override-node-id (override-node-ids override-node-index)
              override-id (node-id->override-id basis override-node-id)
              {:keys [init-props-fn traverse-fn]} (ig/override-by-id basis override-id)
              node-ids (if (identical? prev-traverse-fn traverse-fn)
                         prev-traverse-result
                         (when-let [source-node-ids
                                    (some-> (traverse-fn basis node-id) ; Immediate relevant source nodes connected to a :cascade-delete input.
                                            (coll/into-> []
                                              (remove
                                                (fn already-traversed? [immediate-node-id]
                                                  (coll/some
                                                    #(= override-id (node-id->override-id basis %))
                                                    (ig/get-overrides basis immediate-node-id)))))
                                            (coll/not-empty))]
                           (ig/pre-traverse basis source-node-ids traverse-fn)))
              [ctx undoable-changes] (realize-make-override-nodes ctx undoable-changes override-id node-ids init-props-fn)]
          (recur (inc override-node-index)
                 ctx
                 undoable-changes
                 traverse-fn
                 node-ids))))))

(defn realize-update-overrides
  [{:keys [override-nodes-affected-ordered] :as ctx} undoable-changes]
  (du/measuring (:metrics ctx) :update-overrides
    (coll/reduce-> override-nodes-affected-ordered (pair ctx undoable-changes)
      (fn [[ctx undoable-changes] node-id]
        (realize-populate-overrides ctx undoable-changes node-id)))))

(defn- set-override-node-ids
  [basis original-id override-node-ids]
  (reduce (fn [basis override-node-id]
            (gt/override-node basis original-id override-node-id))
          (gt/override-node-clear basis original-id)
          override-node-ids))

(defn- ctx-replace-override-node-ids
  [ctx original-id override-node-ids]
  (let [basis (:basis ctx)]
    (-> ctx
        (assoc :basis (set-override-node-ids basis original-id override-node-ids))
        (mark-override-originals-changed basis original-id))))

(defonce/type ClearOverrideNodesTXC [original-id override-node-ids]
  TransactionChange
  (perform [_this ctx]
    (ctx-replace-override-node-ids ctx original-id []))

  (revert [_this ctx]
    (ctx-replace-override-node-ids ctx original-id override-node-ids)))

(defonce/type ReplaceOverrideRootTXC [override-id old-override new-override]
  TransactionChange
  (perform [_this ctx]
    (update ctx :basis gt/replace-override override-id new-override))

  (revert [_this ctx]
    (update ctx :basis gt/replace-override override-id old-override)))

(defn- ctx-set-override-node-original
  [ctx override-node-id original-id]
  (let [basis (:basis ctx)
        override-node (gt/node-by-id-at basis override-node-id)]
    (-> ctx
        (assoc :basis (gt/replace-node basis override-node-id (gt/set-original override-node original-id)))
        (mark-all-outputs-activated override-node-id))))

(defonce/type RepointOverrideNodeTXC [override-node-id old-original-id new-original-id new-original-override-node-ids-before]
  TransactionChange
  (perform [_this ctx]
    (-> ctx
        (ctx-set-override-node-original override-node-id new-original-id)
        (ctx-perform-override-node new-original-id override-node-id)))

  (revert [_this ctx]
    (-> ctx
        (ctx-replace-override-node-ids new-original-id new-original-override-node-ids-before)
        (ctx-set-override-node-original override-node-id old-original-id))))

(defn- realize-transfer-overrides
  [ctx undoable-changes from-id->to-id]
  ;; This method updates the existing override layer to use the to-id as the
  ;; root of the override layer. It also updates the "first level" (i.e. direct)
  ;; override nodes that have from-id as their original to instead have to-id as
  ;; their original. It then deletes every other override node that was produced
  ;; from the existing override layer and re-runs the realize-populate-overrides
  ;; function for the updated graph state. This will cause any missing override
  ;; nodes to be re-created from the structure of to-id.
  (let [basis (:basis ctx)
        override-node-ids (into #{}
                                (mapcat (partial ig/get-overrides basis))
                                (keys from-id->to-id)) ; "first level" override nodes
        retained override-node-ids
        override-node-id->override-id (into {}
                                            (map (fn [override-node-id]
                                                   (pair override-node-id
                                                         (node-id->override-id basis override-node-id))))
                                            override-node-ids)
        override-id->override (into {}
                                    (comp
                                      (map val)
                                      (distinct)
                                      (map (fn [override-id]
                                             (pair override-id
                                                   (ig/override-by-id basis override-id)))))
                                    override-node-id->override-id)
        override-node-id->override (comp override-id->override override-node-id->override-id)
        overrides-to-fix (into []
                               (filter (fn [[_ override]]
                                         (contains? from-id->to-id (:root-id override))))
                               override-id->override)
        nodes-to-delete (into []
                              (comp
                                (mapcat (fn [override-node-id]
                                          (let [override (override-node-id->override override-node-id)
                                                traverse-fn (:traverse-fn override)
                                                node-ids-from-override (ig/pre-traverse basis [override-node-id] traverse-fn)]
                                            node-ids-from-override)))
                                (remove retained))
                              override-node-ids)

        ;; Clear out the original to override node-id mappings from the graph.
        ;; Normally entries are removed from this mapping inside
        ;; basis-remove-node as nodes are deleted, but since we're transferring
        ;; overrides, we must do it manually here.
        [ctx undoable-changes]
        (coll/reduce-> (keys from-id->to-id) (pair ctx undoable-changes)
          (fn [[ctx undoable-changes] from-id]
            (let [override-node-ids (ig/get-overrides basis from-id)]
              (if (coll/empty? override-node-ids)
                (pair ctx undoable-changes)
                (perform-and-conj-change ctx undoable-changes (->ClearOverrideNodesTXC from-id override-node-ids))))))

        ;; Re-root overrides that used to have a from node id as root.
        [ctx undoable-changes]
        (coll/reduce-> overrides-to-fix (pair ctx undoable-changes)
          (fn [[ctx undoable-changes] [override-id override]]
            (let [new-override (update override :root-id from-id->to-id)]
              (perform-and-conj-change ctx undoable-changes (->ReplaceOverrideRootTXC override-id override new-override)))))

        ;; Delete old nodes.
        [ctx undoable-changes]
        (if-let [change (make-delete-nodes-change ctx nodes-to-delete)]
          (perform-and-conj-change ctx undoable-changes change)
          (pair ctx undoable-changes))

        ;; * repoint the first level override nodes to use to-node as original
        ;; * add as override nodes of to-node
        [ctx undoable-changes]
        (coll/reduce-> override-node-ids (pair ctx undoable-changes)
          (fn [[ctx undoable-changes] override-node-id]
            (let [basis (:basis ctx)
                  override-node (gt/node-by-id-at basis override-node-id)
                  old-original-id (gt/original override-node)
                  new-original-id (from-id->to-id old-original-id)
                  change (->RepointOverrideNodeTXC override-node-id
                                                   old-original-id
                                                   new-original-id
                                                   (ig/get-overrides basis new-original-id))]
              (perform-and-conj-change ctx undoable-changes change))))]

    ;; Populate the fresh override layers.
    (coll/reduce-> (vals from-id->to-id) (pair ctx undoable-changes)
      (fn [[ctx undoable-changes] to-id]
        (realize-populate-overrides ctx undoable-changes to-id)))))

(defn- raw-property-assigned?
  [node property]
  (contains? (gt/assigned-properties node) property))

(defn- set-raw-property-state
  [basis node-id node property value assigned]
  (gt/replace-node
    basis
    node-id
    (if assigned
      (gt/set-property node basis property value)
      (if (gt/original node)
        (gt/clear-property node basis property)
        (dissoc node property)))))

(defn- mark-property-activated
  [ctx node-id property override-node dynamic property-overridden]
  (if override-node
    (mark-outputs-activated ctx node-id (cond-> (if dynamic [property :_properties] [property])
                                          (not property-overridden) (conj :_overridden-properties)))
    (mark-output-activated ctx node-id property)))

(defn- ctx-set-raw-property-state
  [ctx node-id property value assigned override-node dynamic property-overridden value-changed]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if (nil? node)
      ctx
      (cond-> (update ctx :basis set-raw-property-state node-id node property value assigned)
        value-changed
        (mark-property-activated node-id property override-node dynamic property-overridden)))))

(defn- call-setter-fn [ctx property setter-fn basis node-id old-value new-value]
  (try
    (let [tx-data-context (:tx-data-context ctx)
          setter-actions (setter-fn (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context}) node-id old-value new-value)]
      (when *tx-debug*
        (println (txerrstr ctx "setter actions" (seq setter-actions))))
      setter-actions)
    (catch clojure.lang.ArityException ae
      (when *tx-debug*
        (println "ArityException while inside " setter-fn " on node " node-id " with " old-value new-value (:node-type (gt/node-by-id-at basis node-id))))
      (throw ae))
    (catch Exception e
      (let [node-type (:name @(:node-type (gt/node-by-id-at basis node-id)))]
        (throw (Exception. (format "Setter of node %s (%s) %s could not be called" node-id node-type property) e))))))

(defonce/type SetRawPropertyTXC [node-id property-label old-value old-value-assigned new-value override-node dynamic property-overridden value-changed]
  TransactionChange
  (perform [_this ctx]
    (ctx-set-raw-property-state ctx node-id property-label new-value true override-node dynamic property-overridden value-changed))

  (revert [_this ctx]
    (ctx-set-raw-property-state ctx node-id property-label old-value old-value-assigned override-node dynamic property-overridden value-changed)))

(defonce/type ClearRawPropertyTXC [node-id property-label old-value old-value-assigned override-node dynamic property-overridden]
  TransactionChange
  (perform [_this ctx]
    (ctx-set-raw-property-state ctx node-id property-label nil false override-node dynamic property-overridden true))

  (revert [_this ctx]
    (ctx-set-raw-property-state ctx node-id property-label old-value old-value-assigned override-node dynamic property-overridden true)))

(defn- make-set-raw-property-change
  [node-id node property-label new-value value-changed]
  (let [node-type (gt/node-type node)
        assigned-properties (gt/assigned-properties node)
        override-node (some? (gt/original node))
        dynamic (not (contains? (in/all-properties node-type) property-label))]
    (in/validate-property-value node-type node-id property-label new-value)
    (->SetRawPropertyTXC node-id
                         property-label
                         (get assigned-properties property-label)
                         (contains? assigned-properties property-label)
                         new-value
                         override-node
                         dynamic
                         (gt/property-overridden? node property-label)
                         value-changed)))

(defn- make-clear-raw-property-change
  [ctx node-id property-label]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (when node
      (let [node-type (gt/node-type node)
            dynamic (not (contains? (in/all-properties node-type) property-label))]
        (when-not (gt/original node)
          (gt/clear-property node basis property-label))
        (->ClearRawPropertyTXC node-id
                               property-label
                               (gt/get-property node basis property-label)
                               (raw-property-assigned? node property-label)
                               (some? (gt/original node))
                               dynamic
                               (gt/property-overridden? node property-label))))))

(defn- realize-setter-actions
  [ctx undoable-changes node-id node property-label old-value new-value]
  (if-let [setter-fn (in/property-setter (gt/node-type node) property-label)]
    (let [setter-actions (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id old-value new-value)]
      (realize-tx ctx setter-actions undoable-changes))
    (pair ctx undoable-changes)))

(defn- realize-set-property
  [ctx undoable-changes node-id property-label new-value]
  (if-let [node (gt/node-by-id-at (:basis ctx) node-id)]
    (let [evaluation-context (in/custom-evaluation-context {:basis (:basis ctx)
                                                            :tx-data-context (:tx-data-context ctx)})
          old-value (in/node-property-value node property-label evaluation-context)
          value-changed (not= old-value new-value)
          change (make-set-raw-property-change node-id node property-label new-value value-changed)
          ctx (perform-change ctx change)
          undoable-changes (conj-change undoable-changes change)]
      (if value-changed
        (realize-setter-actions ctx undoable-changes node-id node property-label old-value new-value)
        (pair ctx undoable-changes)))
    (pair ctx undoable-changes)))

(defn- realize-update-property
  [ctx undoable-changes node-id property-label update-fn args opts]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if (nil? node)
      (pair ctx undoable-changes)
      (let [evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context (:tx-data-context ctx)})
            old-value (in/node-property-value node property-label evaluation-context)
            new-value (if (:inject-evaluation-context opts)
                        (apply update-fn evaluation-context old-value args)
                        (apply update-fn old-value args))]
        (realize-set-property ctx undoable-changes node-id property-label new-value)))))

(defn- realize-clear-property
  [ctx undoable-changes node-id property-label]
  (if-let [change (make-clear-raw-property-change ctx node-id property-label)]
    (let [basis (:basis ctx)
          node (gt/node-by-id-at basis node-id)
          old-value (.-old-value ^ClearRawPropertyTXC change)
          ctx (perform-change ctx change)
          undoable-changes (conj-change undoable-changes change)]
      (if-let [_setter-fn (in/property-setter (gt/node-type node) property-label)]
        (realize-setter-actions ctx undoable-changes node-id node property-label old-value nil)
        (pair ctx undoable-changes)))
    (pair ctx undoable-changes)))

(defn- realize-defaults
  [ctx undoable-changes node]
  (let [node-id (gt/node-id node)
        node-type (gt/node-type node)
        ordered-property-setter-infos (in/ordered-property-setter-infos node-type)]
    (if (coll/empty? ordered-property-setter-infos)
      (pair ctx undoable-changes)
      (let [assigned-properties (gt/assigned-properties node)
            value-fn (if (some? (gt/original node))
                       (fn override-node-value-fn [property-label _default-value]
                         (get assigned-properties property-label))
                       (fn regular-node-value-fn [property-label default-value]
                         (get assigned-properties property-label default-value)))]
        (coll/reduce-> ordered-property-setter-infos (pair ctx undoable-changes)
          (fn [[ctx undoable-changes] [property-label default-value setter-fn]]
            (if-some [property-value (value-fn property-label default-value)]
              (let [setter-actions (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id nil property-value)
                    [ctx undoable-changes] (realize-tx ctx setter-actions undoable-changes)]
                (pair ctx undoable-changes))
              (pair ctx undoable-changes))))))))

(defn- ctx-perform-add-node [ctx node]
  (let [basis-after (gt/add-node (:basis ctx) node)
        node-id (gt/node-id node)]
    (assert (gt/node-id? node-id))
    (-> ctx
        (assoc :basis basis-after)
        (update :nodes-added conj node-id)
        (assoc-in [:successors-changed node-id] nil)
        (mark-all-outputs-activated node-id))))

(defn- ctx-revert-add-node [ctx added-node]
  (let [ctx (ctx-delete-node ctx (gt/node-id added-node))
        original-id (gt/original added-node)]
    (cond-> ctx
      original-id
      (update-in
        [:basis :graphs (gt/node-id->graph-id original-id) :node->overrides]
        (fn [node->overrides]
          (if (coll/empty? (get node->overrides original-id))
            (dissoc node->overrides original-id)
            node->overrides))))))

(defn- ctx-callback
  [ctx fn args opts]
  (if (:inject-evaluation-context opts)
    (let [basis (:basis ctx)
          tx-data-context (:tx-data-context ctx)
          evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
      (apply fn evaluation-context args))
    (apply fn args))
  ctx)

(defn- flag-override-nodes-affected [ctx target-id]
  (let [override-nodes-affected-seen (:override-nodes-affected-seen ctx)]
    (if (contains? override-nodes-affected-seen target-id)
      ctx
      (let [override-nodes-affected-ordered (:override-nodes-affected-ordered ctx)]
        (assoc ctx
          :override-nodes-affected-seen (conj override-nodes-affected-seen target-id)
          :override-nodes-affected-ordered (conj override-nodes-affected-ordered target-id))))))

(defmacro ^:private assert-schema-type-compatible
  [source-id source-label output-nodetype output-valtype target-id target-label input-nodetype input-valtype]
  (when in/*check-schemas*
    `(when ~`in/*check-schemas* ; Inner check to support disabling the schema check post compile-time.
       (let [output-valtype# ~output-valtype
             input-valtype# ~input-valtype]
         (assert (in/type-compatible? output-valtype# input-valtype#)
                 (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s and %s are not have compatible types."
                         ~source-id (in/type-name ~output-nodetype) ~source-label
                         ~target-id (in/type-name ~input-nodetype) ~target-label
                         (:k output-valtype#) (:k input-valtype#)))))))

(defn- assert-type-compatible
  [source-id source-node source-label target-id target-node target-label]
  (let [output-nodetype (gt/node-type source-node)
        output-valtype (in/output-type output-nodetype source-label)
        input-nodetype (gt/node-type target-node)
        input-valtype (in/input-type input-nodetype target-label)]
    (assert output-valtype
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an output or property named %s"
                    source-id (in/type-name output-nodetype) source-label
                    target-id (in/type-name input-nodetype) target-label
                    (in/type-name output-nodetype) source-label))
    (assert input-valtype
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an input named %s"
                    source-id (in/type-name output-nodetype) source-label
                    target-id (in/type-name input-nodetype) target-label
                    (in/type-name input-nodetype) target-label))
    (assert-schema-type-compatible source-id source-label output-nodetype output-valtype target-id target-label input-nodetype input-valtype)))

(defn- ctx-perform-connect-arc [{:keys [basis] :as ctx} source-id source-label target-id target-label]
  (if-let [source (gt/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (gt/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
      (let [target-node-type (gt/node-type target)
            target-cascade-deletes (in/cascade-deletes target-node-type)]
        (assert-type-compatible source-id source source-label target-id target target-label)
        (-> ctx
            (mark-input-activated target-id target-label)
            (update :basis gt/connect source-id source-label target-id target-label)
            (cond->
              (not (:full-invalidation ctx))
              ;; When updating the successors, we must also consider any override
              ;; nodes of the source node, since these will inherit an implicit
              ;; connection between them and the corresponding override nodes of
              ;; the target node.
              (flag-successors-changed
                (e/cons
                  (pair source-id source-label)
                  (e/map #(pair % source-label)
                         (ig/get-overrides basis source-id))))

              (contains? target-cascade-deletes target-label)
              ;; If we're connecting to a :cascade-delete input, we will need to
              ;; re-traverse the :cascade-delete inputs of the connected sub-graph
              ;; and create override nodes for each node. This happens in the
              ;; realize-update-overrides function once the transaction concludes.
              (flag-override-nodes-affected target-id))))
      ctx)
    ctx))

(defn- override-node-ids-removed-by-disconnect [ctx source-id target-id target-label]
  (let [basis (:basis ctx)
        target (gt/node-by-id-at basis target-id)]
    (if-not (contains? (in/cascade-deletes (gt/node-type target)) target-label)
      []
      (let [source-override-nodes (map (partial gt/node-by-id-at basis) (ig/get-overrides basis source-id))]
        (loop [target-override-node-ids (ig/get-overrides basis target-id)
               result []]
          (if-let [target-override-id (first target-override-node-ids)]
            (let [basis (:basis ctx)
                  target-override-node (gt/node-by-id-at basis target-override-id)
                  target-override-id (gt/override-id target-override-node)
                  source-override-nodes-in-target-override (filter #(= target-override-id (gt/override-id %)) source-override-nodes)
                  traverse-fn (ig/override-traverse-fn basis target-override-id)
                  to-delete (ig/pre-traverse basis (mapv gt/node-id source-override-nodes-in-target-override) traverse-fn)]
              (recur (rest target-override-node-ids)
                     (into result to-delete)))
            result))))))

(defn- ctx-perform-disconnect-arc [ctx source-id source-label target-id target-label]
  (-> ctx
      (mark-input-activated target-id target-label)
      (update :basis gt/disconnect source-id source-label target-id target-label)
      (cond-> (not (:full-invalidation ctx))
        ;; When updating the successors, we must also consider any override nodes
        ;; of the source node, since these will inherit an implicit connection
        ;; between them and the corresponding override nodes of the target node.
        (flag-successors-changed
          (e/cons
            (pair source-id source-label)
            (e/map #(pair % source-label)
                   (ig/get-overrides (:basis ctx) source-id)))))))

(defn- ctx-disconnect [ctx source-id source-label target-id target-label]
  (let [ctx (ctx-perform-disconnect-arc ctx source-id source-label target-id target-label)]
    (reduce ctx-delete-node
            ctx
            (override-node-ids-removed-by-disconnect ctx source-id target-id target-label))))

(defn- ctx-invalidate [ctx node-id]
  (if (gt/node-by-id-at (:basis ctx) node-id)
    (mark-all-outputs-activated ctx node-id)
    ctx))

(defn- ctx-invalidate-output [ctx node-id output-label]
  (mark-output-activated ctx node-id output-label))

;; ---------------------------------------------------------------------------
;; Transaction steps
;; ---------------------------------------------------------------------------

(defonce/type AddNodeTXC [added-node]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-add-node ctx added-node))

  (revert [_this ctx]
    (ctx-revert-add-node ctx added-node)))

(defonce/type AddNodeTXS [added-node]
  TransactionStep
  (step-type [_this]
    :tx-step/add-node)

  (metrics-key [_this]
    (gt/node-id added-node))

  (realize [_this ctx undoable-changes]
    (let [[ctx undoable-changes] (perform-and-conj-change ctx undoable-changes (->AddNodeTXC added-node))]
      (realize-defaults ctx undoable-changes added-node))))

(defn add-node
  "*transaction step* - Add a node to its corresponding graph."
  [node]
  {:pre [(some? (gt/node-id node))]}
  [(->AddNodeTXS node)])

(defn- explicit-arcs-touching-node-ids
  [basis node-ids]
  (let [node-ids (set node-ids)]
    (into []
          (comp
            cat
            (distinct))
          [(mapcat (partial ig/explicit-arcs-by-source basis) node-ids)
           (mapcat (partial ig/explicit-arcs-by-target basis) node-ids)])))

(defn- deleted-node-override-state
  [basis nodes-by-id]
  (let [graphs (:graphs basis)
        node->overrides-keys (into (keys nodes-by-id)
                                   (keep (fn [[_node-id node]]
                                           (gt/original node)))
                                   nodes-by-id)]
    {:overrides
     (into {}
           (keep (fn [[_node-id node]]
                   (when-let [override-id (gt/override-id node)]
                     (when-let [override (ig/override-by-id basis override-id)]
                       [override-id override]))))
           nodes-by-id)

     :node->overrides
     (into {}
           (keep (fn [node-id]
                   (let [graph-id (gt/node-id->graph-id node-id)
                         node->overrides (get-in graphs [graph-id :node->overrides])]
                     (when (contains? node->overrides node-id)
                       [[graph-id node-id] (node->overrides node-id)]))))
           node->overrides-keys)}))

(defn- make-delete-nodes-change
  [ctx node-ids]
  (when (coll/not-empty node-ids)
    (let [basis (:basis ctx)
          to-delete (ig/pre-traverse basis node-ids ig/cascade-delete-sources)
          nodes-by-id (into {}
                            (keep (fn [node-id]
                                    (when-let [node (gt/node-by-id-at basis node-id)]
                                      [node-id node])))
                            to-delete)]
      (when (coll/not-empty nodes-by-id)
        (let [{:keys [overrides node->overrides]} (deleted-node-override-state basis nodes-by-id)
              deleted-arcs (explicit-arcs-touching-node-ids basis (keys nodes-by-id))]
          (->DeleteNodesTXC nodes-by-id
                            deleted-arcs
                            overrides
                            node->overrides))))))

(defn- ctx-perform-delete-nodes [ctx nodes-by-id]
  ;; ´TODO(decouple-undo-from-graph): We shouldn't call ctx-delete-node here. Instead, gather information beforehand.
  (reduce ctx-delete-node ctx (keys nodes-by-id)))

(defn- ctx-revert-delete-nodes [ctx nodes-by-id arcs overrides node->overrides]
  (let [ctx (reduce (fn [ctx node]
                      (-> ctx
                          (update :basis gt/add-node node)
                          (update :nodes-added conj (gt/node-id node))
                          (assoc-in [:successors-changed (gt/node-id node)] nil)))
                    ctx
                    (vals nodes-by-id))
        ctx (reduce-kv (fn [ctx override-id override]
                         (update ctx :basis gt/add-override override-id override))
                       ctx
                       overrides)
        ctx (reduce-kv (fn [ctx [graph-id node-id] override-node-ids]
                         (let [basis (:basis ctx)]
                           (-> ctx
                               (assoc-in [:basis :graphs graph-id :node->overrides node-id] override-node-ids)
                               (mark-override-originals-changed basis node-id))))
                       ctx
                       node->overrides)
        ctx (reduce (fn [ctx ^Arc arc]
                      (let [source-id (.source-id arc)
                            source-label (.source-label arc)
                            target-id (.target-id arc)
                            target-label (.target-label arc)]
                        (-> ctx
                            (mark-input-activated target-id target-label)
                            (update :basis gt/connect source-id source-label target-id target-label))))
                    ctx
                    arcs)
        ctx (if (:full-invalidation ctx)
              ctx
              (let [basis (:basis ctx)]
                (flag-successors-changed
                  ctx
                  (coll/into-> arcs :eduction
                    (map #(pair (gt/source-id %) (gt/source-label %)))
                    (distinct)
                    (mapcat (fn [[source-id source-label :as source]]
                              (e/cons
                                source
                                (e/map #(pair % source-label)
                                       (ig/get-overrides basis source-id)))))))))]
    (reduce mark-all-outputs-activated ctx (keys nodes-by-id))))

(defonce/type DeleteNodesTXC [nodes-by-id arcs overrides node->overrides]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-delete-nodes ctx nodes-by-id))

  (revert [_this ctx]
    (ctx-revert-delete-nodes ctx nodes-by-id arcs overrides node->overrides)))

(defonce/type DeleteNodeTXS [node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/delete-node)

  (metrics-key [_this]
    node-id)

  (realize [_this ctx undoable-changes]
    (if-let [change (make-delete-nodes-change ctx [node-id])]
      (perform-and-conj-change ctx undoable-changes change)
      (pair ctx undoable-changes))))

(defn delete-node
  "*transaction step* - Delete a node from its graph."
  [node-id]
  [(->DeleteNodeTXS node-id)])

(defonce/type OverrideTXS [root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/override)

  (metrics-key [_this]
    root-id)

  (realize [_this ctx undoable-changes]
    (realize-override ctx undoable-changes root-id traverse-fn init-props-fn init-fn properties-by-node-id)))

(defn override
  "*transaction step* - Create a series of override nodes in a graph."
  [root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  [(->OverrideTXS root-id traverse-fn init-props-fn init-fn properties-by-node-id)])

(defonce/type TransferOverridesTXS [from-id->to-id]
  TransactionStep
  (step-type [_this]
    :tx-step/transfer-overrides)

  (metrics-key [_this]
    ;; When metrics are enabled, this is called for one override node at a time.
    ;; This is potentially less efficient, but we get valuable context about
    ;; which specific nodes are costly to transfer overrides for.
    (when (= 1 (count from-id->to-id))
      (second (first from-id->to-id))))

  (realize [_this ctx undoable-changes]
    (realize-transfer-overrides ctx undoable-changes from-id->to-id)))

(defn transfer-overrides [from-id->to-id]
  [(->TransferOverridesTXS from-id->to-id)])

(defonce/type SetPropertyTXS [node-id property-label new-value]
  TransactionStep
  (step-type [_this]
    :tx-step/set-property)

  (metrics-key [_this]
    (pair node-id property-label))

  (realize [_this ctx undoable-changes]
    (realize-set-property ctx undoable-changes node-id property-label new-value)))

(defn set-property
  "*transaction step* - Sets a property value on a node."
  [node-id property-label new-value]
  {:pre [(gt/node-id? node-id)
         (keyword? property-label)]}
  [(->SetPropertyTXS node-id property-label new-value)])

(defonce/type UpdatePropertyTXS [node-id property-label fn args opts]
  TransactionStep
  (step-type [_this]
    :tx-step/update-property)

  (metrics-key [_this]
    (pair node-id property-label))

  (realize [_this ctx undoable-changes]
    (realize-update-property ctx undoable-changes node-id property-label fn args opts)))

(defn update-property
  "*transaction step* - Expects a node-id, a property-label, and an update-fn
  (with optional args) to be performed on the current value of the property."
  [node-id property-label update-fn args opts]
  {:pre [(gt/node-id? node-id)
         (keyword? property-label)
         (ifn? update-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->UpdatePropertyTXS node-id property-label update-fn args opts)])

(def inject-evaluation-context-opts
  {:inject-evaluation-context true})

(defonce/type ClearPropertyTXS [node-id property-label]
  TransactionStep
  (step-type [_this]
    :tx-step/clear-property)

  (metrics-key [_this]
    (pair node-id property-label))

  (realize [_this ctx undoable-changes]
    (realize-clear-property ctx undoable-changes node-id property-label)))

(defn clear-property
  "*transaction step* - Clears a property on a node."
  [node-id property-label]
  [(->ClearPropertyTXS node-id property-label)])

(defonce/type UpdateGraphValueTXC [graph-id k old-value old-value-assigned new-value]
  TransactionChange
  (perform [_this ctx]
    (assoc-in ctx [:basis :graphs graph-id :graph-values k] new-value))

  (revert [_this ctx]
    (if old-value-assigned
      (assoc-in ctx [:basis :graphs graph-id :graph-values k] old-value)
      (update-in ctx [:basis :graphs graph-id :graph-values] dissoc k))))

(defonce/type UpdateGraphValueTXS [graph-id k fn args]
  TransactionStep
  (step-type [_this]
    :tx-step/update-graph-value)

  (metrics-key [_this]
    graph-id)

  (realize [_this ctx undoable-changes]
    (let [graph-values (get-in ctx [:basis :graphs graph-id :graph-values])
          value-for-key (get graph-values k ::not-found)
          old-value (case value-for-key ::not-found nil value-for-key)
          old-value-assigned (case value-for-key ::not-found false true)
          new-value (apply fn old-value args)]
      (perform-and-conj-change ctx undoable-changes (->UpdateGraphValueTXC graph-id k old-value old-value-assigned new-value)))))

(defn update-graph-value
  "*transaction step* - Update a graph value."
  [graph-id k fn args]
  {:pre [(gt/graph-id? graph-id)
         (ifn? fn)
         (coll/eager-seqable? args)]}
  [(->UpdateGraphValueTXS graph-id k fn args)])

(defonce/type CallbackTXS [callback-fn args opts]
  TransactionStep
  (step-type [_this]
    :tx-step/callback)

  (metrics-key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (pair (ctx-callback ctx callback-fn args opts) undoable-changes)))

(defn callback
  "*transaction step* - Call a function from within the transaction."
  [callback-fn args opts]
  {:pre [(ifn? callback-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->CallbackTXS callback-fn args opts)])

(defonce/type ConnectArcTXC [source-id source-label target-id target-label]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-connect-arc ctx source-id source-label target-id target-label))

  (revert [_this ctx]
    (ctx-perform-disconnect-arc ctx source-id source-label target-id target-label)))

(defonce/type DisconnectArcTXC [source-id source-label target-id target-label]
  TransactionChange
  (perform [_this ctx]
    (ctx-perform-disconnect-arc ctx source-id source-label target-id target-label))

  (revert [_this ctx]
    (ctx-perform-connect-arc ctx source-id source-label target-id target-label)))

(defn- realize-disconnect
  [ctx undoable-changes source-id source-label target-id target-label]
  (let [disconnect-change (->DisconnectArcTXC source-id source-label target-id target-label)
        ctx (perform-change ctx disconnect-change)

        removed-override-node-ids
        (override-node-ids-removed-by-disconnect
          ctx
          source-id
          target-id
          target-label)]

    ;; TODO(decouple-undo-from-graph): Order here seems wrong?
    (if-let [delete-change (make-delete-nodes-change ctx removed-override-node-ids)]
      (let [ctx (perform-change ctx delete-change)
            undoable-changes (conj-change (conj-change undoable-changes disconnect-change) delete-change)]
        (pair ctx undoable-changes))
      (pair ctx (conj-change undoable-changes disconnect-change)))))

(defn- realize-connect
  [{:keys [basis] :as ctx} undoable-changes source-id source-label target-id target-label]
  (if-let [source (gt/node-by-id-at basis source-id)]
    (if-let [target (gt/node-by-id-at basis target-id)]
      (let [target-node-type (gt/node-type target)

            [ctx undoable-changes]
            (if (not= :one (in/input-cardinality target-node-type target-label))
              (pair ctx undoable-changes)
              (reduce
                (fn [[ctx undoable-changes] ^Arc arc]
                  (realize-disconnect ctx undoable-changes (.source-id arc) (.source-label arc) (.target-id arc) (.target-label arc)))
                (pair ctx undoable-changes)
                (ig/explicit-arcs-by-target basis target-id target-label)))

            connect-change (->ConnectArcTXC source-id source-label target-id target-label)]
        (assert-type-compatible source-id source source-label target-id target target-label)
        (pair (perform-change ctx connect-change)
              (conj-change undoable-changes connect-change)))
      (pair ctx undoable-changes))
    (pair ctx undoable-changes)))

(defonce/type ConnectTXS [source-id source-label target-id target-label]
  TransactionStep
  (step-type [_this]
    :tx-step/connect)

  (metrics-key [_this]
    [source-id source-label target-id target-label])

  (realize [_this ctx undoable-changes]
    (realize-connect ctx undoable-changes source-id source-label target-id target-label)))

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and
  label and a target node and label."
  [source-id source-label target-id target-label]
  [(->ConnectTXS source-id source-label target-id target-label)])

(defonce/type ExpandTXS [tx-steps-fn args opts]
  TransactionStep
  (step-type [_this]
    :tx-step/expand)

  (metrics-key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (let [tx-steps (if (:inject-evaluation-context opts)
                     (let [basis (:basis ctx)
                           tx-data-context (:tx-data-context ctx)
                           evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
                       (apply tx-steps-fn evaluation-context args))
                     (apply tx-steps-fn args))]
      (realize-tx ctx tx-steps undoable-changes))))

(defn expand
  "*transaction step* - Call a function and execute the returned transaction
  steps within the transaction."
  [tx-steps-fn args opts]
  {:pre [(ifn? tx-steps-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->ExpandTXS tx-steps-fn args opts)])

(defonce/type DisconnectTXS [source-id source-label target-id target-label]
  TransactionStep
  (step-type [_this]
    :tx-step/disconnect)

  (metrics-key [_this]
    [source-id source-label target-id target-label])

  (realize [_this ctx undoable-changes]
    (realize-disconnect ctx undoable-changes source-id source-label target-id target-label)))

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a transaction step
  disconnecting a source node and label from a target node and label."
  [source-id source-label target-id target-label]
  [(->DisconnectTXS source-id source-label target-id target-label)])

(defn disconnect-sources
  [basis target-id target-label]
  (for [arc (ig/explicit-inputs basis target-id target-label)]
    (disconnect (gt/source-id arc) (gt/source-label arc) target-id target-label)))

(defonce/type LabelTXS [label]
  TransactionStep
  (step-type [_this]
    :tx-step/label)

  (metrics-key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (pair (assoc ctx :label label)
          undoable-changes)))

(defn label
  [label]
  [(->LabelTXS label)])

(defonce/type SequenceLabelTXS [sequence-label]
  TransactionStep
  (step-type [_this]
    :tx-step/sequence-label)

  (metrics-key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (pair (assoc ctx :sequence-label sequence-label)
          undoable-changes)))

(defn sequence-label
  [sequence-label]
  [(->SequenceLabelTXS sequence-label)])

(defonce/type InvalidateTXS [node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate)

  (metrics-key [_this]
    node-id)

  (realize [_this ctx undoable-changes]
    (pair (ctx-invalidate ctx node-id)
          undoable-changes)))

(defn invalidate
  [node-id]
  [(->InvalidateTXS node-id)])

(defonce/type InvalidateOutputTXS [node-id output-label]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate-output)

  (metrics-key [_this]
    (pair node-id output-label))

  (realize [_this ctx undoable-changes]
    (pair (ctx-invalidate-output ctx node-id output-label)
          undoable-changes)))

(defn invalidate-output
  [node-id output-label]
  [(->InvalidateOutputTXS node-id output-label)])

;; ---------------------------------------------------------------------------
;; Transaction step inspection
;; ---------------------------------------------------------------------------

(defn tx-step?
  [value]
  (instance? TransactionStep value))

(def tx-step-type TransactionStep/.step_type)

(defn tx-step-added-node
  [^AddNodeTXS tx-step]
  (when (instance? AddNodeTXS tx-step)
    (.-added-node tx-step)))

(defn tx-step-added-arc
  ^Arc [^ConnectTXS tx-step]
  (when (instance? ConnectTXS tx-step)
    (gt/->Arc (.-source-id tx-step)
              (.-source-label tx-step)
              (.-target-id tx-step)
              (.-target-label tx-step))))

(defn realize-tx
  [ctx tx-data undoable-changes]
  (let [is-non-undoable (non-undoable? tx-data)
        tx-data (cond-> tx-data is-non-undoable non-undoable-tx-data)
        mutated-undoable-changes (if is-non-undoable nil undoable-changes)

        [ctx mutated-undoable-changes]
        (coll/reduce->
          tx-data
          (pair ctx mutated-undoable-changes)
          (fn [[ctx mutated-undoable-changes :as acc] ^TransactionStep tx-step]
            (cond
              (nil? tx-step)
              acc

              (non-undoable? tx-step)
              (let [[ctx] (realize-tx ctx tx-step nil)]
                (pair ctx mutated-undoable-changes))

              (sequential? tx-step)
              (realize-tx ctx tx-step mutated-undoable-changes)

              :else
              (let [[ctx mutated-undoable-changes]
                    (try
                      (du/measuring (:metrics ctx) (.step-type tx-step) (.metrics-key tx-step)
                        (.realize tx-step ctx mutated-undoable-changes))
                      (catch Exception e
                        (when *tx-debug*
                          (println (txerrstr ctx "Transaction failed on " tx-step)))
                        (throw e)))]
                (pair (update ctx :completed-action-count inc)
                      mutated-undoable-changes)))))]
    (pair ctx (or mutated-undoable-changes undoable-changes))))

(def tx-report-keys
  (cond-> [:basis :nodes-added :nodes-deleted :outputs-modified :label :sequence-label :undoable-changes]
          (du/metrics-enabled?) (conj :metrics)))

(defn finalize-update
  [{:keys [tx-data-context] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (zero? (long (:completed-action-count ctx))) :empty :ok)
             :tx-data-context-map (deref tx-data-context))))

(defn new-transaction-context
  [basis node-id-generators override-id-generator tx-data-context-map metrics-collector full-invalidation]
  {:pre [(map? tx-data-context-map)]}
  {:basis basis
   :nodes-affected #{}
   :nodes-added []
   :nodes-deleted {}
   :outputs-modified #{}
   :override-nodes-affected-seen #{}
   :override-nodes-affected-ordered []
   :successors-changed {}
   :node-id-generators node-id-generators
   :override-id-generator override-id-generator
   :completed-action-count 0
   :txid (new-txid)
   :tx-data-context (atom tx-data-context-map)
   :metrics metrics-collector
   :full-invalidation full-invalidation})

(defn update-successors
  [{:keys [^long completed-action-count successors-changed] :as ctx}]
  (du/measuring (:metrics ctx) :update-successors
    (cond
      (zero? completed-action-count)
      ctx

      (:full-invalidation ctx)
      (update ctx :basis ig/invalidate-all-successors)

      :else
      (update ctx :basis ig/update-successors successors-changed))))

(defn trace-dependencies
  [ctx]
  ;; At this point, :nodes-affected is a set of all output Endpoints that have
  ;; been directly affected by the transaction changes.
  ;; We now follow these outputs recursively to obtain a sequence of all the
  ;; outputs that depend on them in the entire graph.
  (du/measuring (:metrics ctx) :trace-dependencies
    (let [outputs-modified (gt/dependencies (:basis ctx) (:nodes-affected ctx))]
      (assoc ctx :outputs-modified outputs-modified))))

(defn finalize-applied-changes
  [ctx]
  (-> ctx
      update-successors
      trace-dependencies
      finalize-update))

(defn transact*
  [ctx tx-data undoable-changes]
  (when *tx-debug*
    (println (txerrstr ctx "tx-data" (if (non-undoable? tx-data)
                                       (non-undoable-tx-data tx-data)
                                       (seq tx-data)))))
  (let [[ctx undoable-changes] (realize-tx ctx tx-data undoable-changes)
        [ctx undoable-changes] (realize-update-overrides ctx undoable-changes)
        undoable-changes (if undoable-changes (persistent! undoable-changes) [])]
    (-> ctx
        (assoc :undoable-changes undoable-changes)
        finalize-applied-changes)))

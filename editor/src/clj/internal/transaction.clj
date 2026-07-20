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
  (:require [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [clojure.lang ArityException]
           [internal.graph.types Arc]
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

(declare ^:private realize-add-nodes
         ^:private realize-delete-nodes)

;; ---------------------------------------------------------------------------
;; Internal state
;; ---------------------------------------------------------------------------

(defonce ^:private not-overridden-sentinel (Object.))

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

(defn realize-tx
  [ctx undoable-changes tx-data]
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
              (let [[ctx] (realize-tx ctx nil tx-step)]
                (pair ctx mutated-undoable-changes))

              (sequential? tx-step)
              (realize-tx ctx mutated-undoable-changes tx-step)

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

(defn- mark-arc-targets-activated
  [ctx arcs]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (let [basis (:basis ctx)
          nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (into nodes-affected
              (keep
                (fn [arc]
                  (when (gt/node-by-id-at basis (gt/target-id arc))
                    (gt/target-endpoint arc))))
              arcs)))))

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

(defn- mark-nodes-outputs-activated
  [ctx nodes]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if (:full-invalidation ctx)
    ctx
    (assoc ctx
      :nodes-affected
      (coll/reduce=> (:nodes-affected ctx) nodes
        (fn [nodes-affected node]
          (let [node-id (gt/node-id node)]
            (into nodes-affected
                  (map #(gt/endpoint node-id %))
                  (-> node gt/node-type in/output-labels))))))))

(defn- next-node-id [ctx graph-id]
  (gt/next-node-id (:node-id-generators ctx) graph-id))

(defn- next-override-id [ctx graph-id]
  (gt/next-override-id (:override-id-generator ctx) graph-id))

(defn- ctx-perform-add-override
  [ctx override-id override]
  (update ctx :basis ig/basis-perform-add-override override-id override))

(defn- ctx-revert-add-override
  [ctx override-id]
  (update ctx :basis ig/basis-revert-add-override override-id))

(defn- flag-successors-changed
  "Merges successor changes into the context. A node id means every label on
  the node changed; a [node-id label] pair means only that label changed."
  [ctx changes]
  (if (:full-invalidation ctx)
    ctx
    (let [successors-changed (:successors-changed ctx)

          affected-changes
          (filterv
            (fn [change]
              (if (gt/node-id? change)
                (get successors-changed change ::not-found)
                (let [[node-id label] change
                      old-affected-node-labels (get successors-changed node-id ::not-found)]
                  (case old-affected-node-labels
                    nil false ; All node labels are already flagged as changed.
                    ::not-found true ; Nothing is flagged for this node yet.
                    (not (contains? old-affected-node-labels label))))))
            changes)]

      (if (coll/empty? affected-changes)
        ctx
        (assoc ctx
          :successors-changed
          (-> successors-changed
              (transient)
              (coll/reduce=> affected-changes
                (fn [successors-changed change]
                  (if (gt/node-id? change)
                    (assoc! successors-changed change nil)
                    (let [[node-id label] change
                          old-affected-node-labels (get successors-changed node-id ::not-found)]
                      (case old-affected-node-labels
                        nil successors-changed ; All node labels are already flagged as changed.
                        ::not-found (assoc! successors-changed node-id #{label})
                        (assoc! successors-changed node-id (conj old-affected-node-labels label)))))))
              (persistent!)))))))

(defn- mark-successor-nodes-changed [ctx node-ids]
  (if (:full-invalidation ctx)
    ctx
    (update ctx :successor-node-ids-changed into node-ids)))

(defn- mark-successor-arcs-changed [ctx arcs]
  (if (:full-invalidation ctx)
    ctx
    (update ctx :successor-arcs-changed into arcs)))

(defn- node-successor-changes
  "Returns a deferred stream of successor changes caused by the specified
  nodes differing between the old-basis and the new-basis."
  [old-basis new-basis node-ids]
  (coll/into-> (pair old-basis new-basis) :eduction
    (mapcat
      (fn [basis]
        (e/mapcat
          (fn [node-id]
            (when (gt/node-by-id-at basis node-id)
              (e/concat
                ;; The node and its originals must account for changes to their
                ;; immediate override nodes.
                (ig/override-originals basis node-id)

                ;; Sources targeting the node or any of its overrides must
                ;; account for changes to their effective outgoing arcs.
                (e/mapcat #(gt/sources basis %)
                          (ig/pre-traverse basis [node-id] ig/get-overrides)))))
          node-ids)))
    (distinct)))

(defn- arc-successor-changes
  "Returns a deferred stream of successor changes caused by the specified
  arcs differing between the old-basis and the new-basis."
  [old-basis new-basis arcs]
  (coll/into-> (pair old-basis new-basis) :eduction
    (mapcat
      (fn [basis]
        (e/mapcat
          (fn [arc]
            (let [source (pair (gt/source-id arc) (gt/source-label arc))
                  target-id (gt/target-id arc)
                  target-label (gt/target-label arc)]
              (if-not (gt/node-by-id-at basis target-id)
                [source]
                (e/cons
                  source
                  (e/mapcat #(gt/sources basis % target-label)
                            (ig/pre-traverse basis [target-id] ig/get-overrides))))))
          arcs)))
    (distinct)))

(defn- ctx-add-nodes [ctx nodes introduced-node->overrides]
  (let [node-ids (mapv gt/node-id nodes)
        ctx (-> ctx
                (update :basis ig/basis-perform-add-nodes nodes introduced-node->overrides)
                (update :nodes-added into node-ids))]
    (if (:full-invalidation ctx)
      ctx
      (let [changed-node-ids (e/concat node-ids (e/map key introduced-node->overrides))
            new-basis (:basis ctx)
            source-arcs (coll/into-> node-ids []
                          (mapcat #(ig/explicit-arcs-by-source new-basis %)))]
        (-> ctx
            (mark-nodes-outputs-activated nodes)
            (mark-successor-nodes-changed changed-node-ids)
            (mark-successor-arcs-changed source-arcs))))))

(defn- ctx-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node->overrides]
  (let [node-ids (mapv gt/node-id nodes)
        nodes-by-id (coll/pair-map-by gt/node-id nodes)
        arcs (coll/into-> removed-arc->source+target-pkids []
               (map key))
        source-arcs (coll/into-> removed-arc->source+target-pkids []
                      (keep
                        (fn [[arc [_source-arc-pkids target-arc-pkids]]]
                          (when (and (coll/not-empty target-arc-pkids)
                                     (contains? nodes-by-id (gt/source-id arc)))
                            arc))))
        changed-node-ids (e/concat node-ids (e/map key node->overrides))]
    (-> ctx
        (mark-nodes-outputs-activated nodes)
        (mark-arc-targets-activated source-arcs)
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed arcs)
        (update :nodes-deleted into nodes-by-id)
        (update :nodes-added coll/transform-> (remove nodes-by-id))
        (update :basis ig/basis-perform-delete-nodes nodes removed-arc->source+target-pkids overrides node->overrides))))

(defonce/type AddOverrideTXC
  [override-id override]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-add-override ctx override-id override))

  (revert [_this ctx]
    (ctx-revert-add-override ctx override-id)))

(defn- realize-add-override [ctx undoable-changes override-id root-node-id traverse-fn init-props-fn]
  (if-let [{:keys [override-id
                   override]}
           (ig/basis-plan-add-override (:basis ctx) override-id root-node-id traverse-fn init-props-fn)]
    (perform-and-conj-change ctx undoable-changes (->AddOverrideTXC override-id override))
    (pair ctx undoable-changes)))

(defonce/type AddOverrideTXS [override-id root-id traverse-fn init-props-fn]
  TransactionStep
  (step-type [_this]
    :tx-step/add-override)

  (metrics-key [_this]
    root-id)

  (realize [_this ctx undoable-changes]
    (realize-add-override ctx undoable-changes override-id root-id traverse-fn init-props-fn)))

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
        [ctx undoable-changes] (realize-add-nodes ctx undoable-changes override-nodes)
        [ctx undoable-changes] (realize-add-override ctx undoable-changes override-id root-id traverse-fn init-props-fn)]

    (realize-tx
      ctx undoable-changes
      (init-fn
        (in/custom-evaluation-context {:basis (:basis ctx) :tx-data-context (:tx-data-context ctx)})
        original-node-id->override-node-id))))

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
                                    properties)]
            (realize-add-nodes ctx undoable-changes [new-override-node])))))))

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
  [{:keys [override-nodes-affected-ordered override-nodes-affected-undoability] :as ctx} undoable-changes]
  (du/measuring (:metrics ctx) :update-overrides
    (coll/reduce-> override-nodes-affected-ordered (pair ctx undoable-changes)
      (fn [[ctx undoable-changes] node-id]
        (let [undoable (override-nodes-affected-undoability node-id ::not-found)]
          (assert (not= ::not-found undoable))
          (if undoable
            (realize-populate-overrides ctx undoable-changes node-id)
            (let [[ctx] (realize-populate-overrides ctx nil node-id)]
              (pair ctx undoable-changes))))))))

(defn- ctx-perform-clear-override-nodes [ctx original-node-id override-node-ids]
  (-> ctx
      (update :basis ig/basis-perform-clear-override-nodes original-node-id override-node-ids)
      (mark-successor-nodes-changed [original-node-id])))

(defn- ctx-revert-clear-override-nodes [ctx original-node-id override-node-ids]
  (-> ctx
      (update :basis ig/basis-revert-clear-override-nodes original-node-id override-node-ids)
      (mark-successor-nodes-changed [original-node-id])))

(defonce/type ClearOverrideNodesTXC
  [original-node-id override-node-ids]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-clear-override-nodes ctx original-node-id override-node-ids))

  (revert [_this ctx]
    (ctx-revert-clear-override-nodes ctx original-node-id override-node-ids)))

(defonce/type ReplaceOverrideRootTXC
  [override-id old-override new-override]

  TransactionChange
  (perform [_this ctx]
    (update ctx :basis ig/basis-perform-replace-override override-id new-override))

  (revert [_this ctx]
    (update ctx :basis ig/basis-revert-replace-override override-id old-override)))

(defn- ctx-perform-repoint-override-node
  [ctx override-node-id new-original-node-id]
  (let [old-basis (:basis ctx)
        ctx (update ctx :basis ig/basis-perform-repoint-override-node override-node-id new-original-node-id)]
    (if (identical? old-basis (:basis ctx))
      ctx
      (-> ctx
          (mark-all-outputs-activated override-node-id)
          (mark-successor-nodes-changed [override-node-id new-original-node-id])))))

(defn- ctx-revert-repoint-override-node
  [ctx override-node-id old-original-node-id new-original-node-id]
  (let [old-basis (:basis ctx)
        ctx (update ctx :basis ig/basis-revert-repoint-override-node override-node-id old-original-node-id new-original-node-id)]
    (if (identical? old-basis (:basis ctx))
      ctx
      (-> ctx
          (mark-all-outputs-activated override-node-id)
          (mark-successor-nodes-changed [override-node-id new-original-node-id])))))

(defonce/type RepointOverrideNodeTXC
  [override-node-id old-original-node-id new-original-node-id]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-repoint-override-node ctx override-node-id new-original-node-id))

  (revert [_this ctx]
    (ctx-revert-repoint-override-node ctx override-node-id old-original-node-id new-original-node-id)))

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
          (fn [[ctx undoable-changes :as ctx+undoable-changes] from-id]
            (let [override-node-ids (ig/get-overrides basis from-id)]
              (if-let [{:keys [original-node-id
                               override-node-ids]}
                       (ig/basis-plan-clear-override-nodes (:basis ctx) from-id override-node-ids)]
                (perform-and-conj-change ctx undoable-changes (->ClearOverrideNodesTXC original-node-id override-node-ids))
                ctx+undoable-changes))))

        ;; Re-root overrides that used to have a from node id as root.
        [ctx undoable-changes]
        (coll/reduce-> overrides-to-fix (pair ctx undoable-changes)
          (fn [[ctx undoable-changes :as ctx+undoable-changes] [override-id override]]
            (let [new-override (update override :root-id from-id->to-id)]
              (if-let [{:keys [override-id
                               old-override
                               new-override]}
                       (ig/basis-plan-replace-override (:basis ctx) override-id new-override)]
                (perform-and-conj-change ctx undoable-changes (->ReplaceOverrideRootTXC override-id old-override new-override))
                ctx+undoable-changes))))

        ;; Delete old nodes.
        [ctx undoable-changes] (realize-delete-nodes ctx undoable-changes nodes-to-delete)

        ;; * repoint the first level override nodes to use to-node as original
        ;; * add as override nodes of to-node
        [ctx undoable-changes]
        (coll/reduce-> override-node-ids (pair ctx undoable-changes)
          (fn [[ctx undoable-changes :as ctx+undoable-changes] override-node-id]
            (let [basis (:basis ctx)
                  override-node (gt/node-by-id-at basis override-node-id)
                  current-original-id (gt/original override-node)
                  new-original-id (from-id->to-id current-original-id)]
              (if-let [{:keys [override-node-id
                               old-original-node-id
                               new-original-node-id]}
                       (ig/basis-plan-repoint-override-node basis override-node-id new-original-id)]
                (perform-and-conj-change ctx undoable-changes (->RepointOverrideNodeTXC override-node-id old-original-node-id new-original-node-id))
                ctx+undoable-changes))))]

    ;; Populate the fresh override layers.
    (coll/reduce-> (vals from-id->to-id) (pair ctx undoable-changes)
      (fn [[ctx undoable-changes] to-id]
        (realize-populate-overrides ctx undoable-changes to-id)))))

(defn- mark-property-state-changes
  [ctx node-id property-label]
  ;; This is called after making changes to properties from TransactionChanges.
  ;; We don't bother to check if the property value changed or if its overridden
  ;; state changed, because evaluating the value-fns can potentially be
  ;; expensive. Besides, we're already skipping creating TransactionChanges for
  ;; unchanged properties when we realize the TransactionSteps.
  (if (:full-invalidation ctx)
    ctx
    (let [basis (:basis ctx)
          node (gt/node-by-id-at basis node-id)]
      (if-not node
        ctx
        (let [node-type (gt/node-type node)
              is-override-node (gt/original node)
              is-declared-property (contains? (in/all-properties node-type) property-label)

              invalidated-output-labels
              (cond-> (if is-declared-property
                        [property-label]
                        [property-label :_properties]) ; :_properties is not automatically invalidated for us, so we must do it ourselves.

                is-override-node
                (conj :_overridden-properties))] ; This is a map of property-label->value.

          (case (count invalidated-output-labels)
            0 ctx
            1 (mark-output-activated ctx node-id (invalidated-output-labels 0))
            (mark-outputs-activated ctx node-id invalidated-output-labels)))))))

(defn- ctx-perform-set-raw-property
  [ctx node-id property-label new-raw-value]
  (-> ctx
      (update :basis ig/basis-perform-set-raw-property node-id property-label new-raw-value)
      (mark-property-state-changes node-id property-label)))

(defn- ctx-revert-set-raw-property
  [ctx node-id property-label old-raw-value]
  (-> ctx
      (update :basis ig/basis-revert-set-raw-property node-id property-label old-raw-value)
      (mark-property-state-changes node-id property-label)))

(defn- ctx-perform-clear-raw-property
  [ctx node-id property-label]
  (-> ctx
      (update :basis ig/basis-perform-clear-raw-property node-id property-label)
      (mark-property-state-changes node-id property-label)))

(defn- ctx-revert-clear-raw-property
  [ctx node-id property-label old-raw-value]
  (-> ctx
      (update :basis ig/basis-revert-clear-raw-property node-id property-label old-raw-value)
      (mark-property-state-changes node-id property-label)))

(defn- call-setter-fn [ctx property setter-fn basis node-id old-value new-value]
  (try
    (let [tx-data-context (:tx-data-context ctx)
          setter-actions (setter-fn (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context}) node-id old-value new-value)]
      (when *tx-debug*
        (println (txerrstr ctx "setter actions" (seq setter-actions))))
      setter-actions)
    (catch ArityException ae
      (when *tx-debug*
        (println "ArityException while inside " setter-fn " on node " node-id " with " old-value new-value (:node-type (gt/node-by-id-at basis node-id))))
      (throw ae))
    (catch Exception e
      (let [node-type (:name @(:node-type (gt/node-by-id-at basis node-id)))]
        (throw (Exception. (format "Setter of node %s (%s) %s could not be called" node-id node-type property) e))))))

(defonce/type SetRawPropertyTXC
  [node-id property-label old-raw-value new-raw-value]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-set-raw-property ctx node-id property-label new-raw-value))

  (revert [_this ctx]
    (ctx-revert-set-raw-property ctx node-id property-label old-raw-value)))

(defonce/type ClearRawPropertyTXC
  [node-id property-label old-raw-value]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-clear-raw-property ctx node-id property-label))

  (revert [_this ctx]
    (ctx-revert-clear-raw-property ctx node-id property-label old-raw-value)))

(defn- realize-set-property-impl
  [ctx undoable-changes node property-label old-value new-value]
  (let [node-id (gt/node-id node)
        node-type (gt/node-type node)
        assigned-properties (gt/assigned-properties node)

        ctx+undoable-changes
        (if-let [{:keys [node-id
                         property-label
                         old-raw-value
                         new-raw-value]}
                 (ig/basis-plan-set-raw-property (:basis ctx) node-id property-label new-value)]
          (perform-and-conj-change ctx undoable-changes (->SetRawPropertyTXC node-id property-label old-raw-value new-raw-value))
          (pair ctx undoable-changes))

        realize-setter-actions
        (or (not (contains? assigned-properties property-label))
            (not= old-value new-value))]

    (if realize-setter-actions
      (if-let [setter-fn (in/property-setter node-type property-label)]
        (let [[ctx undoable-changes] ctx+undoable-changes
              setter-actions (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id old-value new-value)]
          (realize-tx ctx undoable-changes setter-actions))
        ctx+undoable-changes)
      ctx+undoable-changes)))

(defn- realize-set-property
  [ctx undoable-changes node-id property-label new-value]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if-not node
      (pair ctx undoable-changes)
      (let [evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context (:tx-data-context ctx)})
            old-value (in/node-property-value node property-label evaluation-context)]
        (realize-set-property-impl ctx undoable-changes node property-label old-value new-value)))))

(defn- realize-update-property
  [ctx undoable-changes node-id property-label update-fn args opts]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if-not node
      (pair ctx undoable-changes)
      (let [evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context (:tx-data-context ctx)})
            old-value (in/node-property-value node property-label evaluation-context)
            new-value (if (:inject-evaluation-context opts)
                        (apply update-fn evaluation-context old-value args)
                        (apply update-fn old-value args))]
        (realize-set-property-impl ctx undoable-changes node property-label old-value new-value)))))

(defn- realize-clear-property
  [ctx undoable-changes node-id property-label]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if-not node
      (pair ctx undoable-changes)
      (if-let [{:keys [node-id
                       property-label
                       old-raw-value]}
               (ig/basis-plan-clear-raw-property basis node-id property-label)]
        (let [change (->ClearRawPropertyTXC node-id property-label old-raw-value)
              node-type (gt/node-type node)
              setter-fn (in/property-setter node-type property-label)]
          (if-not setter-fn
            (perform-and-conj-change ctx undoable-changes change)
            (let [evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context (:tx-data-context ctx)})
                  old-value (in/node-property-value node property-label evaluation-context)
                  [ctx undoable-changes] (perform-and-conj-change ctx undoable-changes change)]
              (let [setter-actions (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id old-value nil)]
                (realize-tx ctx undoable-changes setter-actions)))))
        (pair ctx undoable-changes)))))

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
          (fn [ctx+undoable-changes [property-label default-value]]
            (if-some [property-value (value-fn property-label default-value)]
              (let [[ctx undoable-changes] ctx+undoable-changes
                    setter-fn (in/property-setter node-type property-label)
                    setter-actions (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id nil property-value)]
                (realize-tx ctx undoable-changes setter-actions))
              ctx+undoable-changes)))))))

(defn- ctx-perform-add-nodes [ctx added-nodes introduced-node->overrides]
  (ctx-add-nodes ctx added-nodes introduced-node->overrides))

(defn- ctx-revert-add-nodes [ctx added-nodes introduced-node->overrides]
  (let [old-basis (:basis ctx)
        node-ids (mapv gt/node-id added-nodes)
        nodes-by-id (coll/pair-map-by gt/node-id added-nodes)
        source-arcs (coll/into-> node-ids []
                      (mapcat #(ig/explicit-arcs-by-source old-basis %)))
        changed-node-ids (e/concat node-ids (e/map key introduced-node->overrides))]
    (-> ctx
        (mark-nodes-outputs-activated added-nodes)
        (mark-arc-targets-activated source-arcs)
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed source-arcs)
        (update :nodes-deleted into nodes-by-id)
        (update :nodes-added coll/transform-> (remove nodes-by-id))
        (update :basis ig/basis-revert-add-nodes added-nodes introduced-node->overrides))))

(defn- ctx-callback
  [ctx fn args opts]
  (if (:inject-evaluation-context opts)
    (let [basis (:basis ctx)
          tx-data-context (:tx-data-context ctx)
          evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
      (apply fn evaluation-context args))
    (apply fn args))
  ctx)

(defn- mark-override-nodes-affected [ctx target-id undoable]
  (let [override-nodes-affected-undoability (:override-nodes-affected-undoability ctx)
        previous-mark-undoability (get override-nodes-affected-undoability target-id ::not-found)]
    (if (= ::not-found previous-mark-undoability)
      (let [override-nodes-affected-ordered (:override-nodes-affected-ordered ctx)]
        (assoc ctx
          :override-nodes-affected-undoability (assoc override-nodes-affected-undoability target-id undoable)
          :override-nodes-affected-ordered (conj override-nodes-affected-ordered target-id)))
      (do
        (assert (= previous-mark-undoability undoable)
                "Cannot mix undoable and non-undoable changes to an override node in the same transaction.")
        ctx))))

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

(defn- ctx-connect-arcs
  [ctx arc->source+target-pkids undoable]
  (let [[ctx changed-arcs]
        (coll/reduce-kv-> arc->source+target-pkids (pair ctx [])
          (fn [[ctx changed-arcs :as result] arc source+target-pkids]
            (let [[_source-arc-pkids target-arc-pkids] source+target-pkids
                  old-basis (:basis ctx)
                  new-basis (ig/basis-perform-connect-arc-pkids old-basis arc source+target-pkids)]
              (if (identical? old-basis new-basis)
                result
                (let [target-id (gt/target-id arc)
                      target-label (gt/target-label arc)
                      target-node (when (coll/not-empty target-arc-pkids)
                                    (gt/node-by-id-at new-basis target-id))
                      changed-arcs (conj changed-arcs arc)
                      ctx (assoc ctx :basis new-basis)]
                  (if-not target-node
                    (pair ctx changed-arcs)
                    (let [target-node-type (gt/node-type target-node)
                          target-cascade-deletes (in/cascade-deletes target-node-type)
                          ctx (-> ctx
                                  (mark-input-activated target-id target-label)
                                  (cond->
                                    (contains? target-cascade-deletes target-label)
                                    ;; If we're connecting to a :cascade-delete
                                    ;; input, we will need to re-traverse the
                                    ;; :cascade-delete inputs of the connected
                                    ;; sub-graph and create override nodes for each
                                    ;; node. This happens once the transaction
                                    ;; concludes in realize-update-overrides.
                                    (mark-override-nodes-affected target-id undoable)))]
                      (pair ctx changed-arcs))))))))]
    (mark-successor-arcs-changed ctx changed-arcs)))

(defn- ctx-disconnect-arcs
  [ctx arc->source+target-pkids]
  (let [[ctx changed-arcs]
        (coll/reduce-kv-> arc->source+target-pkids (pair ctx [])
          (fn [[ctx changed-arcs :as result] arc source+target-pkids]
            (let [[_source-arc-pkids target-arc-pkids] source+target-pkids
                  old-basis (:basis ctx)
                  new-basis (ig/basis-perform-disconnect-arc-pkids old-basis arc source+target-pkids)]
              (if (identical? old-basis new-basis)
                result
                (pair
                  (cond-> (assoc ctx :basis new-basis)
                    (and (coll/not-empty target-arc-pkids)
                         (gt/node-by-id-at new-basis (gt/target-id arc)))
                    (mark-input-activated (gt/target-id arc) (gt/target-label arc)))
                  (conj changed-arcs arc))))))]
    (mark-successor-arcs-changed ctx changed-arcs)))

(defn- ctx-perform-connect-arcs
  [ctx arc->source+target-pkids undoable]
  (ctx-connect-arcs ctx arc->source+target-pkids undoable))

(defn- ctx-revert-connect-arcs
  [ctx arc->source+target-pkids]
  (ctx-disconnect-arcs ctx arc->source+target-pkids))

(defn- ctx-perform-disconnect-arcs
  [ctx arc->source+target-pkids]
  (ctx-disconnect-arcs ctx arc->source+target-pkids))

(defn- ctx-revert-disconnect-arcs
  [ctx arc->source+target-pkids undoable]
  (ctx-connect-arcs ctx arc->source+target-pkids undoable))

(defn- ctx-invalidate [ctx node-id]
  (if (gt/node-by-id-at (:basis ctx) node-id)
    (mark-all-outputs-activated ctx node-id)
    ctx))

(defn- ctx-invalidate-output [ctx node-id output-label]
  (mark-output-activated ctx node-id output-label))

;; ---------------------------------------------------------------------------
;; Transaction steps
;; ---------------------------------------------------------------------------

(defonce/type AddNodesTXC
  [added-nodes introduced-node->overrides]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-add-nodes ctx added-nodes introduced-node->overrides))

  (revert [_this ctx]
    (ctx-revert-add-nodes ctx added-nodes introduced-node->overrides)))

(defn- realize-add-nodes [ctx undoable-changes added-nodes]
  (if-let [{:keys [added-nodes
                   introduced-node->overrides]}
           (ig/basis-plan-add-nodes (:basis ctx) added-nodes)]
    (-> (perform-and-conj-change ctx undoable-changes (->AddNodesTXC added-nodes introduced-node->overrides))
        (coll/reduce=> added-nodes
          (fn [[ctx undoable-changes] added-node]
            (realize-defaults ctx undoable-changes added-node))))
    (pair ctx undoable-changes)))

(defonce/type AddNodesTXS [added-nodes]
  TransactionStep
  (step-type [_this]
    :tx-step/add-nodes)

  (metrics-key [_this]
    (if (= 1 (count added-nodes))
      (gt/node-id (nth added-nodes 0))
      (mapv gt/node-id added-nodes)))

  (realize [_this ctx undoable-changes]
    (realize-add-nodes ctx undoable-changes added-nodes)))

(defn add-nodes
  "*transaction step* - Add nodes to their corresponding graphs."
  [nodes]
  {:pre [(coll/eager-seqable? nodes)]}
  (if (coll/empty? nodes)
    []
    [(->AddNodesTXS (vec nodes))]))

(defn- ctx-perform-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node->overrides]
  (ctx-delete-nodes ctx nodes removed-arc->source+target-pkids overrides node->overrides))

(defn- ctx-revert-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node->overrides]
  (let [node-ids (mapv gt/node-id nodes)
        arcs (coll/into-> removed-arc->source+target-pkids []
               (map key))
        changed-node-ids (e/concat node-ids (e/map key node->overrides))
        ctx (-> ctx
                (update :basis ig/basis-revert-delete-nodes nodes removed-arc->source+target-pkids overrides node->overrides)
                (update :nodes-added into node-ids)
                (mark-nodes-outputs-activated nodes)
                (coll/reduce-kv=> removed-arc->source+target-pkids
                  (fn [ctx arc [_source-arc-pkids target-arc-pkids]]
                    (let [target-id (gt/target-id arc)]
                      (cond-> ctx
                        (and (coll/not-empty target-arc-pkids)
                             (gt/node-by-id-at (:basis ctx) target-id))
                        (mark-input-activated target-id (gt/target-label arc)))))))]
    (-> ctx
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed arcs))))

(defonce/type DeleteNodesTXC
  [deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node->overrides]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node->overrides))

  (revert [_this ctx]
    (ctx-revert-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node->overrides)))

(defn- realize-delete-nodes [ctx undoable-changes deleted-node-ids]
  (if-let [{:keys [deleted-nodes
                   removed-arc->source+target-pkids
                   removed-node->overrides
                   removed-overrides-by-id]}
           (ig/basis-plan-delete-nodes (:basis ctx) deleted-node-ids)]
    (let [change (->DeleteNodesTXC deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node->overrides)]
      (perform-and-conj-change ctx undoable-changes change))
    (pair ctx undoable-changes)))

(defonce/type DeleteNodesTXS [node-ids]
  TransactionStep
  (step-type [_this]
    :tx-step/delete-node)

  (metrics-key [_this]
    (if (= 1 (count node-ids))
      (nth node-ids 0)
      node-ids))

  (realize [_this ctx undoable-changes]
    (realize-delete-nodes ctx undoable-changes node-ids)))

(defn delete-node
  "*transaction step* - Delete a node from its graph."
  [node-id]
  [(->DeleteNodesTXS [node-id])])

(defn delete-nodes
  "*transaction step* - Delete nodes from their graphs."
  [node-ids]
  {:pre [(coll/eager-seqable? node-ids)]}
  (if (coll/empty? node-ids)
    []
    [(->DeleteNodesTXS (vec node-ids))]))

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

(defonce/type UpdateGraphValueTXC
  [graph-id graph-value-key old-value new-value]

  TransactionChange
  (perform [_this ctx]
    (update ctx :basis ig/basis-perform-update-graph-value graph-id graph-value-key new-value))

  (revert [_this ctx]
    (update ctx :basis ig/basis-revert-update-graph-value graph-id graph-value-key old-value)))

(defn- realize-update-graph-value
  [ctx undoable-changes graph-id graph-value-key update-fn args]
  (if-let [{:keys [graph-id
                   graph-value-key
                   old-value
                   new-value]}
           (ig/basis-plan-update-graph-value (:basis ctx) graph-id graph-value-key update-fn args)]
    (perform-and-conj-change ctx undoable-changes (->UpdateGraphValueTXC graph-id graph-value-key old-value new-value))
    (pair ctx undoable-changes)))

(defonce/type UpdateGraphValueTXS [graph-id graph-value-key update-fn args]
  TransactionStep
  (step-type [_this]
    :tx-step/update-graph-value)

  (metrics-key [_this]
    graph-id)

  (realize [_this ctx undoable-changes]
    (realize-update-graph-value ctx undoable-changes graph-id graph-value-key update-fn args)))

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
    (pair (ctx-callback ctx callback-fn args opts)
          undoable-changes)))

(defn callback
  "*transaction step* - Call a function from within the transaction."
  [callback-fn args opts]
  {:pre [(ifn? callback-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->CallbackTXS callback-fn args opts)])

(defonce/type ConnectArcsTXC
  [arc->source+target-pkids undoable]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-connect-arcs ctx arc->source+target-pkids undoable))

  (revert [_this ctx]
    (ctx-revert-connect-arcs ctx arc->source+target-pkids)))

(defonce/type DisconnectArcsTXC
  [arc->source+target-pkids undoable]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-disconnect-arcs ctx arc->source+target-pkids))

  (revert [_this ctx]
    (ctx-revert-disconnect-arcs ctx arc->source+target-pkids undoable)))

(defn- override-node-ids-removed-by-disconnect [ctx arc]
  (let [basis (:basis ctx)
        node-id->node #(gt/node-by-id-at basis %)
        source-id (gt/source-id arc)
        target-id (gt/target-id arc)
        target-label (gt/target-label arc)
        target (node-id->node target-id)]
    (if-not (and target
                 (-> target gt/node-type in/cascade-deletes (contains? target-label)))
      []
      (let [source-override-node-ids (ig/get-overrides basis source-id)
            target-override-node-ids (ig/get-overrides basis target-id)
            source-override-nodes (mapv node-id->node source-override-node-ids)]
        (coll/into-> target-override-node-ids []
          (mapcat
            (fn [target-override-node-id]
              (let [target-override-node (node-id->node target-override-node-id)
                    target-override-id (gt/override-id target-override-node)
                    traverse-fn (ig/override-traverse-fn basis target-override-id)

                    source-override-node-ids-in-target-override
                    (coll/into-> source-override-nodes []
                      (filter #(= target-override-id (gt/override-id %)))
                      (map gt/node-id))]

                (ig/pre-traverse basis source-override-node-ids-in-target-override traverse-fn)))))))))

(defn- realize-disconnect
  [ctx undoable-changes arc]
  (if-let [{:keys [arc->source+target-pkids]}
           (ig/basis-plan-disconnect-arc (:basis ctx) arc)]
    (let [undoable (some? undoable-changes)
          change (->DisconnectArcsTXC arc->source+target-pkids undoable)
          [ctx undoable-changes] (perform-and-conj-change ctx undoable-changes change)
          [_source-arc-pkids target-arc-pkids] (arc->source+target-pkids arc)
          deleted-override-node-ids (if (coll/empty? target-arc-pkids)
                                      []
                                      (override-node-ids-removed-by-disconnect ctx arc))]

      (realize-delete-nodes ctx undoable-changes deleted-override-node-ids))
    (pair ctx undoable-changes)))

(defn- realize-connect
  [{:keys [basis] :as ctx} undoable-changes arc]
  (let [source-id (gt/source-id arc)
        source-label (gt/source-label arc)
        target-id (gt/target-id arc)
        target-label (gt/target-label arc)]
    (if-let [source (gt/node-by-id-at basis source-id)]
      (if-let [target (gt/node-by-id-at basis target-id)]
        (let [target-node-type (gt/node-type target)
              target-input-cardinality (in/input-cardinality target-node-type target-label)

              [ctx undoable-changes :as ctx+undoable-changes]
              (cond-> (pair ctx undoable-changes)
                (= :one target-input-cardinality)
                (coll/reduce=> (ig/explicit-arcs-by-target basis target-id target-label)
                  (fn [[ctx undoable-changes] existing-arc]
                    (realize-disconnect ctx undoable-changes existing-arc))))]

          (assert-type-compatible source-id source source-label target-id target target-label)

          ;; Large projects have a lot of connections, so we have a fast-path
          ;; here in case we don't need to track changes or create an undo step.
          (if (and (nil? undoable-changes)
                   (:full-invalidation ctx))

            ;; Fast path. Just append the arc to the arc-tables without keeping
            ;; track of where it ended up.
            (let [old-basis (:basis ctx)
                  new-basis (ig/basis-perform-append-arc old-basis arc)]
              (if (identical? old-basis new-basis)
                ctx+undoable-changes
                (pair
                  (cond-> (assoc ctx :basis new-basis)
                    (contains? (in/cascade-deletes target-node-type) target-label)
                    (mark-override-nodes-affected target-id false))
                  undoable-changes)))

            ;; Regular path with undo and invalidation tracking.
            (if-let [{:keys [arc->source+target-pkids]}
                     (ig/basis-plan-connect-arc (:basis ctx) arc)]
              (let [undoable (some? undoable-changes)
                    change (->ConnectArcsTXC arc->source+target-pkids undoable)]
                (perform-and-conj-change ctx undoable-changes change))
              ctx+undoable-changes)))
        (pair ctx undoable-changes))
      (pair ctx undoable-changes))))

(defonce/type ConnectTXS [arc]
  TransactionStep
  (step-type [_this]
    :tx-step/connect)

  (metrics-key [_this]
    arc)

  (realize [_this ctx undoable-changes]
    (realize-connect ctx undoable-changes arc)))

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and
  label and a target node and label."
  [source-id source-label target-id target-label]
  [(->ConnectTXS (gt/->Arc source-id source-label target-id target-label))])

(defn- realize-expand
  [ctx undoable-changes tx-steps-fn args opts]
  (realize-tx
    ctx undoable-changes
    (if-not (:inject-evaluation-context opts)
      (apply tx-steps-fn args)
      (let [basis (:basis ctx)
            tx-data-context (:tx-data-context ctx)
            evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
        (apply tx-steps-fn evaluation-context args)))))

(defonce/type ExpandTXS [tx-steps-fn args opts]
  TransactionStep
  (step-type [_this]
    :tx-step/expand)

  (metrics-key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (realize-expand ctx undoable-changes tx-steps-fn args opts)))

(defn expand
  "*transaction step* - Call a function and execute the returned transaction
  steps within the transaction."
  [tx-steps-fn args opts]
  {:pre [(ifn? tx-steps-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->ExpandTXS tx-steps-fn args opts)])

(defonce/type DisconnectTXS [arc]
  TransactionStep
  (step-type [_this]
    :tx-step/disconnect)

  (metrics-key [_this]
    arc)

  (realize [_this ctx undoable-changes]
    (realize-disconnect ctx undoable-changes arc)))

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a transaction step
  disconnecting a source node and label from a target node and label."
  [source-id source-label target-id target-label]
  [(->DisconnectTXS (gt/->Arc source-id source-label target-id target-label))])

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

(defonce/type InvalidateTXC [node-id]
  TransactionChange
  (perform [_this ctx]
    (ctx-invalidate ctx node-id))

  (revert [_this ctx]
    (ctx-invalidate ctx node-id)))

(defn- realize-invalidate
  [ctx undoable-changes node-id]
  (perform-and-conj-change ctx undoable-changes (->InvalidateTXC node-id)))

(defonce/type InvalidateTXS [node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate)

  (metrics-key [_this]
    node-id)

  (realize [_this ctx undoable-changes]
    (realize-invalidate ctx undoable-changes node-id)))

(defn invalidate
  [node-id]
  [(->InvalidateTXS node-id)])

(defonce/type InvalidateOutputTXC [node-id output-label]
  TransactionChange
  (perform [_this ctx]
    (ctx-invalidate-output ctx node-id output-label))

  (revert [_this ctx]
    (ctx-invalidate-output ctx node-id output-label)))

(defn- realize-invalidate-output
  [ctx undoable-changes node-id output-label]
  (perform-and-conj-change ctx undoable-changes (->InvalidateOutputTXC node-id output-label)))

(defonce/type InvalidateOutputTXS [node-id output-label]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate-output)

  (metrics-key [_this]
    (pair node-id output-label))

  (realize [_this ctx undoable-changes]
    (realize-invalidate-output ctx undoable-changes node-id output-label)))

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

(defn tx-step-added-nodes
  [^AddNodesTXS tx-step]
  (when (instance? AddNodesTXS tx-step)
    (.-added-nodes tx-step)))

(defn tx-step-added-arc
  ^Arc [^ConnectTXS tx-step]
  (when (instance? ConnectTXS tx-step)
    (.-arc tx-step)))

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
   :initial-basis basis
   :nodes-affected #{}
   :nodes-added []
   :nodes-deleted {}
   :outputs-modified #{}
   :override-nodes-affected-undoability {}
   :override-nodes-affected-ordered []
   :successor-arcs-changed #{}
   :successor-node-ids-changed #{}
   :successors-changed {}
   :node-id-generators node-id-generators
   :override-id-generator override-id-generator
   :completed-action-count 0
   :txid (new-txid)
   :tx-data-context (atom tx-data-context-map)
   :metrics metrics-collector
   :full-invalidation full-invalidation})

(defn ctx-graphs
  [{:keys [basis] :as _ctx}]
  {:pre [(gt/basis? basis)]}
  (:graphs basis))

(defn- finalize-successors-changed
  [{:keys [basis initial-basis successor-arcs-changed successor-node-ids-changed] :as ctx}]
  (du/measuring (:metrics ctx) :finalize-successors-changed
    (cond-> ctx
      (not (:full-invalidation ctx))
      (flag-successors-changed
        (e/concat
          (node-successor-changes initial-basis basis successor-node-ids-changed)
          (arc-successor-changes initial-basis basis successor-arcs-changed))))))

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
      finalize-successors-changed
      update-successors
      trace-dependencies
      finalize-update))

(defn transact*
  [ctx undoable-changes tx-data]
  (when *tx-debug*
    (println (txerrstr ctx "tx-data" tx-data)))
  (let [[ctx undoable-changes] (realize-tx ctx undoable-changes tx-data)
        [ctx undoable-changes] (realize-update-overrides ctx undoable-changes)
        undoable-changes (if undoable-changes (persistent! undoable-changes) [])]
    (-> ctx
        (assoc :undoable-changes undoable-changes)
        (finalize-applied-changes))))

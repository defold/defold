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
  (:require [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [clojure.lang ArityException ExceptionInfo]
           [internal.graph.types Arc]
           [internal.node ShellNode]
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
         ^:private realize-delete-nodes
         materialize-node!)

;; Enables logging of materialization events.
;;   0 = No logging.
;;   1 = Log failures that prevent materialization.
;;   2 = Also log materialized resources with timings.
;;   3 = Also log commits to the system.
;;   4 = Also log promise producer / consumer and thread details.
(defonce ^:const materialization-log-level 0)

(defmacro log-node-materialization! [log-level node message & log-kv-args]
  (when (<= (long log-level) materialization-log-level)
    (list* `log/debug
           (cond-> [:message message
                    :resource `(:resource ~node)]

             (<= 4 materialization-log-level)
             (into [:thread `(.getName (Thread/currentThread))])

             :always
             (into log-kv-args)))))

(defmacro log-committed-materialization-results! [materialization-results old-system new-system]
  (when (<= 3 materialization-log-level)
    `(let [~'old-materialization-results (:materialization-results ~'old-system)
           ~'new-basis (is/basis ~new-system)]
       (coll/run!-> ~materialization-results
         (fn [[~'node-id ~'materialize-info]]
           (when-let [~'node (ig/node-by-id-at ~'new-basis ~'node-id)]
             (when-not (identical? ~'materialize-info
                                   (get ~'old-materialization-results ~'node-id))
               (log-node-materialization! 3 ~'node "Materialization committed."))))))))

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
     (pair (cond-> (perform-change ~ctx transaction-change#)
             (:realized-changes ~ctx) (update :realized-changes conj transaction-change#))
           (conj-change ~undoable-changes transaction-change#))))

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------

(declare ^:private apply-evaluation-context-changes!)

(defn- realize-tx-impl
  [ctx undoable-changes tx-data evaluation-context]
  (let [is-non-undoable (non-undoable? tx-data)
        tx-data (cond-> tx-data is-non-undoable non-undoable-tx-data)
        mutated-undoable-changes (if is-non-undoable nil undoable-changes)

        [ctx mutated-undoable-changes]
        (coll/reduce->
          tx-data
          (pair ctx mutated-undoable-changes)
          (fn [[ctx mutated-undoable-changes] ^TransactionStep tx-step]
            ;; Producing a lazy step can materialize nodes. Merge those changes
            ;; before executing the step, without realizing later producers
            ;; early.
            (let [ctx (if-not evaluation-context
                        ctx
                        (apply-evaluation-context-changes! ctx evaluation-context))]
              (cond
                (nil? tx-step)
                (pair ctx mutated-undoable-changes)

                (non-undoable? tx-step)
                (let [[ctx] (realize-tx-impl ctx nil tx-step evaluation-context)]
                  (pair ctx mutated-undoable-changes))

                (sequential? tx-step)
                (realize-tx-impl ctx mutated-undoable-changes tx-step evaluation-context)

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
                        mutated-undoable-changes))))))]
    (pair (if-not evaluation-context
            ctx
            (apply-evaluation-context-changes! ctx evaluation-context))
          (or mutated-undoable-changes undoable-changes))))

(defn realize-tx
  [ctx undoable-changes tx-data]
  (realize-tx-impl ctx undoable-changes tx-data nil))

(defn- make-evaluation-context [ctx]
  (in/custom-evaluation-context
    {:basis (:basis ctx)
     :active-materialization (:active-materialization ctx)
     :shared-materialization-registry-atom (:shared-materialization-registry-atom ctx)
     :materialization-results (:materialization-results ctx)
     :initial-invalidate-counters (:initial-invalidate-counters ctx)
     :tx-data-context-atom (:tx-data-context-atom ctx)
     :node-id-generator (:node-id-generator ctx)
     :override-id-generator (:override-id-generator ctx)}))

(defn- track-node-output-changes? [ctx node-id]
  ;; Outputs on nodes absent from the initial basis cannot have cached values
  ;; from this lifetime. Their existing consumers are marked separately when
  ;; nodes and connections are added or restored.
  (and (not (:full-invalidation ctx))
       (contains? (gt/nodes (:initial-basis ctx)) node-id)))

(defn- mark-input-activated
  [ctx node-id input-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if-not (track-node-output-changes? ctx node-id)
    ctx
    (let [basis (:basis ctx)
          dirty-deps (-> (ig/node-by-id-at basis node-id)
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
  (if-not (track-node-output-changes? ctx node-id)
    ctx
    (let [nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (conj nodes-affected (gt/endpoint node-id output-label))))))

(defn- mark-outputs-activated
  [ctx node-id output-labels]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if-not (track-node-output-changes? ctx node-id)
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
                  (when (and (track-node-output-changes? ctx (gt/target-id arc))
                             (ig/node-by-id-at basis (gt/target-id arc)))
                    (gt/target-endpoint arc))))
              arcs)))))

(defn- mark-all-outputs-activated
  [ctx node-id]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if-not (track-node-output-changes? ctx node-id)
    ctx
    (let [basis (:basis ctx)
          output-labels (-> (ig/node-by-id-at basis node-id)
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
            (if-not (track-node-output-changes? ctx node-id)
              nodes-affected
              (coll/into-> (-> node gt/node-type in/output-labels) nodes-affected
                (map #(gt/endpoint node-id %))))))))))

(defn- mark-added-node-consumers-activated [ctx node-ids is-new-override-tree]
  ;; Initial creation of a new override tree cannot activate existing consumers.
  ;; Undo/redo resolves its consumers because later additions may survive.
  (if (or (:full-invalidation ctx) is-new-override-tree)
    ctx
    (let [basis (:basis ctx)]
      (-> ctx
          ;; Adding a node makes dormant and inherited outgoing arcs effective.
          ;; Invalidate existing consumers directly instead of propagating from
          ;; every output of the added node.
          (mark-arc-targets-activated
            (e/mapcat #(ig/arcs-by-source basis %) node-ids))
          ;; Override nodes can survive undoing creation of their original.
          ;; Their inherited properties become available again on redo, even
          ;; when no explicit connection is restored.
          (coll/reduce=> (ig/pre-traverse basis node-ids ig/get-overrides)
            (fn [ctx node-id]
              (mark-all-outputs-activated ctx node-id)))))))

(defn- next-node-id [ctx]
  (gt/next-node-id (:node-id-generator ctx)))

(defn- next-override-id [ctx]
  (gt/next-override-id (:override-id-generator ctx)))

(defn- ctx-perform-add-override
  [ctx override-id override]
  (update ctx :basis ig/basis-perform-add-override override-id override))

(defn- ctx-revert-add-override
  [ctx override-id]
  (update ctx :basis ig/basis-revert-add-override override-id))

(defn- flag-successors-changed
  "Merges successor changes into the context. A node id means every label on
  the node changed; an arc means only its source label changed."
  [ctx changes]
  (if (:full-invalidation ctx)
    ctx
    (let [successors-changed (:successors-changed ctx)

          affected-changes
          (filterv
            (fn [change]
              (if (gt/node-id? change)
                (get successors-changed change ::not-found)
                (let [node-id (gt/source-id change)
                      label (gt/source-label change)
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
                    (let [node-id (gt/source-id change)
                          label (gt/source-label change)
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

(defn- successor-changes
  "Returns a deferred stream of successor changes caused by the specified
  nodes and arcs differing between the old-basis and the new-basis."
  [old-basis new-basis node-ids arcs]
  {:pre [(set? node-ids)]}
  (let [arc-target->arc-set (coll/reduce-> arcs {}
                              (fn [arc-target->arc-set arc]
                                (update arc-target->arc-set
                                        (gt/target arc)
                                        coll/conj-set
                                        arc)))]
    (coll/into-> (pair old-basis new-basis) :eduction
      (mapcat
        (fn [basis]
          (let [existing-node-ids (filterv #(ig/node-by-id-at basis %) node-ids)
                node-and-override-ids (ig/pre-traverse basis existing-node-ids ig/get-overrides)
                node-and-override-id-set (set node-and-override-ids)]
            (e/concat
              ;; The changed nodes and their originals must account for changes
              ;; to their immediate override nodes.
              (e/mapcat #(ig/override-originals basis %) existing-node-ids)

              ;; Sources targeting a changed node or any of its overrides must
              ;; account for changes to their effective outgoing arcs.
              (e/mapcat #(ig/arcs-by-target basis %) node-and-override-ids)

              (e/mapcat
                (fn [[[target-id target-label] source-arcs]]
                  ;; The changed-node traversal above already covered every
                  ;; source targeting this node and its overrides.
                  (when-not (contains? node-and-override-id-set target-id)
                    (let [target-node (ig/node-by-id-at basis target-id)
                          direct-source-arcs (e/remove
                                               (fn [arc]
                                                 ;; A changed source node
                                                 ;; already invalidates all of
                                                 ;; its labels.
                                                 (contains? node-ids (gt/source-id arc)))
                                               source-arcs)]
                      ;; Naturally, we don't need to traverse further if the
                      ;; target node no longer exists. But we also do not need
                      ;; to traverse further if the target is an array input on
                      ;; a regular node with no overrides. If A is already
                      ;; connected to the array input, and we connect B, that
                      ;; doesn't change A's successors. Override nodes need
                      ;; additional consideration because explicit connections
                      ;; will replace inherited connections.
                      (if (or (nil? target-node)
                              (and (= :many (in/input-cardinality (gt/node-type target-node) target-label))
                                   (not (gt/original target-node))
                                   (coll/empty? (ig/get-overrides basis target-id))))
                        direct-source-arcs
                        (e/concat
                          direct-source-arcs
                          (e/mapcat #(ig/arcs-by-target basis % target-label)
                                    (ig/pre-traverse basis [target-id] ig/get-overrides)))))))
                arc-target->arc-set)))))
      (distinct))))

(defn- ctx-add-nodes [ctx nodes introduced-node-id->pkid->override-node-id is-new-override-tree]
  (let [node-ids (mapv gt/node-id nodes)
        ctx (-> ctx
                (update :basis ig/basis-perform-add-nodes nodes introduced-node-id->pkid->override-node-id)
                (update :nodes-added into node-ids))]
    (if (:full-invalidation ctx)
      ctx
      (let [new-basis (:basis ctx)
            source-arcs (coll/into-> node-ids []
                          (mapcat #(ig/explicit-arcs-by-source new-basis %)))]
        (-> ctx
            (mark-added-node-consumers-activated node-ids is-new-override-tree)
            ;; Adding an override changes its original's successors, but
            ;; successor-changes already walks each added node's chain of
            ;; originals via override-originals and flags their successor cache
            ;; entries. Only the added nodes need to be roots for its separate
            ;; traversal of override descendants and their incoming arcs.
            ;;
            ;; Including originals here would also traverse every existing
            ;; sibling override subtree. As resources are materialized, this
            ;; would repeatedly scan previously created instances of the same
            ;; resource, even though their connections have not changed.
            (mark-successor-nodes-changed node-ids)
            (mark-successor-arcs-changed source-arcs))))))

(defn- ctx-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id]
  (let [node-ids (mapv gt/node-id nodes)
        nodes-by-id (coll/pair-map-by gt/node-id identity nodes)
        arcs (coll/into-> removed-arc->source+target-pkids []
               (map key))
        source-arcs (coll/into-> removed-arc->source+target-pkids []
                      (keep
                        (fn [[arc [_source-arc-pkids target-arc-pkids]]]
                          (when (and (coll/not-empty target-arc-pkids)
                                     (contains? nodes-by-id (gt/source-id arc)))
                            arc))))
        changed-node-ids (e/concat node-ids (e/map key node-id->pkid->override-node-id))]
    (-> ctx
        (mark-nodes-outputs-activated nodes)
        (mark-arc-targets-activated source-arcs)
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed arcs)
        (update :nodes-deleted into nodes-by-id)
        (update :nodes-added coll/transform-> (remove nodes-by-id))
        (update :basis ig/basis-perform-delete-nodes nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id))))

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

(defn- realize-override
  [ctx undoable-changes root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  (let [evaluation-context (make-evaluation-context ctx)
        _ (materialize-node! root-id evaluation-context)
        ctx (apply-evaluation-context-changes! ctx evaluation-context)
        basis (:basis ctx)
        node-ids (ig/pre-traverse basis [root-id] traverse-fn)
        override-id (next-override-id ctx)
        override-nodes (mapv (fn [original-node-id]
                               (let [override-node-id (next-node-id ctx)
                                     original-node (ig/node-by-id-at basis original-node-id)
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
        [ctx undoable-changes] (realize-add-nodes ctx undoable-changes override-nodes true)
        [ctx undoable-changes] (realize-add-override ctx undoable-changes override-id root-id traverse-fn init-props-fn)
        evaluation-context (make-evaluation-context ctx)
        tx-data (init-fn evaluation-context original-node-id->override-node-id)
        ctx (apply-evaluation-context-changes! ctx evaluation-context)]
    (realize-tx-impl ctx undoable-changes tx-data evaluation-context)))

(defn- node-id->override-id [basis node-id]
  (->> node-id
       (ig/node-by-id-at basis)
       gt/override-id))

(defn- realize-make-override-nodes [ctx undoable-changes override-id node-ids init-props-fn]
  (coll/reduce-> node-ids (pair ctx undoable-changes)
    (fn [[ctx undoable-changes] node-id]
      (let [basis (:basis ctx)]
        (if (coll/some #(= override-id (node-id->override-id basis %))
                       (ig/get-overrides basis node-id))
          (pair ctx undoable-changes)
          (let [original-node (ig/node-by-id-at basis node-id)
                node-type (gt/node-type original-node)
                properties (when init-props-fn
                             (init-props-fn basis node-id node-type))
                new-override-node-id (next-node-id ctx)
                new-override-node (in/make-override-node
                                    override-id
                                    new-override-node-id
                                    node-type
                                    node-id
                                    properties)]
            (realize-add-nodes ctx undoable-changes [new-override-node] false)))))))

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

(defn- ctx-perform-clear-override-nodes [ctx original-node-id removed-pkid->override-node-id]
  (-> ctx
      (update :basis ig/basis-perform-clear-override-nodes original-node-id removed-pkid->override-node-id)
      (mark-successor-nodes-changed [original-node-id])))

(defn- ctx-revert-clear-override-nodes [ctx original-node-id removed-pkid->override-node-id]
  (-> ctx
      (update :basis ig/basis-revert-clear-override-nodes original-node-id removed-pkid->override-node-id)
      (mark-successor-nodes-changed [original-node-id])))

(defonce/type ClearOverrideNodesTXC
  [original-node-id removed-pkid->override-node-id]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-clear-override-nodes ctx original-node-id removed-pkid->override-node-id))

  (revert [_this ctx]
    (ctx-revert-clear-override-nodes ctx original-node-id removed-pkid->override-node-id)))

(defn- ctx-perform-replace-override-root [ctx override-id new-override]
  (update ctx :basis ig/basis-perform-replace-override override-id new-override))

(defn- ctx-revert-replace-override-root [ctx override-id old-override]
  (update ctx :basis ig/basis-revert-replace-override override-id old-override))

(defonce/type ReplaceOverrideRootTXC
  [override-id old-override new-override]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-replace-override-root ctx override-id new-override))

  (revert [_this ctx]
    (ctx-revert-replace-override-root ctx override-id old-override)))

(defn- ctx-perform-repoint-override-node
  [ctx override-node-id new-original-node-id new-original-pkid]
  (let [old-basis (:basis ctx)
        ctx (update ctx :basis ig/basis-perform-repoint-override-node override-node-id new-original-node-id new-original-pkid)]
    (if (identical? old-basis (:basis ctx))
      ctx
      (-> ctx
          (mark-all-outputs-activated override-node-id)
          (mark-successor-nodes-changed [override-node-id new-original-node-id])))))

(defn- ctx-revert-repoint-override-node
  [ctx override-node-id old-original-node-id new-original-node-id new-original-pkid]
  (let [old-basis (:basis ctx)
        ctx (update ctx :basis ig/basis-revert-repoint-override-node override-node-id old-original-node-id new-original-node-id new-original-pkid)]
    (if (identical? old-basis (:basis ctx))
      ctx
      (-> ctx
          (mark-all-outputs-activated override-node-id)
          (mark-successor-nodes-changed [override-node-id new-original-node-id])))))

(defonce/type RepointOverrideNodeTXC
  [override-node-id old-original-node-id new-original-node-id new-original-pkid]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-repoint-override-node ctx override-node-id new-original-node-id new-original-pkid))

  (revert [_this ctx]
    (ctx-revert-repoint-override-node ctx override-node-id old-original-node-id new-original-node-id new-original-pkid)))

(defn- realize-transfer-overrides
  [{:keys [active-materialization] :as ctx} undoable-changes from-id->to-id]
  (when active-materialization
    (throw (ex-info "Materialization cannot transfer existing overrides."
                    {:node-id (:node-id active-materialization)
                     :from-id->to-id from-id->to-id})))
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
                               removed-pkid->override-node-id]}
                       (ig/basis-plan-clear-override-nodes (:basis ctx) from-id override-node-ids)]
                (perform-and-conj-change ctx undoable-changes (->ClearOverrideNodesTXC original-node-id removed-pkid->override-node-id))
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
                  override-node (ig/node-by-id-at basis override-node-id)
                  current-original-id (gt/original override-node)
                  new-original-id (from-id->to-id current-original-id)]
              (if-let [{:keys [override-node-id
                               old-original-node-id
                               new-original-node-id
                               new-original-pkid]}
                       (ig/basis-plan-repoint-override-node basis override-node-id new-original-id)]
                (perform-and-conj-change ctx undoable-changes (->RepointOverrideNodeTXC override-node-id old-original-node-id new-original-node-id new-original-pkid))
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
  (if-not (track-node-output-changes? ctx node-id)
    ctx
    (let [basis (:basis ctx)
          node (ig/node-by-id-at basis node-id)]
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
    (let [evaluation-context (make-evaluation-context ctx)
          setter-actions (setter-fn evaluation-context node-id old-value new-value)]
      (when *tx-debug*
        (println (txerrstr ctx "setter actions" (seq setter-actions))))
      (pair evaluation-context setter-actions))
    (catch ArityException ae
      (when *tx-debug*
        (println "ArityException while inside " setter-fn " on node " node-id " with " old-value new-value (gt/node-type (ig/node-by-id-at basis node-id))))
      (throw ae))
    (catch Exception e
      (let [node-type (:name @(gt/node-type (ig/node-by-id-at basis node-id)))]
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
  [{:keys [active-materialization] :as ctx} undoable-changes node property-label old-value new-value]
  (let [basis (:basis ctx)
        node-id (gt/node-id node)
        _ (when (and active-materialization
                     (not= (:node-id active-materialization) node-id)
                     (ig/node-by-id-at (:post-prerequisites-basis active-materialization) node-id))
            (throw (ex-info "Materialization cannot modify an existing external node."
                            {:node-id (:node-id active-materialization)
                             :modified-node-id node-id
                             :property property-label})))
        node-type (gt/node-type node)
        assigned-properties (gt/assigned-properties node basis)
        property-assigned (contains? assigned-properties property-label)

        ctx+undoable-changes
        (if-let [{:keys [node-id
                         property-label
                         old-raw-value
                         new-raw-value]}
                 (ig/basis-plan-set-raw-property basis node-id property-label new-value)]
          (do
            (in/validate-property-value node-type node-id property-label new-raw-value)
            (perform-and-conj-change ctx undoable-changes (->SetRawPropertyTXC node-id property-label old-raw-value new-raw-value)))
          (pair ctx undoable-changes))

        realize-setter-actions
        (or (not property-assigned)
            (not= old-value new-value))]

    (if realize-setter-actions
      (if-let [setter-fn (in/property-setter node-type property-label)]
        (let [[ctx undoable-changes] ctx+undoable-changes
              [evaluation-context setter-actions] (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id old-value new-value)
              ctx (apply-evaluation-context-changes! ctx evaluation-context)]
          (realize-tx-impl ctx undoable-changes setter-actions evaluation-context))
        ctx+undoable-changes)
      ctx+undoable-changes)))

(defn- realize-set-property
  [ctx undoable-changes node-id property-label new-value]
  (let [basis (:basis ctx)
        node (ig/node-by-id-at basis node-id)]
    (if-not node
      (pair ctx undoable-changes)
      (let [evaluation-context (make-evaluation-context ctx)
            _ (materialize-node! node-id evaluation-context)
            node (ig/node-by-id-at @(:basis-ref evaluation-context) node-id)
            old-value (in/node-property-value node property-label evaluation-context)
            ctx (apply-evaluation-context-changes! ctx evaluation-context)
            node (ig/node-by-id-at (:basis ctx) node-id)]
        (realize-set-property-impl ctx undoable-changes node property-label old-value new-value)))))

(defn- realize-update-property
  [ctx undoable-changes node-id property-label update-fn args opts]
  (let [basis (:basis ctx)
        node (ig/node-by-id-at basis node-id)]
    (if-not node
      (pair ctx undoable-changes)
      (let [evaluation-context (make-evaluation-context ctx)
            _ (materialize-node! node-id evaluation-context)
            node (ig/node-by-id-at @(:basis-ref evaluation-context) node-id)
            old-value (in/node-property-value node property-label evaluation-context)
            new-value (if (:inject-evaluation-context opts)
                        (apply update-fn evaluation-context old-value args)
                        (apply update-fn old-value args))
            ctx (apply-evaluation-context-changes! ctx evaluation-context)
            node (ig/node-by-id-at (:basis ctx) node-id)]
        (realize-set-property-impl ctx undoable-changes node property-label old-value new-value)))))

(defn- realize-clear-property
  [{:keys [active-materialization] :as ctx} undoable-changes node-id property-label]
  (when (and active-materialization
             (ig/node-by-id-at (:post-prerequisites-basis active-materialization) node-id))
    (throw (ex-info "Materialization cannot clear a property of an existing node."
                    {:node-id (:node-id active-materialization)
                     :modified-node-id node-id
                     :property property-label})))
  (let [basis (:basis ctx)
        node (ig/node-by-id-at basis node-id)]
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
            (let [evaluation-context (make-evaluation-context ctx)
                  old-value (in/node-property-value node property-label evaluation-context)
                  ctx (apply-evaluation-context-changes! ctx evaluation-context)
                  [ctx undoable-changes] (perform-and-conj-change ctx undoable-changes change)
                  [evaluation-context setter-actions] (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id old-value nil)
                  ctx (apply-evaluation-context-changes! ctx evaluation-context)]
              (realize-tx-impl ctx undoable-changes setter-actions evaluation-context))))
        (pair ctx undoable-changes)))))

(defn- realize-defaults
  [ctx undoable-changes node]
  (let [node-id (gt/node-id node)
        node-type (gt/node-type node)
        ordered-property-setter-infos (in/ordered-property-setter-infos node-type)]
    (if (coll/empty? ordered-property-setter-infos)
      (pair ctx undoable-changes)
      (let [assigned-properties (gt/assigned-properties node (:basis ctx))
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
                    [evaluation-context setter-actions] (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id nil property-value)
                    ctx (apply-evaluation-context-changes! ctx evaluation-context)]
                (realize-tx-impl ctx undoable-changes setter-actions evaluation-context))
              ctx+undoable-changes)))))))

(defn- ctx-perform-add-nodes [ctx added-nodes introduced-node-id->pkid->override-node-id]
  ;; A tree created by this change can have surviving nodes when it is redone.
  ;; Only initial creation may skip resolving inherited outgoing arcs.
  (ctx-add-nodes ctx added-nodes introduced-node-id->pkid->override-node-id false))

(defn- ctx-revert-add-nodes [ctx added-nodes introduced-node-id->pkid->override-node-id]
  (let [old-basis (:basis ctx)
        node-ids (mapv gt/node-id added-nodes)
        nodes-by-id (coll/pair-map-by gt/node-id identity added-nodes)
        source-arcs (coll/into-> node-ids []
                      (mapcat #(ig/explicit-arcs-by-source old-basis %)))
        changed-node-ids (e/concat node-ids (e/map key introduced-node-id->pkid->override-node-id))
        override-ids (coll/into-> (coll/vals nodes-by-id) #{} (keep gt/override-id))

        ;; Descendant overrides can survive undoing their original's creation.
        surviving-override-nodes
        (->> (ig/pre-traverse old-basis node-ids ig/get-overrides)
             (e/remove nodes-by-id)
             (e/keep #(ig/node-by-id-at old-basis %)))]
    (-> ctx
        (mark-nodes-outputs-activated added-nodes)
        (mark-arc-targets-activated source-arcs)
        (cond-> (not (coll/empty? override-ids))
          (mark-arc-targets-activated
            (let [originals (coll/into-> node-ids #{} (mapcat #(ig/override-originals old-basis %)))
                  target-roots (coll/into-> originals #{}
                                 (mapcat #(ig/explicit-arcs-by-source old-basis %))
                                 (map gt/target-id))
                  ;; Removed targets and their surviving overrides already
                  ;; have every output marked. Inspect other branches only.
                  candidates (ig/pre-traverse old-basis target-roots ig/get-overrides :seen (set node-ids))]
              (coll/into-> candidates :eduction
                (keep #(ig/node-by-id-at old-basis %))
                (filter #(contains? override-ids (gt/override-id %)))
                (mapcat #(ig/arcs-by-target old-basis (gt/node-id %)))
                (filter #(contains? nodes-by-id (gt/source-id %)))))))
        (mark-nodes-outputs-activated surviving-override-nodes)
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed source-arcs)
        (update :nodes-deleted into nodes-by-id)
        (update :nodes-added coll/transform-> (remove nodes-by-id))
        (update :basis ig/basis-revert-add-nodes added-nodes introduced-node-id->pkid->override-node-id))))

(defn- ctx-callback
  [ctx fn args opts]
  (if (:inject-evaluation-context opts)
    (let [evaluation-context (make-evaluation-context ctx)]
      (apply fn evaluation-context args)
      (apply-evaluation-context-changes! ctx evaluation-context))
    (do
      (apply fn args)
      ctx)))

(defn- mark-override-nodes-affected [ctx target-id undoable]
  (let [override-nodes-affected-undoability (:override-nodes-affected-undoability ctx)
        previous-mark-undoability (get override-nodes-affected-undoability target-id ::not-found)]
    (cond
      (= ::not-found previous-mark-undoability)
      (let [override-nodes-affected-ordered (:override-nodes-affected-ordered ctx)]
        (assoc ctx
          :override-nodes-affected-undoability (assoc override-nodes-affected-undoability target-id undoable)
          :override-nodes-affected-ordered (conj override-nodes-affected-ordered target-id)))

      (not= previous-mark-undoability undoable)
      (throw
        (ex-info
          "Cannot mix undoable and non-undoable changes to an override node in the same transaction."
          {:target-id target-id}))

      :else
      ctx)))

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

(defn- ctx-perform-append-arc
  [ctx arc target-node-type]
  (let [old-basis (:basis ctx)
        new-basis (ig/basis-perform-append-arc old-basis arc)]
    (if (identical? old-basis new-basis)
      ctx
      (let [target-id (gt/target-id arc)
            target-label (gt/target-label arc)]
        (cond-> (-> ctx
                    (assoc :basis new-basis)
                    (mark-input-activated target-id target-label)
                    (mark-successor-arcs-changed [arc]))
          (contains? (in/cascade-deletes target-node-type) target-label)
          (mark-override-nodes-affected target-id false))))))

(defn- ctx-replace-arc
  [ctx removed-arc removed-source-arc-pkids added-arc added-source-arc-pkids undoable]
  (let [old-basis (:basis ctx)
        new-basis (ig/basis-perform-replace-arc old-basis
                                                removed-arc removed-source-arc-pkids
                                                added-arc added-source-arc-pkids)]
    (if (identical? old-basis new-basis)
      ctx
      (let [target-arc (or added-arc removed-arc)
            target-id (gt/target-id target-arc)
            target-label (gt/target-label target-arc)
            target-node (ig/node-by-id-at new-basis target-id)
            changed-arcs (cond-> []
                           added-arc (conj added-arc)
                           removed-arc (conj removed-arc))]
        (cond-> (-> ctx
                    (assoc :basis new-basis)
                    (mark-successor-arcs-changed changed-arcs))
          target-node
          (mark-input-activated target-id target-label)

          (and added-arc
               target-node
               (contains? (-> target-node gt/node-type in/cascade-deletes) target-label))
          (mark-override-nodes-affected target-id undoable))))))

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
                                    (ig/node-by-id-at new-basis target-id))
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
                         (ig/node-by-id-at new-basis (gt/target-id arc)))
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
  (if (ig/node-by-id-at (:basis ctx) node-id)
    (mark-all-outputs-activated ctx node-id)
    ctx))

(defn- ctx-invalidate-output [ctx node-id output-label]
  (mark-output-activated ctx node-id output-label))

;; ---------------------------------------------------------------------------
;; Transaction steps
;; ---------------------------------------------------------------------------

(defonce/type AddNodesTXC
  [added-nodes introduced-node-id->pkid->override-node-id]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-add-nodes ctx added-nodes introduced-node-id->pkid->override-node-id))

  (revert [_this ctx]
    (ctx-revert-add-nodes ctx added-nodes introduced-node-id->pkid->override-node-id)))

(defonce/type MaterializationAddNodesTXC [nodes]
  TransactionChange
  (perform [_ ctx]
    ;; Override slots, like arc slots, belong to the destination basis.
    (if-let [{:keys [added-nodes introduced-node-id->pkid->override-node-id]}
             (ig/basis-plan-add-nodes (:basis ctx) nodes)]
      (ctx-perform-add-nodes ctx added-nodes introduced-node-id->pkid->override-node-id)
      ctx))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defn- realize-add-nodes [{:keys [active-materialization] :as ctx} undoable-changes added-nodes is-new-override-tree]
  (when (and active-materialization
             (coll/any? #(ig/node-by-id-at (:post-prerequisites-basis active-materialization) (gt/node-id %)) added-nodes))
    (throw (ex-info "Materialization cannot replace existing nodes."
                    {:node-id (:node-id active-materialization)
                     :added-node-ids (mapv gt/node-id added-nodes)})))
  (if-let [{:keys [added-nodes
                   introduced-node-id->pkid->override-node-id]}
           (ig/basis-plan-add-nodes (:basis ctx) added-nodes)]
    (let [change (if active-materialization
                   (->MaterializationAddNodesTXC added-nodes)
                   (->AddNodesTXC added-nodes introduced-node-id->pkid->override-node-id))
          ctx (cond-> (ctx-add-nodes ctx added-nodes introduced-node-id->pkid->override-node-id is-new-override-tree)
                (:realized-changes ctx)
                (update :realized-changes conj change))
          undoable-changes (conj-change undoable-changes change)]
      (coll/reduce=> (pair ctx undoable-changes) added-nodes
        (fn [[ctx undoable-changes] added-node]
          (if (in/shell-node? added-node)
            (pair ctx undoable-changes)
            (realize-defaults ctx undoable-changes added-node)))))
    (pair ctx undoable-changes)))

(defonce/type AddNodesTXS [added-nodes]
  TransactionStep
  (step_type [_this]
    :tx-step/add-nodes)

  (metrics_key [_this]
    (if (= 1 (count added-nodes))
      (gt/node-id (nth added-nodes 0))
      (mapv gt/node-id added-nodes)))

  (realize [_this ctx undoable-changes]
    (realize-add-nodes ctx undoable-changes added-nodes false)))

(defn add-nodes
  "*transaction step* - Add nodes to the graph."
  [nodes]
  {:pre [(coll/eager-seqable? nodes)]}
  (if (coll/empty? nodes)
    []
    [(->AddNodesTXS (vec nodes))]))

(defn- ctx-perform-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id]
  (ctx-delete-nodes ctx nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id))

(defn- ctx-revert-delete-nodes [ctx nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id]
  (let [node-ids (mapv gt/node-id nodes)
        arcs (coll/into-> removed-arc->source+target-pkids []
               (map key))
        changed-node-ids (e/concat node-ids (e/map key node-id->pkid->override-node-id))
        ctx (-> ctx
                (update :basis ig/basis-revert-delete-nodes nodes removed-arc->source+target-pkids overrides node-id->pkid->override-node-id)
                (update :nodes-added into node-ids)
                (mark-added-node-consumers-activated node-ids false)
                (coll/reduce-kv=> removed-arc->source+target-pkids
                  (fn [ctx arc [_source-arc-pkids target-arc-pkids]]
                    (let [target-id (gt/target-id arc)]
                      (cond-> ctx
                        (and (coll/not-empty target-arc-pkids)
                             (ig/node-by-id-at (:basis ctx) target-id))
                        (mark-input-activated target-id (gt/target-label arc)))))))]
    (-> ctx
        (mark-successor-nodes-changed changed-node-ids)
        (mark-successor-arcs-changed arcs))))

(defonce/type DeleteNodesTXC
  [deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id))

  (revert [_this ctx]
    (ctx-revert-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id)))

(defonce/type MaterializationDeleteNodesTXC [node-ids]
  TransactionChange
  (perform [_ ctx]
    (if-let [{:keys [deleted-nodes removed-arc->source+target-pkids
                     removed-overrides-by-id removed-node-id->pkid->override-node-id]}
             (ig/basis-plan-delete-nodes (:basis ctx) node-ids)]
      (ctx-perform-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids
                                removed-overrides-by-id removed-node-id->pkid->override-node-id)
      ctx))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defn- realize-delete-nodes [{:keys [active-materialization] :as ctx} undoable-changes deleted-node-ids]
  (if-let [{:keys [deleted-nodes
                   removed-arc->source+target-pkids
                   removed-node-id->pkid->override-node-id
                   removed-overrides-by-id]}
           (ig/basis-plan-delete-nodes (:basis ctx) deleted-node-ids)]
    (let [change (if-not active-materialization
                   (->DeleteNodesTXC deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id)
                   (let [node-ids (mapv gt/node-id deleted-nodes)]
                     (when (coll/any? #(ig/node-by-id-at (:post-prerequisites-basis active-materialization) %) node-ids)
                       (throw (ex-info "Materialization cannot delete existing nodes."
                                       {:node-id (:node-id active-materialization)
                                        :deleted-node-ids node-ids})))
                     (->MaterializationDeleteNodesTXC node-ids)))]
      (perform-and-conj-change ctx undoable-changes change))
    (pair ctx undoable-changes)))

(defonce/type DeleteNodesTXS [node-ids]
  TransactionStep
  (step_type [_this]
    :tx-step/delete-node)

  (metrics_key [_this]
    (if (= 1 (count node-ids))
      (nth node-ids 0)
      node-ids))

  (realize [_this ctx undoable-changes]
    (realize-delete-nodes ctx undoable-changes node-ids)))

(defn delete-node
  "*transaction step* - Delete a node from the graph."
  [node-id]
  [(->DeleteNodesTXS [node-id])])

(defn delete-nodes
  "*transaction step* - Delete nodes from the graph."
  [node-ids]
  {:pre [(coll/eager-seqable? node-ids)]}
  (if (coll/empty? node-ids)
    []
    [(->DeleteNodesTXS (vec node-ids))]))

(defonce/type OverrideTXS [root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  TransactionStep
  (step_type [_this]
    :tx-step/override)

  (metrics_key [_this]
    root-id)

  (realize [_this ctx undoable-changes]
    (realize-override ctx undoable-changes root-id traverse-fn init-props-fn init-fn properties-by-node-id)))

(defn override
  "*transaction step* - Create a series of override nodes in a graph."
  [root-id traverse-fn init-props-fn init-fn properties-by-node-id]
  [(->OverrideTXS root-id traverse-fn init-props-fn init-fn properties-by-node-id)])

(defonce/type TransferOverridesTXS [from-id->to-id]
  TransactionStep
  (step_type [_this]
    :tx-step/transfer-overrides)

  (metrics_key [_this]
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
  (step_type [_this]
    :tx-step/set-property)

  (metrics_key [_this]
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
  (step_type [_this]
    :tx-step/update-property)

  (metrics_key [_this]
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
  (step_type [_this]
    :tx-step/clear-property)

  (metrics_key [_this]
    (pair node-id property-label))

  (realize [_this ctx undoable-changes]
    (realize-clear-property ctx undoable-changes node-id property-label)))

(defn clear-property
  "*transaction step* - Clears a property on a node."
  [node-id property-label]
  [(->ClearPropertyTXS node-id property-label)])

(defonce/type UpdateGraphValueTXC
  [graph-value-key old-value new-value]

  TransactionChange
  (perform [_this ctx]
    (update ctx :basis ig/basis-perform-update-graph-value graph-value-key new-value))

  (revert [_this ctx]
    (update ctx :basis ig/basis-revert-update-graph-value graph-value-key old-value)))

(defn- realize-update-graph-value
  [{:keys [active-materialization] :as ctx} undoable-changes graph-value-key update-fn args]
  (when active-materialization
    (throw (ex-info "Materialization cannot change graph values."
                    {:node-id (:node-id active-materialization)
                     :graph-value-key graph-value-key})))
  (if-let [{:keys [graph-value-key
                   old-value
                   new-value]}
           (ig/basis-plan-update-graph-value (:basis ctx) graph-value-key update-fn args)]
    (perform-and-conj-change ctx undoable-changes (->UpdateGraphValueTXC graph-value-key old-value new-value))
    (pair ctx undoable-changes)))

(defonce/type UpdateGraphValueTXS [graph-value-key update-fn args]
  TransactionStep
  (step_type [_this]
    :tx-step/update-graph-value)

  (metrics_key [_this]
    graph-value-key)

  (realize [_this ctx undoable-changes]
    (realize-update-graph-value ctx undoable-changes graph-value-key update-fn args)))

(defn update-graph-value
  "*transaction step* - Update a graph value."
  [k fn args]
  {:pre [(ifn? fn)
         (coll/eager-seqable? args)]}
  [(->UpdateGraphValueTXS k fn args)])

(defonce/type CallbackTXS [callback-fn args opts]
  TransactionStep
  (step_type [_this]
    :tx-step/callback)

  (metrics_key [_this]
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

(defn- ctx-perform-replace-arc
  [ctx old-arc old-source-arc-pkids new-arc new-source-arc-pkids undoable]
  (ctx-replace-arc ctx
                   old-arc old-source-arc-pkids
                   new-arc new-source-arc-pkids
                   undoable))

(defn- ctx-revert-replace-arc
  [ctx old-arc old-source-arc-pkids new-arc new-source-arc-pkids undoable]
  (ctx-replace-arc ctx
                   new-arc new-source-arc-pkids
                   old-arc old-source-arc-pkids
                   undoable))

(defonce/type ReplaceArcTXC
  [old-arc old-source-arc-pkids new-arc new-source-arc-pkids undoable]

  TransactionChange
  (perform [_this ctx]
    (ctx-perform-replace-arc ctx old-arc old-source-arc-pkids new-arc new-source-arc-pkids undoable))

  (revert [_this ctx]
    (ctx-revert-replace-arc ctx old-arc old-source-arc-pkids new-arc new-source-arc-pkids undoable)))

(defn- override-node-ids-removed-by-disconnect
  "Returns override nodes affected by disconnecting arc. The context must still
  contain the arc so traversal functions can inspect the original graph state."
  [ctx arc]
  (let [basis (:basis ctx)
        node-id->node #(ig/node-by-id-at basis %)
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

(defonce/type MaterializationDisconnectArcTXC [arc]
  TransactionChange
  (perform [_ ctx]
    (if-let [{:keys [arc->source+target-pkids]} (ig/basis-plan-disconnect-arc (:basis ctx) arc)]
      (ctx-perform-disconnect-arcs ctx arc->source+target-pkids)
      ctx))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defonce/type MaterializationConnectArcTXC [arc]
  TransactionChange
  (perform [_ ctx]
    (let [basis (:basis ctx)
          target-id (gt/target-id arc)
          target-label (gt/target-label arc)
          target-node-type (some-> (ig/node-by-id-at basis target-id) gt/node-type)]
      (if-not (and target-node-type (ig/node-by-id-at basis (gt/source-id arc)))
        ctx
        (if (= :many (in/input-cardinality target-node-type target-label))
          (ctx-perform-append-arc ctx arc target-node-type)
          (let [old-arc (first (ig/explicit-arcs-by-target basis target-id target-label))]
            (if (= old-arc arc)
              ctx
              (let [{:keys [old-source-arc-pkids new-source-arc-pkids]} (ig/basis-plan-replace-arc basis old-arc arc)
                    deleted-override-node-ids (when old-arc
                                                (override-node-ids-removed-by-disconnect ctx old-arc))]
                (-> ctx
                    (ctx-perform-replace-arc old-arc old-source-arc-pkids arc new-source-arc-pkids false)
                    (perform-change (->MaterializationDeleteNodesTXC deleted-override-node-ids))))))))))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defn- realize-disconnect
  [{:keys [active-materialization] :as ctx} undoable-changes arc]
  (if-let [{:keys [arc->source+target-pkids]}
           (ig/basis-plan-disconnect-arc (:basis ctx) arc)]
    (let [undoable (some? undoable-changes)
          change (if-not active-materialization
                   (->DisconnectArcsTXC arc->source+target-pkids undoable)
                   (do
                     (when (ig/basis-plan-disconnect-arc (:post-prerequisites-basis active-materialization) arc)
                       (throw (ex-info "Materialization cannot remove an existing connection."
                                       {:node-id (:node-id active-materialization)
                                        :arc arc})))
                     (->MaterializationDisconnectArcTXC arc)))
          [_source-arc-pkids target-arc-pkids] (arc->source+target-pkids arc)
          ctx+undoable-changes (perform-and-conj-change ctx undoable-changes change)

          deleted-override-node-ids
          (when-not (coll/empty? target-arc-pkids)
            (override-node-ids-removed-by-disconnect ctx arc))]

      (if (coll/empty? deleted-override-node-ids)
        ctx+undoable-changes
        (let [[ctx undoable-changes] ctx+undoable-changes]
          (realize-delete-nodes ctx undoable-changes deleted-override-node-ids))))
    (pair ctx undoable-changes)))

(defn- realize-connect
  [{:keys [basis active-materialization] :as ctx} undoable-changes arc]
  (let [source-id (gt/source-id arc)
        source-label (gt/source-label arc)
        target-id (gt/target-id arc)
        target-label (gt/target-label arc)]
    (if-let [source (ig/node-by-id-at basis source-id)]
      (if-let [target (ig/node-by-id-at basis target-id)]
        (let [target-node-type (gt/node-type target)
              fast-path (and (nil? undoable-changes)
                             (:full-invalidation ctx))]
          (assert-type-compatible source-id source source-label target-id target target-label)
          (if active-materialization
            (perform-and-conj-change ctx undoable-changes (->MaterializationConnectArcTXC arc))
            (case (in/input-cardinality target-node-type target-label)
              :one
              (if fast-path
                ;; Regular input, bookkeeping disabled.
                (if-let [old-arc (first (ig/explicit-arcs-by-target basis target-id target-label))]
                  (if-let [{:keys [new-source-arc-pkids
                                   old-source-arc-pkids]}
                           (ig/basis-plan-replace-arc basis old-arc arc)]
                    (let [new-basis (ig/basis-perform-replace-arc
                                      basis
                                      old-arc old-source-arc-pkids
                                      arc new-source-arc-pkids)]
                      (if (identical? basis new-basis)
                        (pair ctx undoable-changes)
                        (let [deleted-override-node-ids
                              (override-node-ids-removed-by-disconnect ctx old-arc)

                              ctx (cond-> (assoc ctx :basis new-basis)
                                    (contains? (in/cascade-deletes target-node-type) target-label)
                                    (mark-override-nodes-affected target-id false))
                              ctx (if-let [{:keys [deleted-nodes
                                                   removed-arc->source+target-pkids
                                                   removed-node-id->pkid->override-node-id
                                                   removed-overrides-by-id]}
                                           (ig/basis-plan-delete-nodes (:basis ctx) deleted-override-node-ids)]
                                    (ctx-perform-delete-nodes ctx deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id)
                                    ctx)]
                          (pair ctx undoable-changes))))
                    (pair ctx undoable-changes))
                  (pair (ctx-perform-append-arc ctx arc target-node-type)
                        undoable-changes))

                ;; Regular input, bookkeeping enabled.
                (let [old-arc (first (ig/explicit-arcs-by-target basis target-id target-label))]
                  (if-let [{:keys [new-source-arc-pkids
                                   old-source-arc-pkids]}
                           (ig/basis-plan-replace-arc basis old-arc arc)]
                    (let [deleted-override-node-ids
                          (when old-arc
                            (override-node-ids-removed-by-disconnect ctx old-arc))

                          undoable (some? undoable-changes)
                          change (->ReplaceArcTXC old-arc old-source-arc-pkids arc new-source-arc-pkids undoable)
                          ctx+undoable-changes (perform-and-conj-change ctx undoable-changes change)]
                      (if (coll/empty? deleted-override-node-ids)
                        ctx+undoable-changes
                        (let [[ctx undoable-changes] ctx+undoable-changes]
                          (realize-delete-nodes ctx undoable-changes deleted-override-node-ids))))
                    (pair ctx undoable-changes))))

              :many
              (if fast-path
                ;; Array input, bookkeeping disabled.
                (pair (ctx-perform-append-arc ctx arc target-node-type)
                      undoable-changes)

                ;; Array input, bookkeeping enabled.
                (if-let [{:keys [arc->source+target-pkids]}
                         (ig/basis-plan-connect-arc basis arc)]
                  (let [undoable (some? undoable-changes)
                        change (->ConnectArcsTXC arc->source+target-pkids undoable)]
                    (perform-and-conj-change ctx undoable-changes change))
                  (pair ctx undoable-changes))))))
        (pair ctx undoable-changes))
      (pair ctx undoable-changes))))

(defonce/type ConnectTXS [arc]
  TransactionStep
  (step_type [_this]
    :tx-step/connect)

  (metrics_key [_this]
    arc)

  (realize [_this ctx undoable-changes]
    (realize-connect ctx undoable-changes arc)))

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and
  label and a target node and label."
  [source-id source-label target-id target-label]
  [(->ConnectTXS (gt/->Arc source-id source-label target-id target-label))])

(defn- realize-expand-impl
  [ctx undoable-changes tx-steps-fn args opts]
  (if-not (:inject-evaluation-context opts)
    (realize-tx ctx undoable-changes (apply tx-steps-fn args))
    (let [evaluation-context (make-evaluation-context ctx)
          tx-data (apply tx-steps-fn evaluation-context args)
          ctx (apply-evaluation-context-changes! ctx evaluation-context)]
      (realize-tx-impl ctx undoable-changes tx-data evaluation-context))))

(defn- realize-expand
  [ctx undoable-changes tx-steps-fn args {:keys [catch-fn] :as opts}]
  (if-not catch-fn
    (realize-expand-impl ctx undoable-changes tx-steps-fn args opts)

    ;; Isolate the immutable transaction context and the transient undo buffer.
    ;; A failed attempt must not leave changes for later undo or replay.
    (let [tx-data-context-atom (:tx-data-context-atom ctx)
          previous-tx-data-context @tx-data-context-atom

          [ctx attempted-undoable-changes]
          (try
            (realize-expand-impl ctx (when undoable-changes (transient []))
                                 tx-steps-fn args opts)
            (catch Exception exception
              (reset! tx-data-context-atom previous-tx-data-context)
              (realize-tx ctx (when undoable-changes (transient []))
                          (catch-fn exception))))]

      (pair ctx
            (when undoable-changes
              (reduce conj!
                      undoable-changes
                      (persistent! attempted-undoable-changes)))))))

(defonce/type ExpandTXS [tx-steps-fn args opts]
  TransactionStep
  (step_type [_this]
    :tx-step/expand)

  (metrics_key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (realize-expand ctx undoable-changes tx-steps-fn args opts)))

(defn expand
  "*transaction step* - Call a function and execute the returned transaction
  steps within the transaction. With :inject-evaluation-context in opts, pass
  an evaluation context as the first argument to tx-steps-fn. With :catch-fn,
  an Exception from generating or applying steps discards the expansion's
  graph changes, undo records and changes to :tx-data-context-atom before
  calling catch-fn with the exception and applying its returned steps.
  Callback side effects outside the graph are not rolled back."
  [tx-steps-fn args opts]
  {:pre [(ifn? tx-steps-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))
         (or (nil? (:catch-fn opts)) (ifn? (:catch-fn opts)))
         (or (nil? (:inject-evaluation-context opts)) (boolean? (:inject-evaluation-context opts)))]}
  [(->ExpandTXS tx-steps-fn args opts)])

(defonce/type DisconnectTXS [arc]
  TransactionStep
  (step_type [_this]
    :tx-step/disconnect)

  (metrics_key [_this]
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
  (for [arc (ig/explicit-arcs-by-target basis target-id target-label)]
    (disconnect (gt/source-id arc) (gt/source-label arc) target-id target-label)))

(defonce/type LabelTXS [label]
  TransactionStep
  (step_type [_this]
    :tx-step/label)

  (metrics_key [_this]
    nil)

  (realize [_this ctx undoable-changes]
    (pair (assoc ctx :label label)
          undoable-changes)))

(defn label
  [label]
  [(->LabelTXS label)])

(defonce/type SequenceLabelTXS [sequence-label]
  TransactionStep
  (step_type [_this]
    :tx-step/sequence-label)

  (metrics_key [_this]
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
  (step_type [_this]
    :tx-step/invalidate)

  (metrics_key [_this]
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
  (step_type [_this]
    :tx-step/invalidate-output)

  (metrics_key [_this]
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

(defn tx-step-type
  [^TransactionStep tx-step]
  (.step_type tx-step))

(defn tx-step-added-nodes
  [^AddNodesTXS tx-step]
  (when (instance? AddNodesTXS tx-step)
    (.-added-nodes tx-step)))

(defn tx-step-added-arc
  ^Arc [^ConnectTXS tx-step]
  (when (instance? ConnectTXS tx-step)
    (.-arc tx-step)))

(def tx-report-keys
  (cond-> [:basis :nodes-added :nodes-deleted :outputs-modified :label :sequence-label :undoable-changes :realized-changes :materialization-results]
    (du/metrics-enabled?) (conj :metrics)))

(defn finalize-update
  [{:keys [tx-data-context-atom] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (zero? (long (:completed-action-count ctx))) :empty :ok)
             :tx-data-context (deref tx-data-context-atom))))

(defn new-transaction-context
  [basis node-id-generator override-id-generator tx-data-context metrics-collector full-invalidation shared-materialization-registry-atom materialization-results]
  {:pre [(map? tx-data-context)]}
  {:basis basis
   :initial-basis basis
   :shared-materialization-registry-atom shared-materialization-registry-atom
   :materialization-results materialization-results
   :nodes-affected #{}
   :nodes-added []
   :nodes-deleted {}
   :outputs-modified #{}
   :override-nodes-affected-undoability {}
   :override-nodes-affected-ordered []
   :successor-arcs-changed #{}
   :successor-node-ids-changed #{}
   :successors-changed {}
   :node-id-generator node-id-generator
   :override-id-generator override-id-generator
   :completed-action-count 0
   :txid (new-txid)
   :tx-data-context-atom (atom tx-data-context)
   :metrics metrics-collector
   :full-invalidation full-invalidation})

(defn- finalize-successors-changed
  [{:keys [basis initial-basis successor-arcs-changed successor-node-ids-changed] :as ctx}]
  (du/measuring (:metrics ctx) :finalize-successors-changed
    (cond-> ctx
      (not (:full-invalidation ctx))
      (flag-successors-changed
        (successor-changes
          initial-basis
          basis
          successor-node-ids-changed
          successor-arcs-changed)))))

(defn update-successors
  [{:keys [^long completed-action-count successors-changed] :as ctx}]
  (du/measuring (:metrics ctx) :update-successors
    (cond
      (zero? completed-action-count)
      ctx

      (:full-invalidation ctx)
      (update ctx :basis ig/invalidate-all-successors)

      (coll/empty? successors-changed)
      ctx

      :else
      (update ctx :basis ig/update-successors successors-changed))))

(defn trace-dependencies
  [ctx]
  ;; At this point, :nodes-affected is a set of all output Endpoints that have
  ;; been directly affected by the transaction changes.
  ;; We now follow these outputs recursively to obtain a sequence of all the
  ;; outputs that depend on them in the entire graph, stopping at fresh nodes.
  ;; Node and connection changes have already marked their existing consumers.
  (du/measuring (:metrics ctx) :trace-dependencies
    (let [outputs-modified (ig/invalidation-dependencies (:initial-basis ctx) (:basis ctx) (:nodes-affected ctx))]
      (assoc ctx :outputs-modified outputs-modified))))

(defn finalize-applied-changes
  [ctx]
  (-> ctx
      finalize-successors-changed
      update-successors
      trace-dependencies
      finalize-update))

(defn finalize-replayed-changes [ctx]
  ;; Replacing a connection can change the substructure of existing overrides
  ;; in the destination, independently of the recorded materialization.
  (let [[ctx] (realize-update-overrides ctx nil)]
    (finalize-applied-changes ctx)))

(defn transact*
  [{:keys [active-materialization] :as ctx} undoable-changes tx-data]
  (when (and active-materialization
             (not= :realizing-tx-data (:phase active-materialization)))
    (throw (ex-info "Materialization can only execute transactions during the :realizing-tx-data phase."
                    {:node-id (:node-id active-materialization)
                     :phase (:phase active-materialization)})))
  (when *tx-debug*
    (println (txerrstr ctx "tx-data" tx-data)))
  (let [[ctx undoable-changes] (realize-tx ctx undoable-changes tx-data)
        [ctx undoable-changes] (realize-update-overrides ctx undoable-changes)
        undoable-changes (if undoable-changes (persistent! undoable-changes) [])]
    (-> ctx
        (assoc :undoable-changes undoable-changes)
        (finalize-applied-changes))))

(defonce/type MaterializeShellTXC [node-id]
  TransactionChange
  (perform [_ ctx]
    (update ctx :basis ig/transform-node node-id
            (fn [node]
              (coll/merge (in/->NodeImpl node-id (gt/node-type node))
                          (gt/assigned-properties node (:basis ctx))))))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defonce/type MaterializeShellTXS [node-id]
  TransactionStep
  (step_type [_] :tx-step/materialize-shell)
  (metrics_key [_] node-id)
  (realize [_ ctx undoable-changes]
    (let [node (ig/node-by-id-at (:basis ctx) node-id)]
      (if-not (in/shell-node? node)
        (pair ctx undoable-changes)
        (let [[ctx] (perform-and-conj-change ctx nil (->MaterializeShellTXC node-id))
              [ctx] (realize-defaults ctx nil (ig/node-by-id-at (:basis ctx) node-id))]
          (pair ctx undoable-changes))))))

(defn materialize-shell [node-id]
  [(->MaterializeShellTXS node-id)])

(defn- capture-prerequisite-source-state [basis materialization-results prerequisite-node-ids]
  ;; Follow owned substructure and override originals, never reverse override
  ;; registrations or outgoing connections to consumers. These can grow when
  ;; independent materializations use the same prerequisite.
  (let [prerequisite-node-ids
        (ig/pre-traverse
          basis prerequisite-node-ids
          (fn prerequisite-dependencies [_basis node-id]
            (:prerequisite-shell-node-ids (get materialization-results node-id))))

        source-node-ids
        (ig/pre-traverse
          basis prerequisite-node-ids
          (fn prerequisite-sources [basis node-id]
            (when-let [node (ig/node-by-id-at basis node-id)]
              (into (if-let [original (gt/original node)] [original] [])
                    (comp (mapcat #(ig/arcs-by-target basis node-id %))
                          (map gt/source-id))
                    (in/cascade-deletes (gt/node-type node))))))]

    (coll/into-> source-node-ids {}
      (map (fn [node-id]
             (pair node-id
                   (pair (ig/node-by-id-at basis node-id)
                         (vec (ig/explicit-arcs-by-target basis node-id)))))))))

(defn stale-materialization? [error]
  (= ::stale-materialization (:ex-type (ex-data error))))

(defn- perform-materialization [ctx {:keys [node-id shell-node changes dependencies prerequisite-shell-node-ids prerequisite-source-state] :as materialize-info}]
  (if (and (identical? materialize-info (get (:materialization-results ctx) node-id))
           (not (in/shell-node? (ig/node-by-id-at (:basis ctx) node-id))))
    ctx ; The materialization has already happened.
    (do
      (when-not (identical? shell-node (ig/node-by-id-at (:basis ctx) node-id))
        (throw (ex-info "The shell was replaced before its materialization could be applied."
                        {:ex-type ::stale-materialization
                         :node-id node-id})))
      (let [ctx (coll/reduce-kv-> dependencies ctx
                  (fn [ctx _node-id dependency]
                    (perform-materialization ctx dependency)))]
        (when-not (= prerequisite-source-state
                     (capture-prerequisite-source-state (:basis ctx) (:materialization-results ctx) prerequisite-shell-node-ids))
          (throw (ex-info "The prerequisites changed before the materialization could be applied."
                          {:ex-type ::stale-materialization
                           :node-id node-id})))
        (assoc-in (reduce perform-change ctx changes)
                  [:materialization-results node-id] materialize-info)))))

(defonce/type MaterializationTXC [materialize-info]
  TransactionChange
  (perform [_ ctx]
    (perform-materialization ctx materialize-info))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Materializations are not undoable."))))

(defonce/type InitializeShellPropertiesTXC [shell-node initialized-shell-node property-labels]
  TransactionChange
  (perform [_ ctx]
    (let [node-id (gt/node-id shell-node)
          current-node (ig/node-by-id-at (:basis ctx) node-id)]
      (if (identical? initialized-shell-node current-node)
        ctx
        (do
          (when-not (identical? shell-node current-node)
            (throw (ex-info "The shell was replaced before its properties could be initialized."
                            {:ex-type ::stale-materialization
                             :node-id node-id})))
          (reduce #(mark-property-state-changes %1 node-id %2)
                  (update ctx :basis ig/transform-node node-id (constantly initialized-shell-node))
                  property-labels)))))
  (revert [_ _ctx]
    (throw (UnsupportedOperationException. "Shell property initialization is not undoable."))))

(defn evaluation-context-change? [change]
  (or (instance? MaterializationTXC change)
      (instance? InitializeShellPropertiesTXC change)))

(defn- apply-evaluation-context-changes! [ctx evaluation-context]
  (let [changes-atom (:changes-atom evaluation-context)
        transaction-changes @changes-atom]
    (if (coll/empty? transaction-changes)
      ctx
      (let [ctx (coll/reduce-> transaction-changes ctx
                  (fn [ctx change]
                    (assert (evaluation-context-change? change)
                            "Evaluation contexts can only accumulate materializations and shell property initializations.")
                    (-> ctx
                        (perform-change change)
                        (update :completed-action-count inc)
                        (cond->
                          (:realized-changes ctx)
                          (update :realized-changes conj change)))))]
        (reset! changes-atom [])
        ctx))))

(defn- evaluation-transaction-context [{:keys [basis-ref materialization-results-atom] :as evaluation-context}]
  (-> (new-transaction-context
        @basis-ref
        (:node-id-generator evaluation-context)
        (:override-id-generator evaluation-context)
        {} nil false
        (:shared-materialization-registry-atom evaluation-context)
        @materialization-results-atom)
      (assoc :active-materialization (:active-materialization evaluation-context)
             :tx-data-context-atom (:tx-data-context-atom evaluation-context)
             :initial-invalidate-counters (:initial-invalidate-counters evaluation-context)
             :realized-changes [])))

(defn- apply-evaluation-result! [evaluation-context change result]
  (let [{:keys [basis-ref changes-atom invalidated-endpoints-atom materialization-results-atom nodes-added-atom nodes-deleted-atom]} evaluation-context
        {:keys [basis outputs-modified materialization-results nodes-added nodes-deleted]} result]
    (reset! basis-ref basis)
    (reset! materialization-results-atom materialization-results)
    (swap! changes-atom conj change)
    (swap! nodes-added-atom
           (fn [previous-nodes-added]
             (into (cond-> previous-nodes-added
                     (coll/not-empty nodes-deleted)
                     (coll/transform-> (remove nodes-deleted)))
                   nodes-added)))
    (swap! nodes-deleted-atom into nodes-deleted)
    (swap! invalidated-endpoints-atom into outputs-modified)
    (when-let [cache-atom (:cache-atom evaluation-context)]
      (swap! cache-atom c/cache-invalidate outputs-modified))
    (swap! (:local-atom evaluation-context) #(reduce dissoc % outputs-modified))
    (when-let [local-temp-atom (:local-temp-atom evaluation-context)]
      (swap! local-temp-atom #(reduce dissoc % outputs-modified)))
    nil))

(defn initialize-shell-properties! [node-id properties {:keys [basis-ref active-materialization] :as evaluation-context}]
  {:pre [(map? properties)
         (nil? active-materialization)]}
  (assert @basis-ref "Evaluation context is closed.")
  (let [node (ig/node-by-id-at @basis-ref node-id)]
    (when (and (in/shell-node? node)
               (not (coll/empty? properties)))
      (let [node-type (gt/node-type node)
            property-labels (into [] (coll/keys properties))]
        (in/validate-property-labels node-type properties)
        (in/validate-property-values node-type node-id properties)
        (doseq [property-label property-labels]
          (assert (not (contains? (gt/assigned-properties node @basis-ref) property-label))
                  (str "Shell property is already initialized: " property-label))
          (assert (contains? (:flags (get (in/declared-outputs node-type) property-label)) :unjammable)
                  (str "Shell property must be unjammable: " property-label))
          (assert (nil? (in/property-setter node-type property-label))
                  (str "Shell property must not have a setter: " property-label)))
        (let [change (->InitializeShellPropertiesTXC node (coll/merge node properties) property-labels)
              result (-> (evaluation-transaction-context evaluation-context)
                         (perform-change change)
                         (assoc :completed-action-count 1)
                         (finalize-applied-changes))]
          (apply-evaluation-result! evaluation-context change result))))))

(defn- prerequisite-path [shared-materialization-registry start-id target-id]
  (loop [pending [[start-id]]
         visited #{}]
    (when-let [path (peek pending)]
      (let [node-id (peek path)
            pending (pop pending)]
        (cond
          (= node-id target-id)
          path

          (contains? visited node-id)
          (recur pending visited)

          :else
          (let [{:keys [promise prerequisite-shell-node-ids]} (shared-materialization-registry node-id)]
            (recur (cond-> pending
                     (and promise (not (realized? promise)))
                     (into (map #(conj path %)) prerequisite-shell-node-ids))
                   (conj visited node-id))))))))

(defn materialize-prerequisites!
  "Registers all direct prerequisites before materializing any of them. Only
  call from a shell's materialize-fn, before returning its transaction steps."
  [node-id prerequisite-node-ids {:keys [active-materialization basis-ref] :as evaluation-context}]
  {:pre [(= node-id (:node-id active-materialization))
         (= :producing-tx-data (:phase active-materialization))]}
  (let [shared-materialization-registry-atom (:shared-materialization-registry-atom evaluation-context)
        prerequisite-node-ids (set prerequisite-node-ids)
        entry (get @shared-materialization-registry-atom node-id)]
    (try
      (when-not (identical? (:shell-node entry)
                            (ig/node-by-id-at @basis-ref node-id))
        (throw (ex-info "The shell was replaced while reading its prerequisites."
                        {:node-id node-id})))
      ;; Check and registration must be one atomic update to detect cross-thread
      ;; cycles. Reading resources, materializing nodes, and waiting happen
      ;; outside retries.
      (swap! shared-materialization-registry-atom
             (fn [shared-materialization-registry]
               (when-not (identical? (:promise entry) (get-in shared-materialization-registry [node-id :promise]))
                 (throw (ex-info "The shell was replaced while reading its prerequisites."
                                 {:node-id node-id})))
               (if-let [path (coll/some #(prerequisite-path shared-materialization-registry % node-id) prerequisite-node-ids)]
                 (throw (ex-info "Circular resource prerequisites."
                                 {:ex-type :prerequisite-cycle
                                  :cycle-node-ids (into [node-id] path)}))
                 (assoc-in shared-materialization-registry [node-id :prerequisite-shell-node-ids] prerequisite-node-ids))))
      (vreset! (:prerequisite-node-ids active-materialization) prerequisite-node-ids)
      (doseq [prerequisite-id prerequisite-node-ids]
        (materialize-node! prerequisite-id (assoc-in evaluation-context [:active-materialization :phase] :materializing-prerequisites))
        (let [node (ig/node-by-id-at @basis-ref prerequisite-id)]
          (when-let [errors (coll/not-empty (gt/get-property node @basis-ref :_output-jammers))]
            (throw (ex-info "A resource prerequisite could not be materialized."
                            {:ex-type :prerequisite-failed
                             :node-id node-id
                             :prerequisite-node-id prerequisite-id
                             :errors errors})))))
      (catch Throwable error
        ;; A defective result must be replayable without following the cycle
        ;; or requiring the failed prerequisite. Completed prerequisites remain.
        (vreset! (:prerequisite-node-ids active-materialization) #{})
        (swap! shared-materialization-registry-atom
               (fn [shared-materialization-registry]
                 (if-not (identical? (:promise entry) (get-in shared-materialization-registry [node-id :promise]))
                   shared-materialization-registry
                   (assoc-in shared-materialization-registry [node-id :prerequisite-shell-node-ids] #{}))))
        (throw error)))))

(defn- find-materialization-entry [entry node rejected-promises]
  ;; Keep older versions for evaluation-contexts with older source snapshots.
  ;; Rejecting a result in one evaluation-context does not invalidate it for
  ;; other evaluation-contexts.
  (loop [entry entry]
    (when entry
      (if (and (identical? node (:shell-node entry))
               (not (contains? rejected-promises (:promise entry))))
        entry
        (recur (:previous-entry entry))))))

(defn- claim-materialization! [shared-materialization-registry-atom node rejected-promises]
  (let [node-id (gt/node-id node)
        candidate {:shell-node node
                   :promise (promise)
                   :prerequisite-shell-node-ids #{}}
        shared-materialization-registry
        (swap! shared-materialization-registry-atom
               (fn [shared-materialization-registry]
                 (if (find-materialization-entry (get shared-materialization-registry node-id) node rejected-promises)
                   shared-materialization-registry
                   (assoc shared-materialization-registry node-id
                          (assoc candidate :previous-entry (get shared-materialization-registry node-id))))))
        entry (find-materialization-entry (get shared-materialization-registry node-id) node rejected-promises)]
    (pair entry (identical? (:promise candidate) (:promise entry)))))

(defn- realize-materialization! [node evaluation-context]
  (let [node-id (gt/node-id node)
        {:keys [basis-ref materialization-results-atom]} evaluation-context
        ;; Each owner gets its own state, scoped to the evaluation-contexts and
        ;; transaction-contexts used for its materialization. Only explicitly
        ;; declared prerequisites may start another materialization.
        active-materialization {:node-id node-id
                                :phase :producing-tx-data
                                :prerequisite-node-ids (volatile! #{})}
        evaluation-context (assoc evaluation-context :active-materialization active-materialization)
        ;; The callback completes declared prerequisites before returning steps.
        ;; Snapshot only afterwards: failure must not retract their results.
        tx-data ((:_materialize-fn node) node-id evaluation-context)
        ;; Keep our declaration even if another shell replaces the registry
        ;; entry before this materialization finishes.
        prerequisite-shell-node-ids @(:prerequisite-node-ids active-materialization)
        evaluation-context-atoms (into []
                                       (keep evaluation-context)
                                       [:basis-ref
                                        :cache-atom
                                        :changes-atom
                                        :hits-atom
                                        :invalidated-endpoints-atom
                                        :local-atom
                                        :local-temp-atom
                                        :materialization-results-atom
                                        :nodes-added-atom
                                        :nodes-deleted-atom
                                        :tx-data-context-atom])
        previous-evaluation-context-atom-values (mapv deref evaluation-context-atoms)
        previous-basis @basis-ref
        previous-materialization-results @materialization-results-atom]
    (try
      (let [result (transact*
                     (evaluation-transaction-context
                       (update evaluation-context :active-materialization assoc
                               :phase :realizing-tx-data
                               :post-prerequisites-basis previous-basis))
                     nil
                     [(materialize-shell node-id) tx-data])
            materialize-info {:node-id node-id
                              :shell-node node
                              :changes (:realized-changes result)
                              :prerequisite-shell-node-ids prerequisite-shell-node-ids
                              :prerequisite-source-state (capture-prerequisite-source-state previous-basis previous-materialization-results prerequisite-shell-node-ids)
                              :dependencies (select-keys previous-materialization-results prerequisite-shell-node-ids)}]
        (apply-evaluation-result!
          evaluation-context
          (->MaterializationTXC materialize-info)
          (assoc-in result [:materialization-results node-id] materialize-info))
        materialize-info)
      (catch Throwable error
        (coll/reduce-kv-> evaluation-context-atoms nil
          (fn [_ index evaluation-context-atom]
            (reset! evaluation-context-atom (previous-evaluation-context-atom-values index))))
        (throw error)))))

(defn materialize-node! [node-id {:keys [active-materialization] :as evaluation-context}]
  (assert @(:basis-ref evaluation-context) "Evaluation context is closed.")
  ;; Dependency tracing skips output functions and caches placeholder values.
  ;; Materialization needs actual values even when triggered by such a
  ;; traversal.
  (let [evaluation-context (cond-> evaluation-context
                             (:dry-run evaluation-context)
                             (assoc :dry-run false
                                    :tracer nil
                                    :in-production #{}
                                    :local-atom (atom {})
                                    :local-temp-atom (atom {})
                                    :hits-atom (atom #{})))
        basis-ref (:basis-ref evaluation-context)
        shared-materialization-registry-atom (:shared-materialization-registry-atom evaluation-context)
        node (ig/node-by-id-at @basis-ref node-id)]
    (when (in/shell-node? node)
      (when (and active-materialization
                 (not= :materializing-prerequisites (:phase active-materialization)))
        (throw (ex-info "Undeclared nested materialization. Register the resource as a prerequisite."
                        {:ex-type :undeclared-materialization
                         :node-id (:node-id active-materialization)
                         :prerequisite-node-id node-id})))
      (loop [rejected-promises #{}]
        (let [[entry owner] (claim-materialization! shared-materialization-registry-atom node rejected-promises)
              result-promise (:promise entry)]
          (if owner
            ;; Our thread was assigned responsibility to materialize the node.
            ;; Materialize it into our evaluation-context, then deliver the
            ;; resulting TransactionChanges to any other threads that have open
            ;; evaluation-contexts that need it materialized.
            (try
              (log-node-materialization! 4 node "Materialization claimed.")
              (let [materialization-start (System/nanoTime)
                    materialize-info (realize-materialization! node evaluation-context)
                    materialization-end (System/nanoTime)]
                (log-node-materialization! 2 node "Materialization ready." :elapsed (du/nanos->string (- materialization-end materialization-start)))
                (deliver result-promise {:materialize-info materialize-info}))
              (catch Throwable error
                ;; Settle this promise even if a newer shell replaced its
                ;; registry entry. Future calls may retry.
                (log-node-materialization! 1 node "Materialization failed." :exception error)
                (deliver result-promise {:error error})
                (swap! shared-materialization-registry-atom
                       (fn [shared-materialization-registry]
                         (if-not (identical? result-promise (get-in shared-materialization-registry [node-id :promise]))
                           shared-materialization-registry
                           (if-let [previous-entry (:previous-entry entry)]
                             (assoc shared-materialization-registry node-id previous-entry)
                             (dissoc shared-materialization-registry node-id)))))
                (throw error)))

            ;; Another thread was assigned responsibility to materialize the
            ;; node. We await the resulting TransactionChanges.
            (let [_ (log-node-materialization! 4 node "Materialization awaited.")
                  {:keys [error materialize-info]} @result-promise]
              (log-node-materialization! 4 node "Materialization received.")
              (if error
                (throw error)
                (let [change (->MaterializationTXC materialize-info)
                      result (try
                               (-> (evaluation-transaction-context evaluation-context)
                                   (perform-change change)
                                   (assoc :completed-action-count 1)
                                   (finalize-replayed-changes))
                               (catch ExceptionInfo error
                                 (when-not (stale-materialization? error)
                                   (throw error))))]
                  (if result
                    (apply-evaluation-result! evaluation-context change result)
                    (recur (conj rejected-promises result-promise))))))))))))

(extend-type ShellNode
  gt/Materializable
  (materialize! [this {:keys [basis-ref] :as evaluation-context}]
    (let [node-id (gt/node-id this)]
      (materialize-node! node-id evaluation-context)
      (ig/node-by-id-at @basis-ref node-id))))

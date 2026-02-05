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
            [internal.system :as is]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [internal.graph.types Arc]
           [java.util.concurrent.atomic AtomicInteger]))

(set! *warn-on-reflection* true)

(defonce/interface TransactionStep
  (step_type []) ; Returns a keyword uniquely identifying the type of transaction step.
  (metrics_key []) ; Returns a key which identifies the subject of the transaction step in metrics reports.
  (perform [ctx])) ; Returns a new ctx with changes applied.

;; ---------------------------------------------------------------------------
;; Internal state
;; ---------------------------------------------------------------------------
(def ^:dynamic *tx-debug* nil)

(def ^:private ^AtomicInteger next-txid (AtomicInteger. 1))
(defn- new-txid [] (.getAndIncrement next-txid))

(defmacro txerrstr [ctx & rest]
  `(str (:txid ~ctx) ": " ~@(interpose " " rest)))

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------
(defn- mark-input-activated
  [ctx node-id input-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if-not (:track-changes ctx)
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
  (if-not (:track-changes ctx)
    ctx
    (let [nodes-affected (:nodes-affected ctx)]
      (assoc ctx
        :nodes-affected
        (conj nodes-affected (gt/endpoint node-id output-label))))))

(defn- mark-outputs-activated
  [ctx node-id output-labels]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (if-not (:track-changes ctx)
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
  (if-not (:track-changes ctx)
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

(defn- next-node-id [ctx graph-id]
  (is/next-node-id* (:node-id-generators ctx) graph-id))

(defn- next-override-id [ctx graph-id]
  (is/next-override-id* (:override-id-generator ctx) graph-id))

(declare ^:private ctx-disconnect)

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
  (let [basis (:basis ctx)]
    (if-let [node (gt/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (let [targets (ig/explicit-targets basis node-id)]
        (-> (reduce (fn [ctx [node-id input]]
                      (mark-input-activated ctx node-id input))
                    ctx
                    targets)
            (disconnect-all-inputs node-id)
            (mark-all-outputs-activated node-id)
            (update :basis gt/delete-node node-id)
            (assoc-in [:nodes-deleted node-id] node)
            (update :nodes-added (partial filterv #(not= node-id %)))))
      ctx)))

(defn- ctx-delete-node [ctx node-id]
  (when *tx-debug*
    (println (txerrstr ctx "deleting " node-id)))
  (let [to-delete (ig/pre-traverse (:basis ctx) [node-id] ig/cascade-delete-sources)]
    (when (and *tx-debug* (not (empty? to-delete)))
      (println (txerrstr ctx "cascading delete of " (pr-str to-delete))))
    (reduce delete-single ctx to-delete)))

(defn- ctx-new-override
  [ctx override-id root-id traverse-fn init-props-fn]
  (let [override (ig/make-override root-id traverse-fn init-props-fn)]
    (update ctx :basis gt/add-override override-id override)))

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

(defn- ctx-override-node [ctx original-node-id override-node-id]
  (assert (= (gt/node-id->graph-id original-node-id) (gt/node-id->graph-id override-node-id))
          "Override nodes must belong to the same graph as the original")
  (let [basis (:basis ctx)
        all-originals (ig/override-originals basis original-node-id)]
    (-> ctx
        (assoc :basis (gt/override-node basis original-node-id override-node-id))

        ;; Any property, input or output on any original nodes must now take the
        ;; new override node into account.
        (flag-all-successors-changed all-originals)

        ;; Similarly, so must the source outputs of any arcs that target any of
        ;; the original nodes.
        (flag-successors-changed (e/mapcat #(gt/sources basis %) all-originals)))))

(defonce/type NewOverrideTXS [override-id root-id traverse-fn init-props-fn]
  TransactionStep
  (step-type [_this]
    :tx-step/new-override)

  (metrics-key [_this]
    root-id)

  (perform [_this ctx]
    (ctx-new-override ctx override-id root-id traverse-fn init-props-fn)))

(defn- new-override
  [override-id root-id traverse-fn init-props-fn]
  [(->NewOverrideTXS override-id root-id traverse-fn init-props-fn)])

(defonce/type OverrideNodeTXS [original-node-id override-node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/override-node)

  (metrics-key [_this]
    original-node-id)

  (perform [_this ctx]
    (ctx-override-node ctx original-node-id override-node-id)))

(defn- override-node
  [original-node-id override-node-id]
  [(->OverrideNodeTXS original-node-id override-node-id)])

(declare apply-tx new-node)

(defn- ctx-override
  [ctx root-id traverse-fn init-props-fn init-fn properties-by-node-id]
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
        new-override-nodes-tx-data (map new-node override-nodes)
        new-override-tx-data (concat
                               (new-override override-id root-id traverse-fn init-props-fn)
                               (map
                                 (fn [node-id override-node-id]
                                   (override-node node-id override-node-id))
                                 node-ids
                                 override-node-ids))]
    (as-> ctx ctx'
          (apply-tx ctx' (concat new-override-nodes-tx-data
                                 new-override-tx-data))
          (apply-tx ctx' (init-fn (in/custom-evaluation-context {:basis (:basis ctx') :tx-data-context (:tx-data-context ctx')})
                                  original-node-id->override-node-id)))))

(defn- node-id->override-id [basis node-id]
  (->> node-id
       (gt/node-by-id-at basis)
       gt/override-id))

(declare ctx-add-node)

(defn- ctx-make-override-nodes [ctx override-id node-ids init-props-fn]
  (reduce (fn [ctx node-id]
            (let [basis (:basis ctx)]
              (if (coll/some #(= override-id (node-id->override-id basis %))
                             (ig/get-overrides basis node-id))
                ctx
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
                  (-> ctx
                      (ctx-add-node new-override-node)
                      (ctx-override-node node-id new-override-node-id))))))
          ctx
          node-ids))

(defn- populate-overrides [ctx node-id]
  ;; When a transaction concludes, this gets called for each node-id where a
  ;; :cascade-delete input gained a new connection. We must now create new
  ;; override nodes for the relevant nodes in the connected subgraph. Each
  ;; override layer will get a chance to spawn its own set of override nodes,
  ;; based on its traverse-fn.
  ;;
  ;; It is possible for an override layer to be created for a root-node and
  ;; connections to have been introduced later in the same transaction. In that
  ;; case, the :override transaction step will have already traversed and
  ;; created override nodes from the :cascade-delete subgraph that was connected
  ;; to the root-node at the time.
  (let [basis (:basis ctx)
        override-node-ids (ig/get-overrides basis node-id)
        override-node-count (count override-node-ids)
        ctx (loop [override-node-index 0
                   ctx ctx
                   prev-traverse-fn nil
                   prev-traverse-result nil]
              (if (>= override-node-index override-node-count)
                ctx
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
                                   (ig/pre-traverse basis source-node-ids traverse-fn)))]
                  (recur (inc override-node-index)
                         (ctx-make-override-nodes ctx override-id node-ids init-props-fn)
                         traverse-fn
                         node-ids))))]
    (reduce populate-overrides
            ctx
            override-node-ids)))

(defn- ctx-transfer-overrides
  [ctx from-id->to-id]
  ;; This method updates the existing override layer to use the to-id as the
  ;; root of the override layer. It also updates the "first level" (i.e. direct)
  ;; override nodes that have from-id as their original to instead have to-id as
  ;; their original. It then deletes every other override node that was produced
  ;; from the existing override layer and re-runs the populate-overrides
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
                                                         (gt/override-id (gt/node-by-id-at basis override-node-id)))))
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
                              override-node-ids)]
    (-> ctx
        (update :basis (fn [basis]
                         ;; Clear out the original to override node-id mappings
                         ;; from the graph. Normally entries are removed from
                         ;; this mapping inside basis-remove-node as nodes are
                         ;; deleted, but since we're transferring overrides, we
                         ;; must do it manually here.
                         (reduce (fn [basis from-node-id]
                                   (gt/override-node-clear basis from-node-id))
                                 basis
                                 (keys from-id->to-id)))) ; from nodes no longer have any override nodes
        (update :basis (fn [basis]
                         (reduce (fn [basis [override-id override]]
                                   (gt/replace-override basis override-id (update override :root-id from-id->to-id)))
                                 basis
                                 overrides-to-fix))) ; re-root overrides that used to have a from node id as root
        (as-> ctx
              (reduce ctx-delete-node ctx nodes-to-delete))

        ;; * repoint the first level override nodes to use to-node as original
        ;; * add as override nodes of to-node
        (as-> ctx
              (reduce (fn [ctx override-node-id]
                        (let [basis (:basis ctx)
                              override-node (gt/node-by-id-at basis override-node-id)
                              old-original (gt/original override-node)
                              new-original (from-id->to-id old-original)
                              new-basis (gt/replace-node basis override-node-id (gt/set-original override-node new-original))]
                          (-> ctx
                              (assoc :basis new-basis)
                              (mark-all-outputs-activated override-node-id)
                              (ctx-override-node new-original override-node-id))))
                      ctx
                      override-node-ids))
        (as-> ctx
              (reduce populate-overrides
                      ctx
                      (vals from-id->to-id))))))

(defn- property-default-setter
  [basis node-id node property new-value]
  (gt/replace-node basis node-id (gt/set-property node basis property new-value)))

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

(defn- validate-property-value-impl [node-type node-id property-label property-value]
  (let [value-type (some-> (in/property-type node-type property-label) deref in/schema s/maybe)
        node-type-name (in/type-name node-type)]
    (when-let [validation-error (some-> value-type (s/check property-value))]
      (in/warn-property-schema node-id property-label node-type-name property-value value-type validation-error)
      (throw (ex-info "SCHEMA-VALIDATION"
                      {:node-id node-id
                       :type node-type-name
                       :property property-label
                       :expected value-type
                       :actual property-value
                       :validation-error validation-error})))))

(defmacro ^:private validate-property-value [node-type node-id property-label property-value]
  (when in/*check-schemas*
    `(when ~`in/*check-schemas* ; Inner check to support disabling the schema check post compile-time.
       (validate-property-value-impl ~node-type ~node-id ~property-label ~property-value))))

(defn- invoke-setter
  [ctx node-id node property old-value new-value override-node? dynamic?]
  (let [node-type (gt/node-type node)
        setter-fn (in/property-setter node-type property)]
    (validate-property-value node-type node-id property new-value)
    (-> ctx
        (update :basis property-default-setter node-id node property new-value)
        (cond->
          (not= old-value new-value)
          (cond->
            (not override-node?) (mark-output-activated node-id property)
            override-node? (mark-outputs-activated node-id (cond-> (if dynamic? [property :_properties] [property])
                                                                   (not (gt/property-overridden? node property)) (conj :_overridden-properties)))
            (not (nil? setter-fn))
            (as-> ctx (apply-tx ctx (call-setter-fn ctx property setter-fn (:basis ctx) node-id old-value new-value))))))))

(defn- apply-defaults [ctx node]
  ;; Ensure property setters are run for all properties that have a non-nil
  ;; value immediately after construction. We don't need to mark outputs
  ;; activated, as we're doing this on a newly constructed node and ctx-add-node
  ;; will mark all our outputs activated regardless.
  (let [node-id (gt/node-id node)
        node-type (gt/node-type node)]
    (reduce
      (fn [ctx [property-label property-value]]
        (if (nil? property-value)
          ctx
          (let [setter-fn (in/property-setter node-type property-label)]
            (validate-property-value node-type node-id property-label property-value)
            (if (nil? setter-fn)
              ctx
              (apply-tx ctx (call-setter-fn ctx property-label setter-fn (:basis ctx) node-id nil property-value))))))
      ctx
      (if (some? (gt/original node))
        (gt/overridden-properties node)
        (let [default-property-values (in/defaults node-type)
              assigned-property-values (gt/assigned-properties node)]
          (e/map (fn [[property-label default-property-value]]
                   (pair property-label
                         (get assigned-property-values property-label default-property-value)))
                 default-property-values))))))

(defn- ctx-add-node [ctx node]
  (let [basis-after (gt/add-node (:basis ctx) node)
        node-id (gt/node-id node)]
    (assert (gt/node-id? node-id))
    (-> ctx
        (assoc :basis basis-after)
        (apply-defaults node)
        (update :nodes-added conj node-id)
        (assoc-in [:successors-changed node-id] nil)
        (mark-all-outputs-activated node-id))))

(defn- ctx-update-property
  [ctx node-id property fn args opts]
  (let [basis (:basis ctx)]
    (if-let [node (gt/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (let [;; Fetch the node value by either evaluating (value ...) for the property or looking in the node map
            ;; The context is intentionally bare, i.e. only :basis, for this reason
            evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context (:tx-data-context ctx)})
            old-value (in/node-property-value node property evaluation-context)
            new-value (if (:inject-evaluation-context opts)
                        (apply fn evaluation-context old-value args)
                        (apply fn old-value args))
            override-node? (some? (gt/original node))
            dynamic? (not (contains? (some-> (gt/node-type node) in/all-properties) property))]
        (invoke-setter ctx node-id node property old-value new-value override-node? dynamic?))
      ctx)))

(defn- ctx-set-property
  [ctx node-id property-label new-value]
  (let [basis (:basis ctx)
        node (gt/node-by-id-at basis node-id)]
    (if (nil? node) ; nil if node was deleted in this transaction
      ctx
      (let [is-override-node (some? (gt/original node))
            is-dynamic (not (contains? (in/all-properties (gt/node-type node)) property-label))

            ;; Use a custom evaluation-context since we're inside a transaction
            ;; and cannot use the cache.
            old-value (let [tx-data-context (:tx-data-context ctx)
                            evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
                        (in/node-property-value node property-label evaluation-context))]
        (invoke-setter ctx node-id node property-label old-value new-value is-override-node is-dynamic)))))

(defn- ctx-set-property-to-nil [ctx node-id node property]
  (let [basis (:basis ctx)
        old-value (gt/get-property node basis property)
        node-type (gt/node-type node)]
    (if-let [setter-fn (in/property-setter node-type property)]
      (apply-tx ctx (call-setter-fn ctx property setter-fn basis node-id old-value nil))
      ctx)))

(defn- ctx-clear-property
  [ctx node-id property]
  (let [basis (:basis ctx)]
    (if-let [node (gt/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (let [dynamic? (not (contains? (some-> (gt/node-type node) in/all-properties) property))]
        (-> ctx
            (mark-outputs-activated node-id (cond-> (if dynamic? [property :_properties] [property])
                                              (and (gt/original node) (gt/property-overridden? node property)) (conj :_overridden-properties)))
            (update :basis gt/replace-node node-id (gt/clear-property node basis property))
            (ctx-set-property-to-nil node-id node property)))
      ctx)))

(defn- ctx-callback
  [ctx fn args opts]
  (if (:inject-evaluation-context opts)
    (let [basis (:basis ctx)
          tx-data-context (:tx-data-context ctx)
          evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
      (apply fn evaluation-context args))
    (apply fn args))
  ctx)

(defn- ctx-expand
  [ctx fn args opts]
  (apply-tx
    ctx
    (if (:inject-evaluation-context opts)
      (let [basis (:basis ctx)
            tx-data-context (:tx-data-context ctx)
            evaluation-context (in/custom-evaluation-context {:basis basis :tx-data-context tx-data-context})]
        (apply fn evaluation-context args))
      (apply fn args))))

(defn- ctx-disconnect-single [ctx target target-id target-label]
  (if (= :one (in/input-cardinality (gt/node-type target) target-label))
    (disconnect-inputs ctx target-id target-label)
    ctx))

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

(defn- ctx-connect [{:keys [basis] :as ctx} source-id source-label target-id target-label]
  (if-let [source (gt/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (gt/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
      (let [target-node-type (gt/node-type target)
            target-cascade-deletes (in/cascade-deletes target-node-type)]
        (assert-type-compatible source-id source source-label target-id target target-label)
        (-> ctx
            ;; If the input has :one cardinality, disconnect existing connections first
            (ctx-disconnect-single target target-id target-label)
            (mark-input-activated target-id target-label)
            (update :basis gt/connect source-id source-label target-id target-label)
            ;; When updating the successors, we must also consider any override
            ;; nodes of the source node, since these will inherit an implicit
            ;; connection between them and the corresponding override nodes of
            ;; the target node.
            (flag-successors-changed (e/cons
                                       (pair source-id source-label)
                                       (e/map #(pair % source-label)
                                              (ig/get-overrides basis source-id))))
            ;; If we're connecting to a :cascade-delete input, we will need to
            ;; re-traverse the :cascade-delete inputs of the connected sub-graph
            ;; and create override nodes for each node. This happens in the
            ;; update-overrides function once the transaction concludes.
            (cond->
              (contains? target-cascade-deletes target-label)
              (flag-override-nodes-affected target-id))))
      ctx)
    ctx))

(defn- ctx-remove-overrides [ctx source-id source-label target-id target-label]
  (let [basis (:basis ctx)
        target (gt/node-by-id-at basis target-id)]
    (if (contains? (in/cascade-deletes (gt/node-type target)) target-label)
      (let [source-override-nodes (map (partial gt/node-by-id-at basis) (ig/get-overrides basis source-id))]
        (loop [target-override-node-ids (ig/get-overrides basis target-id)
               ctx ctx]
          (if-let [target-override-id (first target-override-node-ids)]
            (let [basis (:basis ctx)
                  target-override-node (gt/node-by-id-at basis target-override-id)
                  target-override-id (gt/override-id target-override-node)
                  source-override-nodes-in-target-override (filter #(= target-override-id (gt/override-id %)) source-override-nodes)
                  traverse-fn (ig/override-traverse-fn basis target-override-id)
                  to-delete (ig/pre-traverse basis (mapv gt/node-id source-override-nodes-in-target-override) traverse-fn)]
              (recur (rest target-override-node-ids)
                     (reduce ctx-delete-node ctx to-delete)))
            ctx)))
      ctx)))

(defn- ctx-disconnect [ctx source-id source-label target-id target-label]
  (-> ctx
      (mark-input-activated target-id target-label)
      (update :basis gt/disconnect source-id source-label target-id target-label)
      ;; When updating the successors, we must also consider any override nodes
      ;; of the source node, since these will inherit an implicit connection
      ;; between them and the corresponding override nodes of the target node.
      (flag-successors-changed (e/cons
                                 (pair source-id source-label)
                                 (e/map #(pair % source-label)
                                        (ig/get-overrides (:basis ctx) source-id))))
      (ctx-remove-overrides source-id source-label target-id target-label)))

(defn- ctx-update-graph-value
  [ctx graph-id fn args]
  (cond-> (update-in ctx [:basis :graphs graph-id :graph-values] #(apply fn % args))
          (:track-changes ctx) (update :graphs-modified conj graph-id)))

;; ---------------------------------------------------------------------------
;; Transaction steps
;; ---------------------------------------------------------------------------

(defonce/type AddNodeTXS [added-node]
  TransactionStep
  (step-type [_this]
    :tx-step/add-node)

  (metrics-key [_this]
    (gt/node-id added-node))

  (perform [_this ctx]
    (ctx-add-node ctx added-node)))

(defn new-node
  "*transaction step* - Add a node to its corresponding graph."
  [node]
  {:pre [(some? (gt/node-id node))]}
  [(->AddNodeTXS node)])

(defonce/type DeleteNodeTXS [node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/delete-node)

  (metrics-key [_this]
    node-id)

  (perform [_this ctx]
    (ctx-delete-node ctx node-id)))

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

  (perform [_this ctx]
    (ctx-override ctx root-id traverse-fn init-props-fn init-fn properties-by-node-id)))

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

  (perform [_this ctx]
    (ctx-transfer-overrides ctx from-id->to-id)))

(defn transfer-overrides [from-id->to-id]
  [(->TransferOverridesTXS from-id->to-id)])

(defonce/type SetPropertyTXS [node-id property-label new-value]
  TransactionStep
  (step-type [_this]
    :tx-step/set-property)

  (metrics-key [_this]
    (pair node-id property-label))

  (perform [_this ctx]
    (ctx-set-property ctx node-id property-label new-value)))

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

  (perform [_this ctx]
    (ctx-update-property ctx node-id property-label fn args opts)))

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

  (perform [_this ctx]
    (ctx-clear-property ctx node-id property-label)))

(defn clear-property
  "*transaction step* - Clears a property on a node."
  [node-id property-label]
  [(->ClearPropertyTXS node-id property-label)])

(defonce/type UpdateGraphValueTXS [graph-id fn args]
  TransactionStep
  (step-type [_this]
    :tx-step/update-graph-value)

  (metrics-key [_this]
    graph-id)

  (perform [_this ctx]
    (ctx-update-graph-value ctx graph-id fn args)))

(defn update-graph-value
  "*transaction step* - Update a graph value."
  [graph-id fn args]
  {:pre [(gt/graph-id? graph-id)
         (ifn? fn)
         (coll/eager-seqable? args)]}
  [(->UpdateGraphValueTXS graph-id fn args)])

(defonce/type CallbackTXS [callback-fn args opts]
  TransactionStep
  (step-type [_this]
    :tx-step/callback)

  (metrics-key [_this]
    nil)

  (perform [_this ctx]
    (ctx-callback ctx callback-fn args opts)))

(defn callback
  "*transaction step* - Call a function from within the transaction."
  [callback-fn args opts]
  {:pre [(ifn? callback-fn)
         (coll/eager-seqable? args)
         (or (nil? opts) (map? opts))]}
  [(->CallbackTXS callback-fn args opts)])

(defonce/type ConnectTXS [source-id source-label target-id target-label]
  TransactionStep
  (step-type [_this]
    :tx-step/connect)

  (metrics-key [_this]
    [source-id source-label target-id target-label])

  (perform [_this ctx]
    (ctx-connect ctx source-id source-label target-id target-label)))

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

  (perform [_this ctx]
    (ctx-expand ctx tx-steps-fn args opts)))

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

  (perform [_this ctx]
    (ctx-disconnect ctx source-id source-label target-id target-label)))

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a transaction step
  disconnecting a source node and label from a target node and label."
  [source-id source-label target-id target-label]
  [(->DisconnectTXS source-id source-label target-id target-label)])

(defn disconnect-sources
  [basis target-id target-label]
  (for [[source-id source-label] (gt/sources basis target-id target-label)]
    (disconnect source-id source-label target-id target-label)))

(defonce/type LabelTXS [label]
  TransactionStep
  (step-type [_this]
    :tx-step/label)

  (metrics-key [_this]
    nil)

  (perform [_this ctx]
    (assoc ctx :label label)))

(defn label
  [label]
  [(->LabelTXS label)])

(defonce/type SequenceLabelTXS [sequence-label]
  TransactionStep
  (step-type [_this]
    :tx-step/sequence-label)

  (metrics-key [_this]
    nil)

  (perform [_this ctx]
    (assoc ctx :sequence-label sequence-label)))

(defn sequence-label
  [sequence-label]
  [(->SequenceLabelTXS sequence-label)])

(defonce/type InvalidateTXS [node-id]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate)

  (metrics-key [_this]
    node-id)

  (perform [_this ctx]
    (if (gt/node-by-id-at (:basis ctx) node-id)
      (mark-all-outputs-activated ctx node-id)
      ctx)))

(defn invalidate
  [node-id]
  [(->InvalidateTXS node-id)])

(defonce/type InvalidateOutputTXS [node-id output-label]
  TransactionStep
  (step-type [_this]
    :tx-step/invalidate-output)

  (metrics-key [_this]
    (pair node-id output-label))

  (perform [_this ctx]
    (mark-output-activated ctx node-id output-label)))

(defn invalidate-output
  [node-id output-label]
  [(->InvalidateOutputTXS node-id output-label)])

;; ---------------------------------------------------------------------------
;; Transaction step inspection
;; ---------------------------------------------------------------------------

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

;; ---------------------------------------------------------------------------
;; Applying transactions
;; ---------------------------------------------------------------------------

(defn apply-tx
  [ctx actions]
  (reduce
    (fn [ctx ^TransactionStep action]
      (cond
        (nil? action)
        ctx

        (sequential? action)
        (apply-tx ctx action)

        :else
        (-> (try
              (du/measuring (:metrics ctx) (.step-type action) (.metrics-key action)
                (.perform action ctx))
              (catch Exception e
                (when *tx-debug*
                  (println (txerrstr ctx "Transaction failed on " action)))
                (throw e)))
            (update :completed-action-count inc))))
    ctx
    actions))

(defn mark-nodes-modified
  [{:keys [nodes-affected] :as ctx}]
  (assoc ctx :nodes-modified (into #{} (map gt/endpoint-node-id) nodes-affected)))

(defn- map-vals-bargs
  [m f]
  (util/map-vals f m))

(defn apply-tx-label
  [{:keys [basis label sequence-label] :as ctx}]
  (cond-> (update-in ctx [:basis :graphs] map-vals-bargs #(assoc % :tx-sequence-label sequence-label))

    label
    (update-in [:basis :graphs] map-vals-bargs #(assoc % :tx-label label))))

(def tx-report-keys
  (cond-> [:basis :graphs-modified :nodes-added :nodes-modified :nodes-deleted :outputs-modified :label :sequence-label]
          (du/metrics-enabled?) (conj :metrics)))

(defn finalize-update
  [{:keys [nodes-modified graphs-modified tx-data-context] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (zero? (:completed-action-count ctx)) :empty :ok)
             :graphs-modified (into graphs-modified (map gt/node-id->graph-id) nodes-modified)
             :tx-data-context-map (deref tx-data-context))))

(defn new-transaction-context
  [basis node-id-generators override-id-generator tx-data-context-map metrics-collector track-changes]
  {:pre [(map? tx-data-context-map)]}
  {:basis basis
   :nodes-affected #{}
   :nodes-added []
   :nodes-modified #{}
   :nodes-deleted {}
   :outputs-modified #{}
   :graphs-modified #{}
   :override-nodes-affected-seen #{}
   :override-nodes-affected-ordered []
   :successors-changed {}
   :node-id-generators node-id-generators
   :override-id-generator override-id-generator
   :completed-action-count 0
   :txid (new-txid)
   :tx-data-context (atom tx-data-context-map)
   :metrics metrics-collector
   :track-changes track-changes})

(defn update-overrides
  [{:keys [override-nodes-affected-ordered] :as ctx}]
  (du/measuring (:metrics ctx) :update-overrides
    (reduce populate-overrides
            ctx
            override-nodes-affected-ordered)))

(defn update-successors
  [{:keys [successors-changed] :as ctx}]
  (du/measuring (:metrics ctx) :update-successors
    (update ctx :basis ig/update-successors successors-changed)))

(defn trace-dependencies
  [ctx]
  ;; At this point, :nodes-affected is a set of all output Endpoints that have
  ;; been directly affected by the transaction changes.
  ;; We now follow these outputs recursively to obtain a sequence of all the
  ;; outputs that depend on them in the entire graph.
  (du/measuring (:metrics ctx) :trace-dependencies
    (let [outputs-modified (gt/dependencies (:basis ctx) (:nodes-affected ctx))]
      (assoc ctx :outputs-modified outputs-modified))))

(defn transact*
  [ctx actions]
  (when *tx-debug*
    (println (txerrstr ctx "actions" (seq actions))))
  (-> ctx
      (apply-tx actions)
      update-overrides
      mark-nodes-modified
      update-successors
      trace-dependencies
      apply-tx-label
      finalize-update))

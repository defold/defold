;; Copyright 2020-2022 The Defold Foundation
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
            [internal.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [schema.core :as s]
            [util.coll :refer [pair]]
            [util.debug-util :as du])
  (:import [internal.graph.types Arc]))

(set! *warn-on-reflection* true)

;; ---------------------------------------------------------------------------
;; Configuration parameters
;; ---------------------------------------------------------------------------
(def maximum-retrigger-count 100)
(def maximum-graph-coloring-recursion 1000)

;; ---------------------------------------------------------------------------
;; Internal state
;; ---------------------------------------------------------------------------
(def ^:dynamic *tx-debug* nil)

(def ^:private ^java.util.concurrent.atomic.AtomicInteger next-txid (java.util.concurrent.atomic.AtomicInteger. 1))
(defn- new-txid [] (.getAndIncrement next-txid))

(defmacro txerrstr [ctx & rest]
  `(str (:txid ~ctx) ": " ~@(interpose " " rest)))

;; ---------------------------------------------------------------------------
;; Building transactions
;; ---------------------------------------------------------------------------
(defn new-node
  "*transaction step* - add a node to a graph"
  [node]
  [{:type :create-node
    :node node}])

(defn delete-node
  [node-id]
  [{:type :delete-node
    :node-id node-id}])

(defn- new-override
  [override-id root-id traverse-fn]
  [{:type :new-override
    :override-id override-id
    :root-id root-id
    :traverse-fn traverse-fn}])

(defn- override-node
  [original-node-id override-node-id]
  [{:type :override-node
    :original-node-id original-node-id
    :override-node-id override-node-id}])

(defn override
  [root-id traverse-fn init-fn properties-by-node-id]
  [{:type :override
    :root-id root-id
    :traverse-fn traverse-fn
    :init-fn init-fn
    :properties-by-node-id properties-by-node-id}])

(defn transfer-overrides [from-id->to-id]
  [{:type :transfer-overrides
    :from-id->to-id from-id->to-id}])

(defn update-property
  "*transaction step* - Expects a node, a property label, and a
  function f (with optional args) to be performed on the current value
  of the property."
  [node-id pr f args]
  [{:type :update-property
    :node-id node-id
    :property pr
    :fn f
    :args args}])

(defn update-property-ec
  "Same as update-property, but injects the in-transaction evaluation-context
  as the first argument to the update-fn."
  [node-id pr f args]
  [{:type :update-property
    :node-id node-id
    :property pr
    :fn f
    :args args
    :inject-evaluation-context true}])

(defn clear-property
  [node-id pr]
  [{:type :clear-property
    :node-id node-id
    :property pr}])

(defn update-graph-value
  [graph-id f args]
  [{:type :update-graph-value
    :graph-id graph-id
    :fn f
    :args args}])

(defn callback
  [f args]
  [{:type :callback
    :fn f
    :args args}])

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and label  and a target node and label. It returns a value suitable for consumption by [[perform]]."
  [source-id source-label target-id target-label]
  [{:type :connect
    :source-id source-id
    :source-label source-label
    :target-id target-id
    :target-label target-label}])

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a
  transaction step disconnecting a source node and label
  from a target node and label. It returns a value suitable for consumption
  by [[perform]]."
  [source-id source-label target-id target-label]
  [{:type :disconnect
    :source-id source-id
    :source-label source-label
    :target-id target-id
    :target-label target-label}])

(defn disconnect-sources
  [basis target-id target-label]
  (for [[source-id source-label] (gt/sources basis target-id target-label)]
    (disconnect source-id source-label target-id target-label)))

(defn label
  [label]
  [{:type :label
    :label label}])

(defn sequence-label
  [seq-label]
  [{:type :sequence-label
    :label seq-label}])

(defn invalidate
  [node-id]
  [{:type :invalidate
    :node-id node-id}])

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------
(defn- mark-input-activated
  [ctx node-id input-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
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
            dirty-deps))))

(defn- mark-output-activated
  [ctx node-id output-label]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (let [nodes-affected (:nodes-affected ctx)]
    (assoc ctx
      :nodes-affected
      (conj nodes-affected (gt/endpoint node-id output-label)))))

(defn- mark-outputs-activated
  [ctx node-id output-labels]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (let [nodes-affected (:nodes-affected ctx)]
    (assoc ctx
      :nodes-affected
      (into nodes-affected
            (map #(gt/endpoint node-id %))
            output-labels))))

(defn- mark-all-outputs-activated
  [ctx node-id]
  ;; This gets called a lot, so we're trying to keep allocations to a minimum.
  (let [basis (:basis ctx)
        output-labels (-> (gt/node-by-id-at basis node-id)
                          gt/node-type
                          in/output-labels)
        nodes-affected (:nodes-affected ctx)]
    (assoc ctx
      :nodes-affected
      (into nodes-affected
            (map #(gt/endpoint node-id %))
            output-labels))))

(defn- next-node-id [ctx graph-id]
  (is/next-node-id* (:node-id-generators ctx) graph-id))

(defn- next-override-id [ctx graph-id]
  (is/next-override-id* (:override-id-generator ctx) graph-id))

(defmulti perform
  "A multimethod used for defining methods that perform the individual
  actions within a transaction. This is for internal use, not intended
  to be extended by applications.

  Perform takes a transaction context (ctx) and a map (m) containing a
  value for keyword `:type`, and other keys and values appropriate to
  the transformation it represents. Callers should regard the map and
  context as opaque.

  Calls to perform are only executed by [[transact]]. The data
  required for `perform` calls are constructed in action functions,
  such as [[connect]] and [[update-property]]."
  (fn [ctx m] (:type m)))

(defmulti metrics-key :type)

(def ^:private ctx-disconnect)

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

(def ^:private replace-node (comp first gt/replace-node))

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

(defmethod perform :delete-node
  [ctx {:keys [node-id]}]
  (ctx-delete-node ctx node-id))

(defmethod metrics-key :delete-node
  [{:keys [node-id]}]
  node-id)

(defmethod perform :new-override
  [ctx {:keys [override-id root-id traverse-fn]}]
  (-> ctx
      (update :basis gt/add-override override-id (ig/make-override root-id traverse-fn))))

(defmethod metrics-key :new-override
  [{:keys [root-id]}]
  root-id)

(defn- flag-all-successors-changed [ctx node-ids]
  (let [successors (get ctx :successors-changed)]
    (assoc ctx :successors-changed (reduce (fn [successors node-id]
                                             (assoc successors node-id nil))
                                           successors
                                           node-ids))))

(defn- flag-successors-changed [ctx changes]
  (let [successors (get ctx :successors-changed)]
    (assoc ctx :successors-changed (reduce (fn [successors [node-id label]]
                                             (if-let [node-succ (get successors node-id #{})]
                                               (assoc successors node-id (conj node-succ label))
                                               successors)) ; Found nil - all successors already flagged as changed.
                                           successors
                                           changes))))

(defn- ctx-override-node [ctx original-node-id override-node-id]
  (assert (= (gt/node-id->graph-id original-node-id) (gt/node-id->graph-id override-node-id))
          "Override nodes must belong to the same graph as the original")
  (let [basis (:basis ctx)
        all-originals (ig/override-originals basis original-node-id)]
    (-> ctx
        (assoc :basis (gt/override-node basis original-node-id override-node-id))
        (flag-all-successors-changed all-originals)
        (flag-successors-changed (mapcat #(gt/sources basis %) all-originals)))))

(defmethod perform :override-node
  [ctx {:keys [original-node-id override-node-id]}]
  (ctx-override-node ctx original-node-id override-node-id))

(defmethod metrics-key :override-node
  [{:keys [original-node-id]}]
  original-node-id)

(declare apply-tx)

(defmethod perform :override
  [ctx {:keys [root-id traverse-fn init-fn properties-by-node-id]}]
  (let [basis (:basis ctx)
        graph-id (gt/node-id->graph-id root-id)
        node-ids (ig/pre-traverse basis [root-id] traverse-fn)
        override-id (next-override-id ctx graph-id)
        override-nodes (mapv #(in/make-override-node
                                override-id
                                (next-node-id ctx graph-id)
                                (gt/node-type (gt/node-by-id-at basis %))
                                %
                                (properties-by-node-id %))
                             node-ids)
        override-node-ids (map gt/node-id override-nodes)
        original-node-id->override-node-id (zipmap node-ids override-node-ids)
        new-override-nodes-tx-data (map new-node override-nodes)
        new-override-tx-data (concat
                               (new-override override-id root-id traverse-fn)
                               (map
                                 (fn [node-id override-node-id]
                                   (override-node node-id override-node-id))
                                 node-ids
                                 override-node-ids))]
    (as-> ctx ctx'
      (apply-tx ctx' (concat new-override-nodes-tx-data
                             new-override-tx-data))
      (apply-tx ctx' (init-fn (in/custom-evaluation-context {:basis (:basis ctx')})
                              original-node-id->override-node-id)))))

(defmethod metrics-key :override
  [{:keys [root-id]}]
  root-id)

(defn- node-id->override-id [basis node-id]
  (->> node-id
       (gt/node-by-id-at basis)
       gt/override-id))

(declare ctx-add-node)

(defn- ctx-make-override-nodes [ctx override-id node-ids]
  (reduce (fn [ctx node-id]
            (let [basis (:basis ctx)]
              (if (some #(= override-id (node-id->override-id basis %))
                        (ig/get-overrides basis node-id))
                ctx
                (let [graph-id (gt/node-id->graph-id node-id)
                      new-override-node-id (next-node-id ctx graph-id)
                      new-override-node (in/make-override-node
                                          override-id
                                          new-override-node-id
                                          (gt/node-type (gt/node-by-id-at basis node-id))
                                          node-id
                                          {})]
                  (-> ctx
                      (ctx-add-node new-override-node)
                      (ctx-override-node node-id new-override-node-id))))))
          ctx
          node-ids))

(defn- populate-overrides [ctx node-id]
  (let [basis (:basis ctx)
        override-node-ids (ig/get-overrides basis node-id)
        override-node-count (count override-node-ids)
        ctx (loop [override-node-index 0
                   ctx ctx
                   prev-traverse-fn nil
                   prev-traverse-result nil]
              (if (>= override-node-index override-node-count)
                ctx
                (let [^long override-node-id (override-node-ids override-node-index)
                      override-id (node-id->override-id basis override-node-id)
                      traverse-fn (:traverse-fn (ig/override-by-id basis override-id))
                      node-ids (if (identical? prev-traverse-fn traverse-fn)
                                 prev-traverse-result
                                 (subvec (ig/pre-traverse basis [node-id] traverse-fn) 1))]
                  (recur (inc override-node-index)
                         (ctx-make-override-nodes ctx override-id node-ids)
                         traverse-fn
                         node-ids))))]
    (reduce populate-overrides
            ctx
            override-node-ids)))

(defmethod perform :transfer-overrides
  [ctx {:keys [from-id->to-id]}]
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
                              [new-basis] (gt/replace-node basis override-node-id (gt/set-original override-node new-original))]
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

(defmethod metrics-key :transfer-overrides
  [{:keys [from-id->to-id]}]
  ;; When metrics are enabled, this is called for one override node at a time.
  ;; This is potentially less efficient, but we get valuable context about which
  ;; specific nodes are costly to transfer overrides for.
  (when (= 1 (count from-id->to-id))
    (second (first from-id->to-id))))

(defn- property-default-setter
  [basis node-id node property _ new-value]
  (first (gt/replace-node basis node-id (gt/set-property node basis property new-value))))

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

(defn- invoke-setter
  [ctx node-id node property old-value new-value override-node? dynamic?]
  (let [node-type (gt/node-type node)
        value-type (some-> (in/property-type node-type property) deref in/schema s/maybe)]
    (if-let [validation-error (and in/*check-schemas* value-type (s/check value-type new-value))]
      (do
        (in/warn-output-schema node-id node-type property new-value value-type validation-error)
        (throw (ex-info "SCHEMA-VALIDATION"
                        {:node-id node-id
                         :type node-type
                         :property property
                         :expected value-type
                         :actual new-value
                         :validation-error validation-error})))
      (let [setter-fn (in/property-setter node-type property)]
        (-> ctx
            (update :basis property-default-setter node-id node property old-value new-value)
            (cond->
              (not= old-value new-value)
              (cond->
                (not override-node?) (mark-output-activated node-id property)
                override-node? (mark-outputs-activated node-id (cond-> (if dynamic? [property :_properties] [property])
                                                                 (not (gt/property-overridden? node property)) (conj :_overridden-properties)))
                (not (nil? setter-fn))
                ((fn [ctx] (apply-tx ctx (call-setter-fn ctx property setter-fn (:basis ctx) node-id old-value new-value)))))))))))

(defn- apply-defaults [ctx node]
  (let [node-id (gt/node-id node)
        override-node? (some? (gt/original node))]
    (loop [ctx ctx
           props (seq (in/declared-property-labels (gt/node-type node)))]
      (if-let [prop (first props)]
        (let [ctx (if-let [v (get node prop)]
                    (invoke-setter ctx node-id node prop nil v override-node? false)
                    ctx)]
          (recur ctx (rest props)))
        ctx))))

(defn- ctx-add-node [ctx node]
  (let [[basis-after full-node] (gt/add-node (:basis ctx) node)
        node-id (gt/node-id full-node)]
    (-> ctx
        (assoc :basis basis-after)
        (apply-defaults node)
        (update :nodes-added conj node-id)
        (assoc-in [:successors-changed node-id] nil)
        (mark-all-outputs-activated node-id))))

(defmethod perform :create-node [ctx {:keys [node]}]
  (when (and *tx-debug* (nil? (gt/node-id node))) (println "NIL NODE ID: " node))
  (ctx-add-node ctx node))

(defmethod metrics-key :create-node
  [{:keys [node]}]
  (gt/node-id node))

(defmethod perform :update-property [ctx {:keys [node-id property fn args inject-evaluation-context] :as tx-step}]
  (let [basis (:basis ctx)]
    (when (and *tx-debug* (nil? node-id)) (println "NIL NODE ID: update-property " tx-step))
    (if-let [node (gt/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (let [;; Fetch the node value by either evaluating (value ...) for the property or looking in the node map
            ;; The context is intentionally bare, i.e. only :basis, for this reason
            evaluation-context (in/custom-evaluation-context {:basis basis})
            old-value (in/node-property-value* node property evaluation-context)
            new-value (if inject-evaluation-context
                        (apply fn evaluation-context old-value args)
                        (apply fn old-value args))
            override-node? (some? (gt/original node))
            dynamic? (not (contains? (some-> (gt/node-type node) in/all-properties) property))]
        (invoke-setter ctx node-id node property old-value new-value override-node? dynamic?))
      ctx)))

(defmethod metrics-key :update-property
  [{:keys [node-id property]}]
  [node-id property])

(defn- ctx-set-property-to-nil [ctx node-id node property]
  (let [basis (:basis ctx)
        old-value (gt/get-property node basis property)
        node-type (gt/node-type node)]
    (if-let [setter-fn (in/property-setter node-type property)]
      (apply-tx ctx (call-setter-fn ctx property setter-fn basis node-id old-value nil))
      ctx)))

(defmethod perform :clear-property [ctx {:keys [node-id property]}]
  (let [basis (:basis ctx)]
    (if-let [node (gt/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (let [dynamic? (not (contains? (some-> (gt/node-type node) in/all-properties) property))]
        (-> ctx
            (mark-outputs-activated node-id (cond-> (if dynamic? [property :_properties] [property])
                                              (and (gt/original node) (gt/property-overridden? node property)) (conj :_overridden-properties)))
            (update :basis replace-node node-id (gt/clear-property node basis property))
            (ctx-set-property-to-nil node-id node property)))
      ctx)))

(defmethod metrics-key :clear-property
  [{:keys [node-id property]}]
  [node-id property])

(defmethod perform :callback [ctx {:keys [fn args]}]
  (apply fn args)
  ctx)

(defmethod metrics-key :callback
  [_]
  nil)

(defn- ctx-disconnect-single [ctx target target-id target-label]
  (if (= :one (in/input-cardinality (gt/node-type target) target-label))
    (disconnect-inputs ctx target-id target-label)
    ctx))

(defn- flag-override-nodes-affected [ctx target target-label]
  (let [target-id (gt/node-id target)
        override-nodes-affected-seen (:override-nodes-affected-seen ctx)]
    (if (contains? override-nodes-affected-seen target-id)
      ctx
      (let [target-node-type (gt/node-type target)
            target-cascade-deletes (in/cascade-deletes target-node-type)]
        (if-not (contains? target-cascade-deletes target-label)
          ctx
          (let [override-nodes-affected-ordered (:override-nodes-affected-ordered ctx)]
            (assoc ctx
              :override-nodes-affected-seen (conj override-nodes-affected-seen target-id)
              :override-nodes-affected-ordered (conj override-nodes-affected-ordered target-id))))))))

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
    (assert (in/type-compatible? output-valtype input-valtype)
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s and %s are not have compatible types."
                    source-id (in/type-name output-nodetype) source-label
                    target-id (in/type-name input-nodetype) target-label
                    (:k output-valtype) (:k input-valtype)))))

(defn- ctx-connect [ctx source-id source-label target-id target-label]
  (if-let [source (gt/node-by-id-at (:basis ctx) source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (gt/node-by-id-at (:basis ctx) target-id)] ; nil if target node was deleted in this transaction
      (do
        (assert-type-compatible source-id source source-label target-id target target-label)
        (-> ctx
            ;; If the input has :one cardinality, disconnect existing connections first
            (ctx-disconnect-single target target-id target-label)
            (mark-input-activated target-id target-label)
            (update :basis gt/connect source-id source-label target-id target-label)
            (flag-successors-changed [[source-id source-label]])
            (flag-override-nodes-affected target target-label)))
      ctx)
    ctx))

(defmethod perform :connect
  [ctx {:keys [source-id source-label target-id target-label] :as tx-data}]
  (ctx-connect ctx source-id source-label target-id target-label))

(defmethod metrics-key :connect
  [{:keys [source-id source-label target-id target-label]}]
  [source-id source-label target-id target-label])

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
      (flag-successors-changed [[source-id source-label]])
      (ctx-remove-overrides source-id source-label target-id target-label)))

(defmethod perform :disconnect
  [ctx {:keys [source-id source-label target-id target-label]}]
  (ctx-disconnect ctx source-id source-label target-id target-label))

(defmethod metrics-key :disconnect
  [{:keys [source-id source-label target-id target-label]}]
  [source-id source-label target-id target-label])

(defmethod perform :update-graph-value
  [ctx {:keys [graph-id fn args]}]
  (-> ctx
      (update-in [:basis :graphs graph-id :graph-values] #(apply fn % args))
      (update :graphs-modified conj graph-id)))

(defmethod metrics-key :update-graph-value
  [{:keys [graph-id]}]
  graph-id)

(defmethod perform :label
  [ctx {:keys [label]}]
  (assoc ctx :label label))

(defmethod metrics-key :label
  [_]
  nil)

(defmethod perform :sequence-label
  [ctx {:keys [label]}]
  (assoc ctx :sequence-label label))

(defmethod metrics-key :sequence-label
  [_]
  nil)

(defmethod perform :invalidate
  [ctx {:keys [node-id] :as tx-data}]
  (if (gt/node-by-id-at (:basis ctx) node-id)
    (mark-all-outputs-activated ctx node-id)
    ctx))

(defmethod metrics-key :invalidate
  [{:keys [node-id]}]
  node-id)

(defn- apply-tx
  [ctx actions]
  (loop [ctx ctx
         actions actions]
    (if (seq actions)
      (if-let [action (first actions)]
        (if (sequential? action)
          (recur (apply-tx ctx action) (next actions))
          (recur (-> (try
                       (du/measuring (:metrics ctx) (:type action) (metrics-key action)
                         (perform ctx action))
                       (catch Exception e
                         (when *tx-debug*
                           (println (txerrstr ctx "Transaction failed on " action)))
                         (throw e)))
                     (update :completed conj action))
                 (next actions)))
        (recur ctx (next actions)))
      ctx)))

(defn- mark-nodes-modified
  [{:keys [nodes-affected] :as ctx}]
  (assoc ctx :nodes-modified (into #{} (map gt/endpoint-node-id) nodes-affected)))

(defn- map-vals-bargs
  [m f]
  (util/map-vals f m))

(defn- apply-tx-label
  [{:keys [basis label sequence-label] :as ctx}]
  (cond-> (update-in ctx [:basis :graphs] map-vals-bargs #(assoc % :tx-sequence-label sequence-label))

    label
    (update-in [:basis :graphs] map-vals-bargs #(assoc % :tx-label label))))

(def tx-report-keys
  (cond-> [:basis :graphs-modified :nodes-added :nodes-modified :nodes-deleted :outputs-modified :label :sequence-label]
          (du/metrics-enabled?) (conj :metrics)))

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph."
  [{:keys [nodes-modified graphs-modified] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (empty? (:completed ctx)) :empty :ok)
             :graphs-modified (into graphs-modified (map gt/node-id->graph-id nodes-modified)))))

(defn new-transaction-context
  [basis node-id-generators override-id-generator metrics-collector]
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
   :completed []
   :txid (new-txid)
   :tx-data-context (atom {})
   :deferred-setters []
   :metrics metrics-collector})

(defn- update-overrides
  [{:keys [override-nodes-affected-ordered] :as ctx}]
  (du/measuring (:metrics ctx) :update-overrides
    (reduce populate-overrides
            ctx
            override-nodes-affected-ordered)))

(defn- update-successors
  [{:keys [successors-changed] :as ctx}]
  (du/measuring (:metrics ctx) :update-successors
    (update ctx :basis ig/update-successors successors-changed)))

(defn- trace-dependencies
  [ctx]
  ;; at this point, :outputs-modified contains [node-id output] pairs.
  ;; afterwards, it will have the transitive closure of all [node-id output] pairs
  ;; reachable from the original collection.
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

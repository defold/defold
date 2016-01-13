(ns internal.transaction
  "Internal functions that implement the transactional behavior."
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [internal.node :as in]
            [internal.property :as ip]
            [internal.system :as is]))

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
  [{:type     :create-node
    :node     node}])

(defn become
  [node-id to-node]
  [{:type     :become
    :node-id  node-id
    :to-node  to-node}])

(defn delete-node
  [node-id]
  [{:type     :delete-node
    :node-id  node-id}])

(defn new-override
  [override-id root-id traverse-fn]
  [{:type :new-override
    :override-id override-id
    :root-id root-id
    :traverse-fn traverse-fn}])

(defn override-node
  [original-node-id override-node-id]
  [{:type        :override-node
    :original-node-id original-node-id
    :override-node-id override-node-id}])

(defn update-property
  "*transaction step* - Expects a node, a property label, and a
  function f (with optional args) to be performed on the current value
  of the property."
  [node-id pr f args]
  [{:type     :update-property
    :node-id  node-id
    :property pr
    :fn       f
    :args     args}])

(defn clear-property
  [node-id pr]
  [{:type     :clear-property
    :node-id  node-id
    :property pr}])

(defn update-graph-value
  [gid f args]
  [{:type :update-graph-value
    :gid  gid
    :fn   f
    :args args}])

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and label (`from-resource from-label`) and a target node and label
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]."
  [from-node-id from-label to-node-id to-label]
  [{:type         :connect
    :source-id    from-node-id
    :source-label from-label
    :target-id    to-node-id
    :target-label to-label}])

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a
  transaction step disconnecting a source node and label
  (`from-resource from-label`) from a target node and label
  (`to-resource to-label`). It returns a value suitable for consumption
  by [[perform]]."
  [from-node-id from-label to-node-id to-label]
  [{:type         :disconnect
    :source-id    from-node-id
    :source-label from-label
    :target-id    to-node-id
    :target-label to-label}])

(defn disconnect-sources
  [basis target-node target-label]
  (for [[src-node src-label] (gt/sources basis target-node target-label)]
    (disconnect src-node src-label target-node target-label)))

(defn label
  [label]
  [{:type  :label
    :label label}])

(defn sequence-label
  [seq-label]
  [{:type  :sequence-label
    :label seq-label}])

(defn invalidate
  [node-id]
  [{:type :invalidate
    :node-id node-id}])

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------
(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- mark-activated
  [ctx node-id input-label]
  (let [basis (:basis ctx)
        dirty-deps (-> (ig/node-by-id-at basis node-id) (gt/node-type basis) gt/input-dependencies (get input-label))]
    (update-in ctx [:nodes-affected node-id] into dirty-deps)))

(defn- next-node-id [ctx gid]
  (is/next-node-id* (:node-id-generators ctx) gid))

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

(defn- disconnect-inputs
  [ctx target-node target-label]
  (loop [ctx ctx
         sources (gt/sources (:basis ctx) target-node target-label)]
    (if-let [source (first sources)]
      (recur (perform ctx {:type         :disconnect
                           :source-id    (first source)
                           :source-label (second source)
                           :target-id    target-node
                           :target-label target-label})
        (next sources))
      ctx)))

(defn- disconnect-outputs
  [ctx source-node source-label]
  (loop [ctx ctx
         targets (gt/targets (:basis ctx) source-node source-label)]
    (if-let [target (first targets)]
      (recur (perform ctx {:type         :disconnect
                           :source-id    source-node
                           :source-label source-label
                           :target-id    (first target)
                           :target-label (second target)})
        (next targets))
      ctx)))

(defn- disconnect-stale [ctx node-id old-node new-node labels-fn disconnect-fn]
  (let [basis (:basis ctx)
        stale-labels (set/difference
                       (-> old-node (gt/node-type basis) labels-fn)
                       (-> new-node (gt/node-type basis) labels-fn))]
    (loop [ctx ctx
           labels stale-labels]
      (if-let [label (first labels)]
        (recur (disconnect-fn ctx node-id label) (rest labels))
        ctx))))

(defn- disconnect-stale-inputs [ctx node-id old-node new-node]
  (disconnect-stale ctx node-id old-node new-node gt/input-labels disconnect-inputs))

(defn- disconnect-stale-outputs [ctx node-id old-node new-node]
  (disconnect-stale ctx node-id old-node new-node gt/output-labels disconnect-outputs))

(def ^:private replace-node (comp first gt/replace-node))

(defn- activate-all-outputs
  [ctx node-id node]
  (let [all-labels (map vector (repeat node-id) (-> node (gt/node-type (:basis ctx)) gt/output-labels))]
    (update ctx :outputs-modified into all-labels)))

(defmethod perform :become
  [ctx {:keys [node-id to-node]}]
  (if-let [old-node (ig/node-by-id-at (:basis ctx) node-id)] ; nil if node was deleted in this transaction
    (let [new-node (merge to-node old-node)]
      (-> ctx
        (disconnect-stale-inputs node-id old-node new-node)
        (disconnect-stale-outputs node-id old-node new-node)
        (update :basis replace-node node-id new-node)
        (activate-all-outputs node-id new-node)))
    ctx))

(def ^:private basis-delete-node (comp first gt/delete-node))

(defn- disconnect-all-inputs [ctx node-id node]
  (reduce (fn [ctx in] (disconnect-inputs ctx node-id in)) ctx (-> node (gt/node-type (:basis ctx)) gt/input-labels)))

(defn- disconnect-all-outputs [ctx node-id node]
  (reduce (fn [ctx in] (disconnect-outputs ctx node-id in)) ctx (-> node (gt/node-type (:basis ctx)) gt/output-labels)))

(defn- delete-single
  [ctx node-id]
  (if-let [node (ig/node-by-id-at (:basis ctx) node-id)] ; nil if node was deleted in this transaction
    (let [type (gt/node-type node (:basis ctx))
          all-labels (set/union
                       (-> type gt/input-labels)
                       (-> type gt/output-labels))]
      (-> ctx
        (disconnect-all-inputs node-id node)
        (update :basis basis-delete-node node-id)
        (update-in [:nodes-affected node-id] set/union all-labels)
        (assoc-in [:nodes-deleted node-id] node)
        (update :nodes-added (partial filterv #(not= node-id %)))))
    ctx))

(defn- cascade-delete-sources
  [basis node-id]
  (for [input         (gt/cascade-deletes (gt/node-type (ig/node-by-id-at basis node-id) basis))
        [source-id _] (gt/sources basis node-id input)]
    source-id))

(defn- ctx-delete-node [ctx node-id]
  (let [basis-to-use (if (contains? (set (:nodes-added ctx)) node-id) (:basis ctx) (:original-basis ctx))
        overrides-deep (comp reverse (partial tree-seq (constantly true) (partial ig/overrides basis-to-use)))
        to-delete    (->> (ig/pre-traverse basis-to-use [node-id] cascade-delete-sources)
                       (mapcat overrides-deep))]
    (reduce delete-single ctx to-delete)))

(defmethod perform :delete-node
  [ctx {:keys [node-id]}]
  (ctx-delete-node ctx node-id))

(defmethod perform :new-override
  [ctx {:keys [override-id root-id traverse-fn]}]
  (-> ctx
    (update :basis gt/add-override override-id (ig/make-override root-id traverse-fn))))

(defn- ctx-override-node [ctx original-id override-id]
  (let [basis (:basis ctx)
        original (ig/node-by-id-at basis original-id)
        all-outputs (-> original (gt/node-type basis) gt/output-labels)]
    (-> ctx
      (update :basis gt/override-node original-id override-id)
      (update :successors-changed into (map vector (repeat original-id) all-outputs)))))

(defmethod perform :override-node
  [ctx {:keys [original-node-id override-node-id]}]
  (ctx-override-node ctx original-node-id override-node-id))

(defn- node-value [basis node-id property]
  (in/node-value node-id property {:basis basis :cache nil :skip-validation true}))

(declare apply-tx)

(defn- invoke-setter
  [ctx node-id node property old-value new-value]
  (let [ctx (mark-activated ctx node-id property)]
    (if-let [setter-fn (in/setter-for (:basis ctx) node property)]
      (-> ctx
        (update :basis ip/property-default-setter node-id property old-value new-value)
        (apply-tx (setter-fn (:basis ctx) node-id old-value new-value)))
      (-> ctx
        (update :basis ip/property-default-setter node-id property old-value new-value)
        (update-in [:properties-modified node-id] conj property)))))

(defn apply-defaults [ctx node]
  (let [node-id (gt/node-id node)]
    (loop [ctx ctx
           props (util/key-set (gt/property-types node (:basis ctx)))]
      (if-let [prop (first props)]
        (let [ctx (if-let [v (get node prop)]
                    (invoke-setter ctx node-id node prop nil v)
                    ctx)]
          (recur ctx (rest props)))
        ctx))))

(defn- merge-nodes-affected [nodes-affected node-id all-outputs]
  (merge-with set/union nodes-affected {node-id all-outputs}))

(defn- ctx-add-node [ctx node]
  (let [[basis-after full-node] (gt/add-node (:basis ctx) node)
        node-id                 (gt/node-id full-node)
        all-outputs             (-> full-node (gt/node-type basis-after) gt/output-labels)]
    (-> ctx
      (assoc :basis basis-after)
      (apply-defaults node)
      (update :nodes-added conj node-id)
      (update :successors-changed into (map vector (repeat node-id) all-outputs))
      (update :nodes-affected merge-nodes-affected node-id all-outputs))))

(defmethod perform :create-node [ctx {:keys [node]}]
  (ctx-add-node ctx node))

(defmethod perform :update-property [ctx {:keys [node-id property fn args]}]
  (let [basis (:basis ctx)]
    (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (do
        (let [old-value (node-value basis node-id property)
              new-value (apply fn old-value args)]
          (if (not= old-value new-value)
            (invoke-setter ctx node-id node property old-value new-value)
            ctx)))
      ctx)))

(defn- ctx-set-property-to-nil [ctx node-id node property]
  (let [basis (:basis ctx)
        old-value (node-value basis node property)]
    (if-let [setter-fn (in/setter-for basis node property)]
      (apply-tx ctx (setter-fn basis node-id old-value nil))
      ctx)))

(defmethod perform :clear-property [ctx {:keys [node-id property]}]
  (let [basis (:basis ctx)]
    (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
      (do
        (-> ctx
          (mark-activated node-id property)
          (update :basis replace-node node-id (gt/clear-property node basis property))
          (update-in [:properties-modified node-id] conj property)
          (ctx-set-property-to-nil node-id node property)))
      ctx)))

(defn- ctx-disconnect-single [ctx target target-id target-label]
  (if (= :one (gt/input-cardinality (gt/node-type target (:basis ctx)) target-label))
    (disconnect-inputs ctx target-id target-label)
    ctx))

(def ctx-connect)

(defn- ctx-add-overrides [ctx source-id source source-label target target-label]
  (let [basis (:basis ctx)
        target-id (gt/node-id target)]
    (if ((gt/cascade-deletes (gt/node-type target basis)) target-label)
      (loop [overrides (ig/overrides basis (gt/node-id target))
             ctx ctx]
        (if-let [or (first overrides)]
          (let [basis (:basis ctx)
                or-node (ig/node-by-id-at basis or)
                override-id (gt/override-id or-node)
                traverse-fn (ig/override-traverse-fn basis override-id)]
            (if (traverse-fn source-id source-label target-id target-label)
              (let [gid (gt/node-id->graph-id or)
                   new-sub-id (next-node-id ctx gid)
                   new-sub-node (in/make-override-node override-id new-sub-id source-id {})]
               (recur (rest overrides)
                      (-> ctx
                        (ctx-add-node new-sub-node)
                        (ctx-override-node source-id new-sub-id))))
              ctx))
          ctx))
      ctx)))

(defn- ctx-connect [ctx source-id source-label target-id target-label]
  (if-let [source (ig/node-by-id-at (:basis ctx) source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (ig/node-by-id-at (:basis ctx) target-id)] ; nil if target node was deleted in this transaction
      (-> ctx
        ; If the input has :one cardinality, disconnect existing connections first
        (ctx-disconnect-single target target-id target-label)
        (mark-activated target-id target-label)
        (update :basis gt/connect source-id source-label target-id target-label)
        (update :successors-changed conj [source-id source-label])
        (ctx-add-overrides source-id source source-label target target-label))
      ctx)
    ctx))

(defmethod perform :connect
  [ctx {:keys [source-id source-label target-id target-label] :as tx-data}]
  (ctx-connect ctx source-id source-label target-id target-label))

(defn- ctx-remove-overrides [ctx source source-label target target-label]
  (let [basis (:basis ctx)]
    (if ((gt/cascade-deletes (gt/node-type target basis)) target-label)
      (let [source-id (gt/node-id source)
            target-id (gt/node-id target)
            src-or-nodes (map (partial ig/node-by-id-at basis) (ig/overrides basis source-id))]
        (loop [tgt-overrides (ig/overrides basis target-id)
               ctx ctx]
          (if-let [or (first tgt-overrides)]
            (let [basis (:basis ctx)
                  or-node (ig/node-by-id-at basis or)
                  override-id (gt/override-id or-node)
                  tgt-ors (filter #(= override-id (gt/override-id %)) src-or-nodes)
                  traverse-fn (ig/override-traverse-fn basis override-id)
                  to-delete (ig/pre-traverse-sources basis (mapv gt/node-id tgt-ors) traverse-fn)]
              (recur (rest tgt-overrides)
                     (reduce ctx-delete-node ctx to-delete)))
            ctx)))
      ctx)))

(defn- ctx-disconnect [ctx source-id source-label target-id target-label]
  (if-let [source (ig/node-by-id-at (:basis ctx) source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (ig/node-by-id-at (:basis ctx) target-id)] ; nil if target node was deleted in this transaction
      (-> ctx
        (mark-activated target-id target-label)
        (update :basis gt/disconnect source-id source-label target-id target-label)
        (update :successors-changed conj [source-id source-label])
        (ctx-remove-overrides source source-label target target-label))
      ctx)
    ctx))

(defmethod perform :disconnect
  [ctx {:keys [source-id source-label target-id target-label]}]
  (ctx-disconnect ctx source-id source-label target-id target-label))

(defmethod perform :update-graph-value
  [ctx {:keys [gid fn args]}]
  (-> ctx
      (update-in [:basis :graphs gid :graph-values] #(apply fn % args))
      (update :graphs-modified conj gid)))

(defmethod perform :label
  [ctx {:keys [label]}]
  (assoc ctx :label label))

(defmethod perform :sequence-label
  [ctx {:keys [label]}]
  (assoc ctx :sequence-label label))

(defmethod perform :invalidate
  [ctx {:keys [node-id] :as tx-data}]
  (if-let [node (ig/node-by-id-at (:basis ctx) node-id)]
    (update ctx :nodes-affected merge-nodes-affected node-id (-> node (gt/node-type (:basis ctx)) gt/output-labels))
    ctx))

(defn- apply-tx
  [ctx actions]
  (loop [ctx     ctx
         actions actions]
    (if-let [action (first actions)]
      (if (sequential? action)
        (recur (apply-tx ctx action) (next actions))
        (recur
          (update
            (try (perform ctx action)
              (catch Exception e
                (when *tx-debug*
                  (println (txerrstr ctx "Transaction failed on " action)))
                (throw e)))
            :completed conj action) (next actions)))
      ctx)))

(defn- last-seen-node
  [{:keys [basis nodes-deleted]} node-id]
  (or
    (ig/node-by-id-at basis node-id)
    (get nodes-deleted node-id)))

(defn- mark-outputs-modified
  [{:keys [nodes-affected] :as ctx}]
  (update-in ctx [:outputs-modified] #(set/union % (pairs nodes-affected))))

(defn- mark-nodes-modified
  [{:keys [nodes-affected properties-affected] :as ctx}]
  (update ctx :nodes-modified #(set/union % (keys nodes-affected) (keys properties-affected))))

(defn map-vals-bargs
  [m f]
  (util/map-vals f m))

(defn- apply-tx-label
  [{:keys [basis label sequence-label] :as ctx}]
  (cond-> (update-in ctx [:basis :graphs] map-vals-bargs #(assoc % :tx-sequence-label sequence-label))

    label
    (update-in [:basis :graphs] map-vals-bargs #(assoc % :tx-label label))))

(def tx-report-keys [:basis :graphs-modified :nodes-added :nodes-modified :nodes-deleted :outputs-modified :properties-modified :label :sequence-label])

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph."
  [{:keys [nodes-modified graphs-modified] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (empty? (:completed ctx)) :empty :ok)
             :graphs-modified (into graphs-modified (map gt/node-id->graph-id nodes-modified)))))

(defn new-transaction-context
  [basis node-id-generators]
  {:basis               basis
   :original-basis      basis
   :nodes-affected      {}
   :nodes-added         []
   :nodes-modified      #{}
   :nodes-deleted       {}
   :outputs-modified    []
   :graphs-modified     #{}
   :properties-modified {}
   :successors-changed  #{}
   :node-id-generators node-id-generators
   :completed           []
   :txid                (new-txid)})

(defn update-successors
  [{:keys [successors-changed] :as ctx}]
  (update ctx :basis ig/update-successors successors-changed))

(defn- trace-dependencies
  [ctx]
  ;; at this point, :outputs-modified contains [node-id output] pairs.
  ;; afterwards, it will have the transitive closure of all [node-id output] pairs
  ;; reachable from the original collection.
  (update ctx :outputs-modified #(gt/dependencies (:basis ctx) %)))

(defn transact*
  [ctx actions]
  (when *tx-debug*
    (println (txerrstr ctx "actions" (seq actions))))
  (-> ctx
      (apply-tx actions)
      mark-outputs-modified
      mark-nodes-modified
      update-successors
      trace-dependencies
      apply-tx-label
      finalize-update))

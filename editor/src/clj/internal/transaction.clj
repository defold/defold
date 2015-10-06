(ns internal.transaction
  "Internal functions that implement the transactional behavior."
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [internal.node :as in]
            [internal.property :as ip]))

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
  [{:keys [basis] :as ctx} node-id input-label]
  (let [dirty-deps (-> (ig/node-by-id-at basis node-id) gt/node-type gt/input-dependencies (get input-label))]
    (update-in ctx [:nodes-affected node-id] into dirty-deps)))

(defn- activate-all-outputs
  [{:keys [basis] :as ctx} node-id node]
  (let [all-labels  (-> node gt/node-type gt/output-labels)
        ctx         (update-in ctx [:nodes-affected node-id] into all-labels)
        all-targets (into #{[node-id nil]} (mapcat #(gt/targets basis node-id %) all-labels))]
    (reduce
     (fn [ctx [target-id target-label]]
       (mark-activated ctx target-id target-label))
     ctx
     all-targets)))

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
  (loop [ctx     ctx
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
         targets     (gt/targets (:basis ctx) source-node source-label)]
    (if-let [target (first targets)]
      (recur (perform ctx {:type         :disconnect
                           :source-id    source-node
                           :source-label source-label
                           :target-id    (first target)
                           :target-label (second target)})
             (next targets))
      ctx)))

(defmethod perform :become
  [{:keys [basis nodes-affected] :as ctx}
   {:keys [node-id to-node]}]
  (if-let [old-node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (let [old-node         (ig/node-by-id-at basis node-id)
          to-node-id       (gt/node-id to-node)
          new-node         (merge to-node old-node)

          ;; disconnect inputs that no longer exist
          vanished-inputs  (set/difference (-> old-node gt/node-type gt/input-labels)
                                           (-> new-node gt/node-type gt/input-labels))
          ctx              (reduce (fn [ctx in]  (disconnect-inputs ctx node-id  in))  ctx vanished-inputs)

          ;; disconnect outputs that no longer exist
          vanished-outputs (set/difference (-> old-node gt/node-type gt/output-labels)
                                           (-> new-node gt/node-type gt/output-labels))
          ctx              (reduce (fn [ctx out] (disconnect-outputs ctx node-id out)) ctx vanished-outputs)

          [basis-after _]  (gt/replace-node (:basis ctx) node-id new-node)]
      (assoc (activate-all-outputs ctx node-id new-node)
             :basis           basis-after))
    ctx))

(defn- delete-single
  [{:keys [basis nodes-deleted nodes-added] :as ctx} node-id]
  (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (let [all-labels         (set/union (-> node gt/node-type gt/input-labels) (-> node gt/node-type gt/output-labels))
          ctx                (reduce (fn [ctx in]  (disconnect-inputs ctx node-id  in)) ctx (-> node gt/node-type gt/input-labels))
          basis              (:basis ctx)
          [basis node]       (gt/delete-node basis node-id)
          ctx                (update-in ctx [:nodes-affected node-id] set/union all-labels)]
      (assoc ctx
             :basis            basis
             :nodes-deleted    (assoc nodes-deleted node-id node)
             :nodes-added      (util/removev #(= node-id %) nodes-added)))
    ctx))

(defn- cascade-delete-sources
  [basis node-id]
  (for [input         (gt/cascade-deletes (gt/node-type (ig/node-by-id-at basis node-id)))
        [source-id _] (gt/sources basis node-id input)]
    source-id))

(defmethod perform :delete-node
  [ctx {:keys [node-id]}]
  (let [basis-to-use (if (contains? (set (:nodes-added ctx)) node-id) (:basis ctx) (:original-basis ctx))
        to-delete    (ig/pre-traverse basis-to-use [node-id] cascade-delete-sources)]
    (reduce delete-single ctx to-delete)))

(defn- assert-has-property-label
  [node label]
  (assert (contains? node label)
          (format "Attemping to use property %s from %s, but it does not exist" label (:name (gt/node-type node)))))

(declare apply-tx)

(defn- invoke-setter
  [{:keys [basis properties-modified] :as ctx} node-id node property old-value new-value]
  (let [ctx (mark-activated ctx node-id property)]
    (if-let [setter-fn (in/setter-for node property)]
      (apply-tx ctx (setter-fn basis node-id old-value new-value))
      (-> ctx
          (mark-activated node-id property)
          (assoc
           :basis               (ip/property-default-setter basis node-id property old-value new-value)
           :properties-modified (update-in properties-modified [node-id] conj property))))))

(defn apply-defaults
  [{:keys [basis] :as ctx} node]
  (let [node-id (gt/node-id node)]
    (letfn [(use-setter [ctx prop] (if-let [v (get node prop)]
                                     (invoke-setter ctx node-id node prop nil v)
                                     ctx))]
      (reduce use-setter ctx (util/key-set (gt/property-types node))))))

(defmethod perform :create-node
  [{:keys [basis nodes-affected world-ref nodes-added successors-changed] :as ctx}
   {:keys [node]}]
  (let [[basis-after full-node] (gt/add-node basis node)
        node-id                 (gt/node-id full-node)
        all-outputs             (-> full-node gt/node-type gt/output-labels)]
    (-> ctx
        (assoc :basis basis-after)
        (apply-defaults node)
        (assoc :nodes-added        (conj nodes-added node-id)
               :successors-changed (into successors-changed (map (fn [label] [node-id label]) all-outputs))
               :nodes-affected     (merge-with set/union nodes-affected {node-id all-outputs})))))

(defmethod perform :update-property
  [{:keys [basis properties-modified] :as ctx}
   {:keys [node-id property fn args]}]
  (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (do
      (assert-has-property-label node property)
      (let [old-value (in/node-value node-id property {:basis basis :cache nil :skip-validation true})
            new-value (apply fn old-value args)]
       (if (= old-value new-value)
         ctx
         (invoke-setter ctx node-id node property old-value new-value))))
    ctx))

(defmethod perform :connect
  [{:keys [basis graphs-modified successors-changed] :as ctx}
   {:keys [source-id source-label target-id target-label] :as tx-data}]
  (if-let [source (ig/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
   (if-let [target (ig/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
     (-> ctx
         (mark-activated target-id target-label)
         (assoc
          :basis              (gt/connect basis source-id source-label target-id target-label)
          :successors-changed (conj successors-changed [source-id source-label])))
     ctx)
   ctx))

(defmethod perform :disconnect
  [{:keys [basis graphs-modified successors-changed] :as ctx}
   {:keys [source-id source-label target-id target-label]}]
  (if-let [source (ig/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (ig/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
      (-> ctx
          (mark-activated target-id target-label)
          (assoc
           :basis              (gt/disconnect basis source-id source-label target-id target-label)
           :successors-changed (conj successors-changed [source-id source-label])))
      ctx)
    ctx))

(defmethod perform :update-graph-value
  [{:keys [basis] :as ctx} {:keys [gid fn args]}]
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
  [{:keys [basis nodes-affected] :as ctx} {:keys [node-id] :as tx-data}]
  (if-let [node (ig/node-by-id-at basis node-id)]
    (assoc ctx :nodes-affected (merge-with set/union nodes-affected {node-id (-> node gt/node-type gt/output-labels)}))
    ctx))

(defn- apply-tx
  [ctx actions]
  (loop [ctx     ctx
         actions actions]
    (if-let [action (first actions)]
      (if (sequential? action)
        (recur (apply-tx ctx action) (next actions))
        (recur
         (update-in
          (try (perform ctx action)
               (catch Exception e
                 (when *tx-debug*
                   (println (txerrstr ctx "Transaction failed on " action)))
                 (throw e)))
          [:completed] conj action) (next actions)))
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
  [basis]
  {:basis               basis
   :original-basis      basis
   :nodes-affected      {}
   :nodes-added         []
   :nodes-modified      #{}
   :nodes-deleted       {}
   :graphs-modified     #{}
   :properties-modified {}
   :successors-changed  #{}
   :completed           []
   :txid                (new-txid)})

(defn update-successors*
  [basis changes]
  (if-let [change (first changes)]
    (let [new-successors (ig/index-successors basis change)
          gid            (gt/node-id->graph-id (first change))]
      (recur (assoc-in basis [:graphs gid :successors change] new-successors) (next changes)))
    basis))

(defn update-successors
  [{:keys [successors-changed] :as ctx}]
  (update ctx :basis update-successors* successors-changed))

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

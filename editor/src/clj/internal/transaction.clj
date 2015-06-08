(ns internal.transaction
  "Internal functions that implement the transactional behavior."
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.util :refer :all]
            [internal.graph :as ig]
            [internal.graph.types :as gt]))

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
  `(str (:txid ~ctx) ":" (:txpass ~ctx) " " ~@(interpose " " rest)))

(defn nid [n] (if (number? n) n (if (gt/node? n) (gt/node-id n) (if (map? n) (:_id n) n))))

;; ---------------------------------------------------------------------------
;; Building transactions
;; ---------------------------------------------------------------------------
(defn new-node
  "*transaction step* - add a node to a graph"
  [node]
  [{:type     :create-node
    :node     node}])

(defn become
  [from-node to-node]
  [{:type     :become
    :node-id  (nid from-node)
    :to-node  to-node}])

(defn delete-node
  [node]
  [{:type     :delete-node
    :node-id  (nid node)}])

(defn update-property
  "*transaction step* - Expects a node, a property label, and a
  function f (with optional args) to be performed on the current value
  of the property."
  [node pr f args]
  [{:type     :update-property
    :node-id  (nid node)
    :property pr
    :fn       f
    :args     args}])

(defn set-property
  [node pr v]
  (update-property node pr (constantly v) []))

(defn update-graph
  [gid f args]
  [{:type :update-graph
    :gid  gid
    :fn   f
    :args args}])

(defn connect
  "*transaction step* - Creates a transaction step connecting a source node and label (`from-resource from-label`) and a target node and label
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]."
  [from-node from-label to-node to-label]
  [{:type         :connect
    :source-id    (nid from-node)
    :source-label from-label
    :target-id    (nid to-node)
    :target-label to-label}])

(defn disconnect
  "*transaction step* - The reverse of [[connect]]. Creates a
  transaction step disconnecting a source node and label
  (`from-resource from-label`) from a target node and label
  (`to-resource to-label`). It returns a value suitable for consumption
  by [[perform]]."
  [from-node from-label to-node to-label]
  [{:type         :disconnect
    :source-id    (nid from-node)
    :source-label from-label
    :target-id    (nid to-node)
    :target-label to-label}])

(defn label
  [label]
  [{:type  :label
    :label label}])

(defn sequence-label
  [seq-label]
  [{:type  :sequence-label
    :label seq-label}])

;; ---------------------------------------------------------------------------
;; Executing transactions
;; ---------------------------------------------------------------------------
(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- mark-activated
  [{:keys [basis] :as ctx} node-id input-label]
  (let [dirty-deps (get (gt/input-dependencies (ig/node-by-id-at basis node-id)) input-label)]
    (update-in ctx [:nodes-affected node-id] set/union dirty-deps)))

(defn- activate-all-outputs
  [{:keys [basis] :as ctx} node-id node]
  (let [all-labels  (gt/outputs node)
        ctx         (update-in ctx [:nodes-affected node-id] set/union all-labels)
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

(defmethod perform :create-node
  [{:keys [basis triggers-to-fire nodes-affected world-ref new-event-loops nodes-added] :as ctx}
   {:keys [node]}]
  (let [[basis-after full-node] (gt/add-node basis node)
        node-id                 (gt/node-id full-node)]
    (assoc ctx
           :basis            basis-after
           :nodes-added      (conj nodes-added node-id)
           :new-event-loops  (if (gt/message-target? full-node) (conj new-event-loops node-id) new-event-loops)
           :triggers-to-fire (update triggers-to-fire node-id assoc :added [])
           :nodes-affected   (merge-with set/union nodes-affected {node-id (gt/outputs full-node)}))))

(defn- disconnect-inputs
  [ctx target-node target-label]
  (loop [ctx     ctx
         sources (gt/sources (:basis ctx) (nid target-node) target-label)]
    (if-let [source (first sources)]
      (recur (perform ctx {:type         :disconnect
                           :source-id    (nid (first source))
                           :source-label (second source)
                           :target-id    (nid target-node)
                           :target-label target-label})
             (next sources))
      ctx)))

(defn- disconnect-outputs
  [ctx source-node source-label]
  (loop [ctx ctx
         targets     (gt/targets (:basis ctx) (nid source-node) source-label)]
    (if-let [target (first targets)]
      (recur (perform ctx {:type         :disconnect
                           :source-id    (nid source-node)
                           :source-label source-label
                           :target-id    (nid (first target))
                           :target-label (second target)})
             (next targets))
      ctx)))

(defmethod perform :become
  [{:keys [basis nodes-affected new-event-loops old-event-loops] :as ctx}
   {:keys [node-id to-node]}]
  (if-let [old-node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (let [old-node         (ig/node-by-id-at basis node-id)
          to-node-id       (gt/node-id to-node)
          new-node         (merge to-node old-node)

          ;; disconnect inputs that no longer exist
          vanished-inputs  (set/difference (gt/inputs old-node) (gt/inputs new-node))
          ctx              (reduce (fn [ctx in]  (disconnect-inputs ctx node-id  in))  ctx vanished-inputs)

          ;; disconnect outputs that no longer exist
          vanished-outputs (set/difference (gt/outputs old-node) (gt/outputs new-node))
          ctx              (reduce (fn [ctx out] (disconnect-outputs ctx node-id out)) ctx vanished-outputs)

          [basis-after _]  (gt/replace-node basis node-id new-node)

          start-loop       (and      (gt/message-target? new-node)  (not (gt/message-target? old-node)))
          end-loop         (and (not (gt/message-target? new-node))      (gt/message-target? old-node))]
      (assoc (activate-all-outputs ctx node-id new-node)
             :basis           basis-after
             :new-event-loops (if start-loop (conj new-event-loops node-id)  new-event-loops)
             :old-event-loops (if end-loop   (conj old-event-loops old-node) old-event-loops)))
    ctx))

(defmethod perform :delete-node
  [{:keys [basis nodes-deleted old-event-loops nodes-added triggers-to-fire] :as ctx}
   {:keys [node-id]}]
  (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (let [all-labels         (set/union (gt/inputs node) (gt/outputs node))
          ctx                (reduce (fn [ctx in]  (disconnect-inputs ctx node-id  in))  ctx (gt/inputs node))
          basis              (:basis ctx)
          [basis node]       (gt/delete-node basis node-id)
          ctx                (update-in ctx [:nodes-affected node-id] set/union all-labels)]
      (assoc ctx
             :basis            basis
             :old-event-loops  (if (gt/message-target? node) (conj old-event-loops node) old-event-loops)
             :nodes-deleted    (assoc nodes-deleted node-id node)
             :nodes-added      (removev #(= node-id %) nodes-added)
             :triggers-to-fire (update triggers-to-fire node-id assoc :deleted [])))
    ctx))

(defmethod perform :update-property
  [{:keys [basis triggers-to-fire properties-modified] :as ctx}
   {:keys [node-id property fn args]}]
  (if-let [node (ig/node-by-id-at basis node-id)] ; nil if node was deleted in this transaction
    (let [old-value          (get node property)
          [basis-after node] (gt/update-property basis node-id property fn args)
          new-value          (get node property)]
      (if (= old-value new-value)
        ctx
        (-> ctx
            (mark-activated node-id property)
            (assoc
             :basis               basis-after
             :triggers-to-fire    (update-in triggers-to-fire [node-id :property-touched] conj property)
             :properties-modified (update-in properties-modified [node-id] conj property)))))
    ctx))

(defmethod perform :connect
  [{:keys [basis triggers-to-fire graphs-modified successors-changed] :as ctx}
   {:keys [source-id source-label target-id target-label]}]
  (if-let [source (ig/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (ig/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
      (-> ctx
          (mark-activated target-id target-label)
          (assoc
           :basis              (gt/connect basis source-id source-label target-id target-label)
           :triggers-to-fire   (update-in triggers-to-fire [target-id :input-connections] conj target-label)
           :successors-changed (conj successors-changed [source-id source-label])))
      ctx)
    ctx))

(defmethod perform :disconnect
  [{:keys [basis triggers-to-fire graphs-modified successors-changed] :as ctx}
   {:keys [source-id source-label target-id target-label]}]
  (if-let [source (ig/node-by-id-at basis source-id)] ; nil if source node was deleted in this transaction
    (if-let [target (ig/node-by-id-at basis target-id)] ; nil if target node was deleted in this transaction
      (-> ctx
          (mark-activated target-id target-label)
          (assoc
           :basis              (gt/disconnect basis source-id source-label target-id target-label)
           :triggers-to-fire   (update-in triggers-to-fire [target-id :input-connections] conj target-label)
           :successors-changed (conj successors-changed [source-id source-label])))
      ctx)
    ctx))

(defmethod perform :update-graph
  [{:keys [basis] :as ctx} {:keys [gid fn args]}]
  (-> ctx
      (update-in [:basis :graphs gid] #(apply fn % args))
      (update :graphs-modified conj gid)))

(defmethod perform :label
  [ctx {:keys [label]}]
  (assoc ctx :label label))

(defmethod perform :sequence-label
  [ctx {:keys [label]}]
  (assoc ctx :sequence-label label))

(defn- apply-tx
  [ctx actions]
  (loop [ctx     ctx
         actions actions]
    (if-let [action (first actions)]
      (if (sequential? action)
        (recur (apply-tx ctx action) (next actions))
        (recur (update-in (perform ctx action) [:completed] conj action) (next actions)))
      ctx)))

(defn- last-seen-node
  [{:keys [basis nodes-deleted]} node-id]
  (or
    (ig/node-by-id-at basis node-id)
    (get nodes-deleted node-id)))

(defn- trigger-activations
  [{:keys [triggers-to-fire] :as ctx}]
  (for [[node-id m] triggers-to-fire
        [kind args] m
        :let [node (last-seen-node ctx node-id)]
        :when node
        [l tr] (-> node gt/node-type gt/triggers (get kind) seq)]
    (if (empty? args)
      [tr node l kind]
      [tr node l kind (set args)])))

(defn- invoke-trigger
  [csub [tr & args]]
  (apply tr csub (:basis csub) args))

(defn- debug-invoke-trigger
  [csub [tr & args :as trvec]]
  (println (txerrstr csub "invoking" tr "on" (gt/node-id (first args)) "with" (rest args)))
  (println (txerrstr csub "nodes triggered" (:nodes-affected csub)))
  (println (txerrstr csub (select-keys csub [:basis :nodes-added :nodes-deleted :outputs-modified :properties-modified])))
  (invoke-trigger csub trvec))

(def ^:private fire-trigger (if *tx-debug* debug-invoke-trigger invoke-trigger))

(defn- process-triggers
  [ctx]
  (when *tx-debug*
    (println (txerrstr ctx "triggers to fire: " (:triggers-to-fire ctx)))
    (println (txerrstr ctx "trigger activations: " (pr-str (trigger-activations ctx)))))
  (update-in ctx [:pending]
             into (remove empty?
                          (mapv
                           #(fire-trigger ctx %)
                           (trigger-activations ctx)))))

(defn- mark-outputs-modified
  [{:keys [nodes-affected] :as ctx}]
  (update-in ctx [:outputs-modified] #(set/union % (pairs nodes-affected))))

(defn- mark-nodes-modified
  [{:keys [nodes-affected properties-affected] :as ctx}]
  (update ctx :nodes-modified #(set/union % (keys nodes-affected) (keys properties-affected))))

(defn- one-transaction-pass
  [ctx actions]
  (when *tx-debug*
    (println (txerrstr ctx "actions this pass " actions)))
  (-> (update-in ctx [:txpass] inc)
    (assoc :triggers-to-fire {})
    (assoc :nodes-affected {})
    (apply-tx actions)
    mark-outputs-modified
    mark-nodes-modified
    process-triggers
    (dissoc :triggers-to-fire)
    (dissoc :nodes-affected)))

(defn- exhaust-actions-and-triggers
  ([ctx]
   (exhaust-actions-and-triggers ctx maximum-retrigger-count))
  ([{[current-action & pending-actions] :pending :as ctx} retrigger-count]
   (assert (< 0 retrigger-count) (txerrstr ctx "Maximum number of trigger executions reached; probable infinite recursion."))
   (if (empty? current-action)
     ctx
     (recur
      (one-transaction-pass (assoc ctx :pending pending-actions) current-action)
      (dec retrigger-count)))))

(defn map-vals-bargs
  [m f]
  (map-vals f m))

(defn- apply-tx-label
  [{:keys [basis label sequence-label] :as ctx}]
  (cond-> (update-in ctx [:basis :graphs] map-vals-bargs #(assoc % :tx-sequence-label sequence-label))

    label
    (update-in [:basis :graphs] map-vals-bargs #(assoc % :tx-label label))))

(def tx-report-keys [:basis :new-event-loops :graphs-modified :nodes-added :nodes-modified :nodes-deleted :outputs-modified :properties-modified :label :sequence-label])

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph."
  [{:keys [nodes-modified graphs-modified] :as ctx}]
  (-> (select-keys ctx tx-report-keys)
      (assoc :status (if (empty? (:completed ctx)) :empty :ok)
             :graphs-modified (into graphs-modified (map gt/node-id->graph-id nodes-modified)))))

(defn new-transaction-context
  [basis actions]
  {:basis               basis
   :original-basis      basis
   :new-event-loops     #{}
   :old-event-loops     #{}
   :nodes-added         []
   :nodes-modified      #{}
   :nodes-deleted       {}
   :graphs-modified     #{}
   :properties-modified {}
   :successors-changed  #{}
   :pending             [actions]
   :completed           []
   :tx-id               (new-txid)
   :txpass              0})


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
  [ctx]
  (-> ctx
      exhaust-actions-and-triggers
      update-successors
      trace-dependencies
      apply-tx-label
      finalize-update))

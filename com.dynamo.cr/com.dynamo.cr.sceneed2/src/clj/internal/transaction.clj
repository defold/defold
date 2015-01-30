(ns internal.transaction
  (:require [clojure.set :as set]
            [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.bus :as bus]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [service.log :as log]))


; ---------------------------------------------------------------------------
; Configuration parameters
; ---------------------------------------------------------------------------
(def maximum-retrigger-count 50)
(def maximum-graph-coloring-recursion 1000)

; ---------------------------------------------------------------------------
; Internal state
; ---------------------------------------------------------------------------
(def ^:dynamic *scope* nil)

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn- new-cache-key
  []
  (.getAndIncrement nextkey))

; ---------------------------------------------------------------------------
; Transaction protocols
; ---------------------------------------------------------------------------
(declare ->TransactionLevel transact)

(defn- make-transaction-level
  ([receiver]
    (make-transaction-level receiver 0))
  ([receiver depth]
    (->TransactionLevel receiver depth (transient []))))

(defprotocol TransactionStarter
  (tx-begin [this]      "Create a subordinate transaction scope"))

(defprotocol Transaction
  (tx-bind  [this work] "Add a unit of work to the transaction")
  (tx-apply [this]      "Apply the transaction work"))

(defprotocol TransactionReceiver
  (tx-merge [this work] "Merge the transaction work into yourself."))

(deftype TransactionLevel [receiver depth accumulator]
  TransactionStarter
  (tx-begin [this]      (make-transaction-level this (inc depth)))

  Transaction
  (tx-bind  [this work] (conj! accumulator work))
  (tx-apply [this]      (tx-merge receiver (persistent! accumulator)))

  TransactionReceiver
  (tx-merge [this work] (tx-bind this work)
                        {:status :pending}))

(deftype TransactionSeed [world-ref]
  TransactionStarter
  (tx-begin [this]      (make-transaction-level this))

  TransactionReceiver
  (tx-merge [this work] (if (< 0 (count work))
                          (transact world-ref work)
                          {:status :empty})))

(deftype NullTransaction []
  TransactionStarter
  (tx-begin [this]      (assert false "The system is not initialized enough to run transactions yet.")))

(def ^:dynamic *transaction* (->NullTransaction))

(defn set-world-ref! [r]
  (alter-var-root #'*transaction* (constantly (->TransactionSeed r))))

; ---------------------------------------------------------------------------
; Building transactions
; ---------------------------------------------------------------------------
(defn new-node
  [node]
  [{:type :create-node
    :node node}])

(defn become
  [from-node to-node]
  [{:type     :become
    :node-id  (:_id from-node)
    :to-node  to-node}])

(defn delete-node
  [node]
  [{:type    :delete-node
    :node-id (:_id node)}])

(defn update-property
  [node pr f args]
  [{:type     :update-property
    :node-id  (:_id node)
    :property pr
    :fn       f
    :args     args}])

(defn connect
  [from-node from-label to-node to-label]
  [{:type         :connect
    :source-id    (:_id from-node)
    :source-label from-label
    :target-id    (:_id to-node)
    :target-label to-label}])

(defn disconnect
  [from-node from-label to-node to-label]
  [{:type         :disconnect
    :source-id    (:_id from-node)
    :source-label from-label
    :target-id    (:_id to-node)
    :target-label to-label}])

(defn send-message
  [to-node body]
  [{:type    :message
    :to-node (:_id to-node)
    :body    body}])

(defn label
  [label]
  [{:type  :label
    :label label}])

(defn- tempid? [x] (neg? x))
(defn has-tempid? [n] (and (:_id n) (tempid? (:_id n))))
(defn resolve-tempid [ctx x] (when x (if (pos? x) x (get (:tempids ctx) x))))

(defn node->cache-keys [n]
  (zipmap (t/cached-outputs n) (repeatedly new-cache-key)))

; ---------------------------------------------------------------------------
; Executing transactions
; ---------------------------------------------------------------------------
(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- mark-activated
  [{:keys [graph] :as ctx} node-id input-label]
  (let [dirty-deps (get (t/output-dependencies (dg/node graph node-id)) input-label)]
    (update-in ctx [:nodes-triggered node-id] set/union dirty-deps)))

(defn- activate-all-outputs
  [ctx node-id node]
  (reduce
    (fn [ctx out]
      (mark-activated ctx node-id out))
    ctx
    (concat (t/outputs node) (keys (t/properties node)))))

(defmulti perform (fn [ctx m] (:type m)))

(defmethod perform :create-node
  [{:keys [graph tempids cache-keys nodes-triggered world-ref new-event-loops nodes-added] :as ctx}
   {:keys [node]}]
  (let [next-id     (dg/next-node graph)
        full-node   (assoc node :_id next-id :world-ref world-ref) ;; TODO: can we remove :world-ref from nodes?
        graph-after (lg/add-labeled-node graph (t/inputs node) (t/outputs node) full-node)
        full-node   (dg/node graph-after next-id)]
    (assoc ctx
      :graph               graph-after
      :nodes-added         (conj nodes-added next-id)
      :tempids             (assoc tempids (:_id node) next-id)
      :cache-keys          (assoc cache-keys next-id (node->cache-keys full-node))
      :new-event-loops     (if (satisfies? t/MessageTarget full-node) (conj new-event-loops next-id) new-event-loops)
      :nodes-triggered     (merge-with set/union nodes-triggered {next-id (t/outputs full-node)}))))

(defn- disconnect-inputs
  [ctx target-node target-label]
  (reduce
    (fn [ctx [in-node in-label]]
      (perform ctx {:type         :disconnect
                    :source-id    in-node
                    :source-label in-label
                    :target-id    (:_id target-node)
                    :target-label target-label}))
    ctx
    (lg/sources (:graph ctx) (:_id target-node) target-label)))

(defn- disconnect-outputs
  [ctx source-node source-label]
  (reduce
    (fn [ctx [target-node target-label]]
      (perform ctx {:type         :disconnect
                    :source-id    source-node
                    :source-label source-label
                    :target-id    (:_id target-node)
                    :target-label target-label}))
    ctx
    (lg/targets (:graph ctx) (:_id source-node) source-label)))

(defmethod perform :become
  [{:keys [graph tempids cache-keys nodes-triggered world-ref new-event-loops obsolete-cache-keys old-event-loops] :as ctx}
   {:keys [node-id to-node]}]
  (let [old-node         (dg/node graph node-id)
        to-node-id       (:_id to-node)
        new-node         (merge to-node old-node)

        ;; disconnect inputs that no longer exist
        vanished-inputs  (set/difference (t/inputs old-node) (t/inputs new-node))
        ctx              (reduce (fn [ctx in]  (disconnect-inputs ctx node-id  in))  ctx vanished-inputs)

        ;; disconnect outputs that no longer exist
        vanished-outputs (set/difference (t/outputs old-node) (t/outputs new-node))
        ctx              (reduce (fn [ctx out] (disconnect-outputs ctx node-id out)) ctx vanished-outputs)

        graph            (dg/transform-node graph node-id (constantly new-node))

        start-loop       (and      (satisfies? t/MessageTarget new-node)  (not (satisfies? t/MessageTarget old-node)))
        end-loop         (and (not (satisfies? t/MessageTarget new-node))      (satisfies? t/MessageTarget old-node))]
    (assoc (activate-all-outputs ctx node-id new-node)
      :graph               graph
      :tempids             (if (tempid? to-node-id) (assoc tempids to-node-id node-id) tempids)
      :new-event-loops     (if start-loop (conj new-event-loops node-id)  new-event-loops)
      :old-event-loops     (if end-loop   (conj old-event-loops old-node) old-event-loops)
      :obsolete-cache-keys (concat obsolete-cache-keys (vals (get cache-keys node-id)))
      :cache-keys          (assoc cache-keys node-id (node->cache-keys to-node)))))

(defmethod perform :delete-node
  [{:keys [graph nodes-deleted old-event-loops] :as ctx} {:keys [node-id]}]
  (when-not (dg/node graph (resolve-tempid ctx node-id))
    (prn :delete-node "Can't locate node for ID " node-id))
  (let [node-id     (resolve-tempid ctx node-id)
        node        (dg/node graph node-id)
        ctx         (activate-all-outputs ctx node-id node)]
    (assoc ctx
      :graph           (dg/remove-node graph node-id)
      :old-event-loops (if (satisfies? t/MessageTarget node) (conj old-event-loops node) old-event-loops)
      :nodes-deleted   (assoc nodes-deleted node-id node))))

(defmethod perform :update-property
  [{:keys [graph] :as ctx} {:keys [node-id property fn args]}]
  (let [node-id   (resolve-tempid ctx node-id)
        old-value (get (dg/node graph node-id) property)
        new-graph (apply dg/transform-node graph node-id update-in [property] fn args)
        new-value (get (dg/node new-graph node-id) property)]
    (if (= old-value new-value)
      ctx
      (-> ctx
         (mark-activated node-id property)
         (assoc :graph new-graph)))))

(defmethod perform :connect
  [{:keys [graph] :as ctx} {:keys [source-id source-label target-id target-label]}]
  (let [source-id (resolve-tempid ctx source-id)
        target-id (resolve-tempid ctx target-id)]
    (-> ctx
        (mark-activated target-id target-label)
        (assoc :graph (lg/connect graph source-id source-label target-id target-label)))))

(defmethod perform :disconnect
  [{:keys [graph] :as ctx} {:keys [source-id source-label target-id target-label]}]
  (let [source-id (resolve-tempid ctx source-id)
        target-id (resolve-tempid ctx target-id)]
    (-> ctx
        (mark-activated target-id target-label)
        (assoc :graph (lg/disconnect graph source-id source-label target-id target-label)))))

(defmethod perform :message
  [ctx {:keys [to-node body]}]
  (let [target-id (resolve-tempid ctx to-node)]
    (update-in ctx [:messages] conj (bus/address-to target-id body))))

(defmethod perform :label
  [ctx {:keys [label]}]
  (assoc ctx :label label))

(defn- apply-tx
  [ctx actions]
  (reduce
    (fn [ctx action]
      (cond
        (sequential? action) (apply-tx ctx action)
        :else                (-> ctx
                                 (perform action)
                                 (update-in [:completed] conj action))))
    ctx
    actions))

(defn- pairwise [f coll]
  (for [n coll
        x (f n)]
    [n x]))

(defn- downstream-activation
  [{:keys [graph] :as ctx} outputs]
  (->> (pairs outputs)
       (mapcat #(apply lg/targets graph %))
       (reduce #(apply mark-activated %1 %2) ctx)))

(defn- trace-trigger-activation
  ([ctx]
    (trace-trigger-activation ctx (:nodes-triggered ctx) maximum-graph-coloring-recursion))
  ([ctx next-batch iterations-remaining]
    (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
    (let [new-ctx    (downstream-activation ctx next-batch)
          next-batch (map-diff (:nodes-triggered new-ctx) (:nodes-triggered ctx) set/difference)]
      (if (empty? next-batch)
        new-ctx
        (recur new-ctx next-batch (dec iterations-remaining))))))

(defn- determine-obsoletes
  [{:keys [graph obsolete-cache-keys outputs-modified cache-keys] :as ctx}]
  (assoc ctx :obsolete-cache-keys
    (concat obsolete-cache-keys
      (keep identity (map #(get-in cache-keys %) (pairs outputs-modified))))))

(defn- dispose-obsoletes
  [{:keys [cache obsolete-cache-keys nodes-deleted] :as ctx}]
  (let [candidates (concat
                     (filter t/disposable? (map #(get cache %) obsolete-cache-keys))
                     (filter t/disposable? (vals nodes-deleted)))]
    (assoc ctx :values-to-dispose (keep identity candidates))))

(defn- evict-obsolete-caches
  [{:keys [obsolete-cache-keys] :as ctx}]
  (update-in ctx [:cache] (fn [c] (reduce cache/evict c obsolete-cache-keys))))

(defn- determine-autoupdates
  [{:keys [graph outputs-modified] :as ctx}]
  (update-in ctx [:expired-outputs] concat
             (doall
               (for [[n vs] outputs-modified
                    v vs
                    :let [node (dg/node graph n)]
                    :when (and node (contains? (t/auto-update-outputs node) v))]
                [node v]))))

(deftype TriggerReceiver [transaction-context]
  TransactionReceiver
  (tx-merge [this work]
    (cond-> transaction-context
      (seq work)
      (update-in [:pending] conj work))))

(def ^:private trigger-ordering
  [:added :modified :deleted])

(def ^:private context-trigger-keys
  {:added    :nodes-added
   :modified :outputs-modified
   :deleted  :nodes-deleted})

(defn- node-triggers
  [node]
  (-> node t/node-type t/triggers (select-keys trigger-ordering)))

(defn- all-triggers
  [all-activated]
  (for [kind    trigger-ordering
       n        all-activated
       [l tr]   (-> n t/node-type t/triggers (get kind) seq)]
   [tr n l kind]))

(defn should-trigger?
  [ctx node kind]
  (contains? (get ctx (context-trigger-keys kind)) (:_id node)))

(defn activated-triggers
  [ctx triggers]
  (filter (fn [[tr n l k]] (should-trigger? ctx n k)) triggers))

(defn- process-triggers
  [{:keys [graph nodes-triggered nodes-added outputs-modified nodes-deleted] :as ctx}]
  (let [all-activated  (concat (filter identity (map #(dg/node graph %) (keys nodes-triggered))) (vals nodes-deleted))
        invoke-trigger (fn [csub [tr & args]]
                         (binding [*transaction* (make-transaction-level (->TriggerReceiver csub))]
                           (apply tr csub (:graph csub) args)
                           (tx-apply *transaction*)))
        trigger-ctx    (-> ctx
                         (update-in [:nodes-added]      set/intersection (into #{} (keys nodes-triggered)))
                         (update-in [:nodes-deleted]    select-keys (keys nodes-triggered))
                         (update-in [:outputs-modified] select-keys (keys nodes-triggered)))
        trigger-ctx    (reduce invoke-trigger trigger-ctx (activated-triggers trigger-ctx (all-triggers all-activated)))]
    (assoc ctx :pending (:pending trigger-ctx))))

(defn- mark-outputs-modified
  [ctx]
  (update-in ctx [:outputs-modified] #(merge-with set/union % (:nodes-triggered ctx))))

(defn- one-transaction-pass
  [ctx actions]
  (-> (assoc ctx :nodes-triggered {})
    (apply-tx actions)
    trace-trigger-activation
    mark-outputs-modified
    process-triggers
    (dissoc :nodes-triggered)))

(defn- exhaust-actions-and-triggers
  ([ctx]
    (exhaust-actions-and-triggers ctx maximum-retrigger-count))
  ([{[current-action & pending-actions] :pending :as ctx} retrigger-count]
    (assert (< 0 retrigger-count) "Maximum number of trigger executions reached; probable infinite recursion.")
    (if (empty? current-action)
      ctx
      (recur (one-transaction-pass (assoc ctx :pending pending-actions) current-action) (dec retrigger-count)))))

(def tx-report-keys [:status :expired-outputs :values-to-dispose :new-event-loops :tempids :graph :nodes-added :nodes-deleted :outputs-modified :label])

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph.
   Likewise for cache and cache-keys."
  [{:keys [graph cache cache-keys world-time] :as ctx}]
  (let [empty-tx?  (empty? (:completed ctx))
        status     (if empty-tx? :empty :ok)
        world-time (if empty-tx? world-time (inc world-time))]
    (update-in ctx [:world] assoc
      :graph      graph
      :cache      cache
      :cache-keys cache-keys
      :world-time world-time
      :last-tx    (assoc (select-keys ctx tx-report-keys) :status status))))

(defn- new-transaction-context
  [world-ref actions]
  (let [current-world @world-ref]
    {:world-ref           world-ref
     :world               current-world
     :graph               (:graph current-world)
     :cache               (:cache current-world)
     :cache-keys          (:cache-keys current-world)
     :world-time          (:world-time current-world)
     :expired-outputs     []
     :tempids             {}
     :new-event-loops     #{}
     :old-event-loops     #{}
     :nodes-added         #{}
     :nodes-modified      #{}
     :nodes-deleted       {}
     :messages            []
     :pending             [actions]
     :completed           []}))

(defn start-event-loop!
  [world-ref id]
  (let [in (bus/subscribe (:message-bus @world-ref) id)]
    (binding [*transaction* (->TransactionSeed world-ref)]
      (a/go-loop []
        (when-let [msg (a/<! in)]
          (when (not= ::stop-event-loop (:type msg))
            (try
              (let [n (dg/node (:graph @world-ref) id)]
                (when-not n
                  (log/error :message "Nil node in event loop"  :id id :chan-msg msg)))
              (t/process-one-event (dg/node (:graph @world-ref) id) msg)
              (catch Exception ex
                (log/error :message "Error in node event loop" :exception ex)))
            (recur)))))))

(defn stop-event-loop!
  [world-ref {:keys [_id]}]
  (bus/publish (:message-bus @world-ref) (bus/address-to _id {:type ::stop-event-loop})))

(defn- transact*
  [world-ref ctx]
  (dosync
    (let [txr (-> ctx
                 exhaust-actions-and-triggers
                 determine-obsoletes
                 dispose-obsoletes
                 evict-obsolete-caches
                 determine-autoupdates
                 finalize-update)]
      (ref-set world-ref (:world txr))
      txr)))

(defn transact
  [world-ref txs]
  (let [{:keys [world messages new-event-loops old-event-loops] :as tx-result} (transact* world-ref (new-transaction-context world-ref txs))]
    (doseq [l old-event-loops]
      (stop-event-loop! world-ref l))
    (doseq [l new-event-loops]
      (start-event-loop! world-ref l))
    (when (= :ok (-> world :last-tx :status))
      (a/onto-chan (:disposal-queue world) (-> world :last-tx :values-to-dispose) false)
      (bus/publish-all (:message-bus world) messages))
    (:last-tx world)))

; ---------------------------------------------------------------------------
; Documentation
; ---------------------------------------------------------------------------
(doseq [[v doc]
       {*ns*
        "Internal functions that implement the transactional behavior."

        #'perform
        "A multimethod used for defining methods that perform the individual actions within a
transaction. This is for internal use, not intended to be extended by applications.

Perform takes a transaction context (ctx) and a map (m) containing a value for keyword `:type`, and other keys and
values appropriate to the transformation it represents. Callers should regard the map and context as opaque.

Calls to perform are only executed by [[transact]]. The data required for `perform` calls are constructed in action functions,
such as [[connect]] and [[update-property]]."

        #'connect
        "*transaction step* - Creates a transaction step connecting a source node and label (`from-resource from-label`) and a target node and label
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]. Nodes passed to `connect` may have tempids."

        #'disconnect
        "*transaction step* - The reverse of [[connect]]. Creates a transaction step disconnecting a source node and label
(`from-resource from-label`) from a target node and label
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]. Nodes passed to `disconnect` may be tempids."

        #'new-node
        "*transaction step* - creates a resource in the project. Expects a node. May include an `:_id` key containing a
tempid if the resource will be referenced again in the same transaction. If supplied, _input_ and _output_ are sets of input and output labels, respectively.
If not supplied as arguments, the `:input` and `:output` keys in the node may will be assigned as the resource's inputs and outputs."

        #'update-property
        "*transaction step* - Expects a node, a property label, and a function f (with optional args) to be performed on the
current value of the property. The node may be a uncommitted, in which case it will have a tempid."

        #'transact
        "Execute a transaction to create a new version of the world's state. This takes in
a ref to the current world state, modifies it according to the transaction steps in txs,
and returns a new world.

The txs must have been created by the transaction step functions in this namespace: [[connect]],
[[disconnect]], [[new-node]], and [[update-property]]. The collection of txs can be nested."
}]
  (alter-meta! v assoc :doc doc))

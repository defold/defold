(ns internal.transaction
  (:require [clojure.set :as set]
            [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.types :as t]
            [internal.bus :as bus]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [service.log :as log]))

(def ^:dynamic *scope* nil)

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn- new-cache-key
  []
  (.getAndIncrement nextkey))

; ---------------------------------------------------------------------------
; Transaction protocols
; ---------------------------------------------------------------------------

(defprotocol Transaction
  (tx-bind       [this step]  "Add a step to the transaction")
  (tx-apply      [this]       "Apply the transaction steps")
  (tx-begin      [this world] "Create a subordinate transaction scope"))

(defprotocol Return
  (tx-return     [this]       "Return the steps of the transaction."))

(declare ->NestedTransaction transact)

(deftype NestedTransaction [enclosing]
  Transaction
  (tx-bind [this step]        (tx-bind enclosing step))
  (tx-apply [this]            nil)
  (tx-begin [this _]          (->NestedTransaction this)))

(deftype RootTransaction [world-ref steps]
  Transaction
  (tx-bind [this step]       (conj! steps step))
  (tx-apply [this]           (when (< 0 (count steps))
                               (transact world-ref (persistent! steps))))
  (tx-begin [this _]         (->NestedTransaction this)))

(deftype NullTransaction []
  Transaction
  (tx-bind [this step]       (assert false "This must be done inside a (transactional) block."))
  (tx-apply [this]           nil)
  (tx-begin [this world-ref] (->RootTransaction world-ref (transient []))))

(deftype TriggerTransaction [steps]
  Transaction
  (tx-bind [this step]       (conj! steps step))
  (tx-apply [this]           nil)
  (tx-begin [this _]         (->NestedTransaction this))

  Return
  (tx-return [this]          (persistent! steps)))

(def ^:dynamic *transaction* (->NullTransaction))

; ---------------------------------------------------------------------------
; Building transactions
; ---------------------------------------------------------------------------
(defn new-node
  ([node]
    (new-node node (set (keys (:inputs node))) (set (keys (:transforms node)))))
  ([node inputs outputs]
    [{:type    :create-node
      :node    node
      :inputs  inputs
      :outputs outputs}]))

(defn set-property
  [node pr val]
  [{:type     :set-property
    :node-id  (:_id node)
    :property pr
    :value    val}])

(defn update-property
  [node pr f args]
  [{:type     :update-property
    :node-id  (:_id node)
    :property pr
    :fn       f
    :args     args}])

(defn connect
  [from-node from-label to-node to-label]
  [{:type          :connect
    :source-id    (:_id from-node)
    :source-label from-label
    :target-id    (:_id to-node)
    :target-label to-label}])

(defn disconnect
  [from-node from-label to-node to-label]
  [{:type          :disconnect
    :source-id    (:_id from-node)
    :source-label from-label
    :target-id    (:_id to-node)
    :target-label to-label}])

(defn send-message
  [to-node body]
  [{:type    :message
    :to-node (:_id to-node)
    :body    body}])

(defn has-tempid? [n] (and (:_id n) (neg? (:_id n))))
(defn resolve-tempid [ctx x] (if (pos? x) x (get (:tempids ctx) x)))

(defn node->cache-keys [n]
  (zipmap (t/cached-outputs n) (repeatedly new-cache-key)))

; ---------------------------------------------------------------------------
; Executing transactions
; ---------------------------------------------------------------------------
(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- mark-dirty
  [{:keys [graph] :as ctx} node-id input-label]
  (let [dirty-deps (get (t/output-dependencies (dg/node graph node-id)) input-label)]
    (if (empty? dirty-deps)
      ctx
      (update-in ctx [:outputs-modified node-id] into dirty-deps))))

(defmulti perform (fn [ctx m] (:type m)))

(defmethod perform :create-node
  [{:keys [graph tempids cache-keys nodes-modified outputs-modified world-ref new-event-loops nodes-added] :as ctx}
   {:keys [node] :as m}]
  (let [next-id     (dg/next-node graph)
        full-node   (assoc node :_id next-id :world-ref world-ref)
        graph-after (lg/add-labeled-node graph (t/inputs node) (t/outputs node) full-node)
        full-node   (dg/node graph-after next-id)]
    (assoc ctx
      :graph               graph-after
      :nodes-added         (conj nodes-added full-node)
      :tempids             (assoc tempids (:_id node) next-id)
      :cache-keys          (assoc cache-keys next-id (node->cache-keys full-node))
      :new-event-loops     (if (satisfies? t/MessageTarget full-node) (conj new-event-loops next-id) new-event-loops)
      :outputs-modified    (merge-with concat outputs-modified {next-id (into [] (t/outputs full-node))})
      :nodes-modified      (conj nodes-modified next-id))))

(defmethod perform :set-property
  [{:keys [graph nodes-modified] :as ctx} {:keys [node-id property value] :as m}]
  (let [node-id (resolve-tempid ctx node-id)]
    (-> ctx
        (mark-dirty node-id property)
        (assoc :graph            (dg/transform-node graph node-id assoc property value)
               :nodes-modified   (conj nodes-modified node-id)))))

(defmethod perform :update-property
  [{:keys [graph nodes-modified] :as ctx} {:keys [node-id property fn args] :as m}]
  (let [node-id (resolve-tempid ctx node-id)]
    (-> ctx
        (mark-dirty node-id property)
        (assoc :graph            (apply dg/transform-node graph node-id update-in [property] fn args)
               :nodes-modified   (conj nodes-modified node-id)))))

(defmethod perform :connect
  [{:keys [graph nodes-modified] :as ctx} {:keys [source-id source-label target-id target-label] :as m}]
  (let [source-id (resolve-tempid ctx source-id)
        target-id (resolve-tempid ctx target-id)]
    (-> ctx
        (mark-dirty target-id target-label)
        (assoc :graph            (lg/connect graph source-id source-label target-id target-label)
               :nodes-modified   (conj nodes-modified target-id)))))

(defmethod perform :disconnect
  [{:keys [graph nodes-modified] :as ctx} {:keys [source-id source-label target-id target-label] :as m}]
  (let [source-id (resolve-tempid ctx source-id)
        target-id (resolve-tempid ctx target-id)]
    (-> ctx
        (mark-dirty target-id target-label)
        (assoc :graph            (lg/disconnect graph source-id source-label target-id target-label)
               :nodes-modified   (conj nodes-modified target-id)))))

(defmethod perform :message
  [ctx {:keys [to-node body]}]
  (update-in ctx [:messages] conj (bus/address-to to-node body)))

(defn- apply-tx
  [ctx actions]
  (reduce
    (fn [ctx action]
      (cond
        (sequential? action) (apply-tx ctx action)
        :else                (perform ctx action)))
    ctx
    actions))

(defn- pairwise [f coll]
  (for [n coll
        x (f n)]
    [n x]))

(defn- downstream-dirties
  [{:keys [graph] :as ctx} outputs]
  (->> (pairs outputs)
       (mapcat #(apply lg/targets graph %))
       (reduce #(apply mark-dirty %1 %2) ctx)
       :outputs-modified))

(defn- trace-dirty-outputs
  [ctx]
  (loop [ctx                  ctx
         next-batch           (:outputs-modified ctx)
         iterations-remaining 1000]
    (let [next-batch (downstream-dirties (select-keys ctx [:graph]) next-batch)
          ctx        (update-in ctx [:outputs-modified] #(merge-with into % next-batch))]
      (if (empty? next-batch)
        ctx
        (do
          (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
          (recur ctx next-batch (dec iterations-remaining)))))))

(defn- determine-obsoletes
  [{:keys [graph outputs-modified cache-keys] :as ctx}]
  (assoc ctx :obsolete-cache-keys
         (keep identity (map #(get-in cache-keys %) (pairs outputs-modified)))))

(defn- dispose-obsoletes
  [{:keys [cache obsolete-cache-keys] :as ctx}]
  (assoc ctx :values-to-dispose (keep identity (filter t/disposable? (map #(get cache %) obsolete-cache-keys)))))

(defn- evict-obsolete-caches
  [{:keys [obsolete-cache-keys] :as ctx}]
  (update-in ctx [:cache] (fn [c] (reduce cache/evict c obsolete-cache-keys))))

(defn- determine-autoupdates
  [{:keys [graph outputs-modified] :as ctx}]
  (update-in ctx [:expired-outputs] concat
             (for [[n vs] outputs-modified
                   v vs
                   :let [node (dg/node graph n)]
                   :when (t/auto-update? node v)]
               [node v])))

(defn start-event-loop!
  [world-ref graph id]
  (let [in (bus/subscribe (:message-bus @world-ref) id)]
    (binding [*transaction* (->NullTransaction)]
      (a/go-loop []
        (when-let [msg (a/<! in)]
          (try
            (t/process-one-event (dg/node graph id) msg)
            (catch Exception ex
              (log/error :message "Error in node event loop" :exception ex)))
          (recur))))))

(defn- start-event-loops
  [{:keys [world-ref graph new-event-loops previously-started] :as ctx}]
  (let [loops-to-start (set/difference new-event-loops previously-started)]
    (doseq [l loops-to-start]
      (start-event-loop! world-ref graph l))
  (update-in ctx [:previously-started] set/union loops-to-start)))

(defn- process-triggers
  [{:keys [graph outputs-modified previously-triggered] :as ctx}]
  (let [new-triggers (set/difference (into #{} (keys outputs-modified)) previously-triggered)
        next-ctx (reduce (fn [csub [n tr]]
                           (binding [*transaction* (->TriggerTransaction (transient []))]
                             (tr (:graph csub) n csub)
                             (update-in csub [:pending] conj (tx-return *transaction*))))
                   ctx
                   (pairwise :triggers (map #(dg/node graph %) new-triggers)))]
    (update-in next-ctx [:previously-triggered] set/union new-triggers)))

(defn- transact*
  [ctx]
  (let [tx-list (first (:pending ctx))]
    (-> (update-in ctx [:pending] next)
      (apply-tx tx-list)
      trace-dirty-outputs
      determine-obsoletes
      dispose-obsoletes
      evict-obsolete-caches
      determine-autoupdates
      start-event-loops
      process-triggers)))

(defn- new-transaction-context
  [world-ref txs]
  (let [current-world @world-ref]
    {:world-ref           world-ref
     :world               current-world
     :graph               (:graph current-world)
     :cache               (:cache current-world)
     :cache-keys          (:cache-keys current-world)
     :outputs-modified    {}
     :world-time          (:world-time current-world)
     :expired-outputs     []
     :tempids             {}
     :new-event-loops     #{}
     :nodes-added         #{}
     :nodes-modified      #{}
     :messages            []
     :pending             [txs]}))

(def tx-report-keys [:status :expired-outputs :values-to-dispose :new-event-loops :tempids :graph :txs :nodes-added :outputs-modified])

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph.
   Likewise for cache and cache-keys."
  [{:keys [graph cache cache-keys world-time] :as ctx} completed?]
  (if-not completed?
    (update-in ctx [:world] assoc :last-tx {:status :aborted})
    (update-in ctx [:world] assoc
            :graph      graph
            :cache      cache
            :cache-keys cache-keys
            :world-time (inc world-time)
            :last-tx    (assoc (select-keys ctx tx-report-keys) :status :ok))))

(defn- run-to-completion
  [w ctx depth]
  (if (and (seq (:pending ctx)) (< depth 50))
    (recur w (transact* ctx) (inc depth))
    (finalize-update ctx (< depth 50))))

(defn transact
  [world-ref txs]
  (dosync
    (let [{:keys [world messages]} (run-to-completion @world-ref (new-transaction-context world-ref txs) 0)]
      (ref-set world-ref world)
      (when (= :ok (-> world :last-tx :status))
        (bus/publish-all (:message-bus world) messages))
      (:last-tx world))))

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

        #'set-property
        "*transaction step* - Expects a node, a property label, and a value. That will become the new value of the property."


        #'update-property
        "*transaction step* - Expects a node, a property label, and a function f (with optional args) to be performed on the
current value of the property. The node may be a uncommitted, in which case it will have a tempid."

        #'transact
        "Execute a transaction to create a new version of the world's state. This takes in
a ref to the current world state, modifies it according to the transaction steps in txs,
and returns a new world.

The txs must have been created by the transaction step functions in this namespace: [[connect]],
[[disconnect]], [[new-node]], [[set-property]], and [[update-property]]. The collection of txs can be nested."
}]
  (alter-meta! v assoc :doc doc))

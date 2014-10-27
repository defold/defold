(ns internal.transaction
  (:require [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.resource :refer [disposable?]]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]))

(def ^:dynamic *scope* nil)

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn- new-cache-key
  []
  (.getAndIncrement nextkey))

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

(defn update-node
  [node f & args]
  [{:type    :update-node
    :node-id (:_id node)
    :fn      f
    :args    args}])

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

(defn resolve-tempid [ctx x] (if (pos? x) x (get (:tempids ctx) x)))

(defn resource->cache-keys [n]
  (zipmap (t/cached-outputs n) (repeatedly new-cache-key)))

; ---------------------------------------------------------------------------
; Executing transactions
; ---------------------------------------------------------------------------
(defmulti perform (fn [ctx m] (:type m)))

(defmethod perform :create-node
  [{:keys [graph tempids cache-keys modified-nodes world world-ref new-event-loops] :as ctx} m]
  (let [next-id (dg/next-node graph)]
    (assoc ctx
      :graph           (lg/add-labeled-node graph (-> m :node :descriptor :inputs keys) (-> m :node :descriptor :transforms keys) (assoc (:node m) :_id next-id :world-ref world-ref))
      :tempids         (assoc tempids (get-in m [:node :_id]) next-id)
      :cache-keys      (assoc cache-keys next-id (resource->cache-keys (:node m)))
      :new-event-loops (if (satisfies? t/MessageTarget (:node m)) (conj new-event-loops next-id) new-event-loops)
      :modified-nodes  (conj modified-nodes next-id))))

(defmethod perform :update-node
  [{:keys [graph modified-nodes] :as ctx} m]
  (let [n (resolve-tempid ctx (:node-id m))]
    (assoc ctx
           :graph          (apply dg/transform-node graph n (:fn m) (:args m))
           :modified-nodes (conj modified-nodes n))))

(defmethod perform :connect
  [{:keys [graph modified-nodes] :as ctx} m]
  (let [src (resolve-tempid ctx (:source-id m))
        tgt (resolve-tempid ctx (:target-id m))]
    (assoc ctx
           :graph          (lg/connect graph src (:source-label m) tgt (:target-label m))
           :modified-nodes (conj modified-nodes tgt))))

(defmethod perform :disconnect
  [{:keys [graph modified-nodes] :as ctx} m]
  (let [src (resolve-tempid ctx (:source-id m))
        tgt (resolve-tempid ctx (:target-id m))]
    (assoc ctx
           :graph          (lg/disconnect graph src (:source-label m) tgt (:target-label m))
           :modified-nodes (conj modified-nodes tgt))))

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

(defn- affected-nodes
  [{:keys [graph modified-nodes] :as ctx}]
  (assoc ctx :affected-nodes (dg/tclosure graph modified-nodes)))

(defn- determine-obsoletes
  [{:keys [graph affected-nodes cache-keys] :as ctx}]
  (assoc ctx :obsolete-cache-keys
         (keep identity (mapcat #(vals (get cache-keys %)) affected-nodes))))

(defn- dispose-obsoletes
  [{:keys [cache obsolete-cache-keys] :as ctx}]
  (assoc ctx :values-to-dispose (keep identity (filter disposable? (map #(get cache %) obsolete-cache-keys)))))

(defn- evict-obsolete-caches
  [{:keys [obsolete-cache-keys] :as ctx}]
  (update-in ctx [:cache] (fn [c] (reduce cache/evict c obsolete-cache-keys))))

(defn- determine-autoupdates
  [{:keys [graph affected-nodes] :as ctx}]
  (assoc ctx :expired-outputs (pairwise (comp :on-update :descriptor) (map #(dg/node graph %) affected-nodes))))

(def tx-report-keys [:status :expired-outputs :values-to-dispose :affected-nodes :new-event-loops :tempids :graph :txs])

(defn- finalize-update
  "Makes the transacted graph the new value of the world-state graph.
   Likewise for cache and cache-keys."
  [{:keys [graph cache cache-keys world-time] :as ctx}]
  (update-in ctx [:world] assoc
             :graph graph
             :cache cache
             :cache-keys cache-keys
             :world-time (inc world-time)
             :last-tx (assoc (select-keys ctx tx-report-keys) :status :ok)))

(defn- transact*
  [ctx]
  (-> (apply-tx ctx (:txs ctx))
    affected-nodes
    determine-obsoletes
    dispose-obsoletes
    evict-obsolete-caches
    determine-autoupdates
    finalize-update))

(defn- new-transaction-context
  [world-ref txs]
  (let [current-world @world-ref]
    {:world-ref       world-ref
     :world           current-world
     :txs             txs
     :graph           (:graph current-world)
     :cache           (:cache current-world)
     :cache-keys      (:cache-keys current-world)
     :world-time      (:world-time current-world)
     :tempids         {}
     :new-event-loops []
     :modified-nodes  #{}
     :subscribe-to    (:subscribe-to current-world)
     :pending         [#(transact* %)]}))

(defn- run-to-completion
  [w ctx]
  (if-let [action (first (:pending ctx))]
    (recur w (action (update-in ctx [:pending] rest)))
    (:world ctx)))

(defn transact
  [world-ref txs]
  (:last-tx (dosync (alter world-ref run-to-completion (new-transaction-context world-ref txs)))))

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

In this case, the map passed to perform would look like `{:type :update-node :id tempid :fn update-fn :args update-fn-args}` All calls
to `perform` return a new (updated) transaction context.

Calls to perform are only executed by [[transact]]. The data required for `perform` calls are constructed in action functions,
such as [[connect]] and [[update-node]]."

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

        #'update-node
        "*transaction step* - Expects a node and function f (with optional args) to be performed on the
resource indicated by the node. The node may be a uncommitted, in which case it will have a tempid."

        #'transact
        "Execute a transaction to create a new version of the world's state. This takes in
a ref to the current world state, modifies it according to the transaction steps in txs,
and returns a new world.

The txs must have been created by the transaction step functions in this namespace: [[connect]],
[[disconnect]], [[new-node]], and [[update-node]]. The collection of txs can be nested."
}]
  (alter-meta! v assoc :doc doc))
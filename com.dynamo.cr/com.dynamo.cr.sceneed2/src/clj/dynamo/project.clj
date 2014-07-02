(ns dynamo.project
  (:require [clojure.core.cache :as cache]
            [service.log :as log]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.graph.query :as q]
            [internal.java :as j]
            [dynamo.node :as node]
            [internal.cache :refer [make-cache]]
            [schema.core :as s]))

(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(defn background-executor-loop
  [q]
  (loop [v (.take q)]
    (try
      (v)
      (catch Throwable t
        (log/error :exception t :calling v)))
    (recur (.take q))))

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn new-cache-key [] (.getAndIncrement nextkey))

(def CacheKey {s/Int {s/Keyword s/Int}})

(def Project
  {:cache-keys CacheKey
   :graph      s/Any
   :cache      s/Any})

(defn make-project
  []
  (let [q (java.util.concurrent.LinkedBlockingQueue.)]
    {:graph           (dg/empty-graph)
     :cache-keys      {}
     :cache           (make-cache)
     :disposal-queue  q
     :disposal-thread (j/daemonize #(background-executor-loop q))}))

(defonce project-state (ref (make-project)))

(defn- hit-cache [cache-key value]
  (dosync (alter project-state update-in [:cache] cache/hit cache-key))
  value)

(defn- miss-cache [cache-key value]
  (dosync (alter project-state update-in [:cache] cache/miss cache-key value))
  value)

(defn- produce-value [resource label]
  (node/get-value (:graph @project-state) (:_id resource) label))

(defn get-resource-value [resource label]
  (if-let [cache-key (get-in @project-state [:cache-keys (:_id resource) label])]
    (let [cache (:cache @project-state)]
      (if (cache/has? cache cache-key)
          (hit-cache  cache-key (get cache cache-key))
          (miss-cache cache-key (produce-value resource label))))
    (produce-value resource label)))

(defn new-resource
  ([resource]
    (new-resource resource (set (keys (:inputs resource))) (set (keys (:transforms resource)))))
  ([resource inputs outputs]
    [{:type :create-node
        :node resource
        :inputs inputs
        :outputs outputs}]))

(defn update-resource
  [resource f & args]
  [{:type :update-node
      :node-id (:_id resource)
      :fn f
      :args args}])

(defn connect
  [from-resource from-label to-resource to-label]
  [{:type :connect
     :source-id    (:_id from-resource)
     :source-label from-label
     :target-id    (:_id to-resource)
     :target-label to-label}])

(defn disconnect
  [from-resource from-label to-resource to-label]
  [{:type :disconnect
     :source-id    (:_id from-resource)
     :source-label from-label
     :target-id    (:_id to-resource)
     :target-label to-label}])

(defn resolve-tempid [ctx x] (if (pos? x) x (get (:tempids ctx) x)))

(defn resource->cache-keys [m]
  (apply hash-map
         (flatten
           (for [output (:cached m)]
            [output (new-cache-key)]))))

(defmulti perform (fn [ctx m] (:type m)))

(defmethod perform :create-node
  [{:keys [graph tempids cache-keys] :as ctx} m]
  (let [next-id (dg/next-node graph)]
    (assoc ctx
      :graph (lg/add-labeled-node graph (:inputs m) (:outputs m) (assoc (:node m) :_id next-id))
      :tempids    (assoc tempids    (get-in m [:node :_id]) next-id)
      :cache-keys (assoc cache-keys next-id (resource->cache-keys (:node m))))))

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

(defn apply-tx
  [ctx actions]
  (reduce
    (fn [ctx action]
      (cond
        (sequential? action) (apply-tx ctx action)
        :else                (perform ctx action)))
    ctx
    actions))

(defn- new-transaction-context
  [g]
  {:graph g
   :cache-keys {}
   :tempids {}
   :modified-nodes #{}})

(defn dispose-values!
  [q vs]
  (doseq [v vs]
    (when (satisfies? IDisposable v)
      (.put q (bound-fn [] (.dispose v))))))

(defn evict-values!
  [cache keys]
  (reduce cache/evict cache keys))

(defn- pairwise [f coll]
  (for [n coll
        x (f n)]
    [n x]))

(defn transact
  [tx]
  (dosync
    (let [tx-result (apply-tx (new-transaction-context (:graph @project-state)) tx)]
      (alter project-state assoc :graph (:graph tx-result))
      (alter project-state update-in [:cache-keys] merge (:cache-keys tx-result))
      (let [affected-subgraph   (dg/tclosure (:graph tx-result) (:modified-nodes tx-result))
            affected-cache-keys (keep identity (mapcat #(vals (get-in @project-state [:cache-keys %])) affected-subgraph))
            old-cache           (:cache @project-state)
            affected-values     (map #(get old-cache %) affected-cache-keys)]
        (dispose-values! (:disposal-queue @project-state) affected-values)
        (alter project-state update-in [:cache] evict-values! affected-cache-keys)
        (doseq [expired-output (pairwise :on-update (map #(dg/node (:graph tx-result) %) affected-subgraph))]
          (apply get-resource-value expired-output))
        (assoc tx-result :status :ok)))))

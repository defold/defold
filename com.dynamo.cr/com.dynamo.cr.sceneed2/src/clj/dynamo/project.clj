(ns dynamo.project
  (:require [clojure.java.io :as io]
            [clojure.core.async :as a :refer [put! onto-chan]]
            [clojure.core.cache :as cache]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [service.log :as log]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.graph.query :as q]
            [internal.java :as j]
            [dynamo.types :as t]
            [dynamo.node :as node :refer [defnode]]
            [dynamo.file]
            [internal.cache :refer [make-cache]]
            [plumbing.core :refer [defnk]]
            [schema.core :as s])
  (:import [org.eclipse.core.resources IFile]))

(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn new-cache-key [] (.getAndIncrement nextkey))

(def ^:dynamic *current-project* nil)

(defrecord UnloadableNamespace [ns-decl]
  IDisposable
  (dispose [this] (remove-ns (second ns-decl))))

(defnk load-project-file
  [project this g]
  ;; TODO - error handling and reporting
  (let [source  (:resource this)
        ns-decl (read-file-ns-decl source)]
    (binding [*current-project* project]
      (Compiler/load (io/reader source) (.toString (.getFullPath source)) (.getName source))
      (UnloadableNamespace. ns-decl))))

(def ClojureSourceFile
  {:properties {:resource {:schema IFile}}
   :transforms {:namespace #'load-project-file}
   :cached     #{:namespace}
   :on-update  #{:namespace}})

(defnode ClojureSourceNode
 ClojureSourceFile)

(declare transact new-resource)

(defn on-load-code
  [project-state ^IFile resource input]
  (transact project-state
            (new-resource (make-clojure-source-node :resource resource))))

(defn make-project
  [tx-report-chan]
  {:loaders         {".clj" on-load-code}
   :graph           (dg/empty-graph)
   :cache-keys      {}
   :cache           (make-cache)
   :tx-report-chan  tx-report-chan
   :build-path      "build"})

(defn dispose-project
  [project-state]
  (let [report-ch (:tx-report-chan @project-state)
        cached-vals (vals (:cache @project-state))]
    (onto-chan report-ch (filter #(satisfies? IDisposable %) cached-vals) false)))

(defn register-loader
  [project-state filetype loader]
  (dosync (alter project-state assoc-in [:loaders filetype] loader)))

(defn- loader-for
  [project-state filename]
  (let [lfs (:loaders @project-state)]
    (or
      (some (fn [[filetype lf]] (when (.endsWith filename filetype) lf)) lfs)
      (fn [_ _] (throw (ex-info (str "No loader has been registered that can handle " filename) {}))))))

(defn resource-at-path
  [project-state path]
  (when-let [eproj (:eclipse-project @project-state)]
    (.getFile eproj path)))

(defn load-resource
  [project-state ^IFile file]
  ((loader-for project-state (.getName file)) project-state file (io/reader file)))

(defn- hit-cache [project-state cache-key value]
  (dosync (alter project-state update-in [:cache] cache/hit cache-key))
  value)

(defn- miss-cache [project-state cache-key value]
  (dosync (alter project-state update-in [:cache] cache/miss cache-key value))
  value)

(defn- produce-value [project-state resource label]
  (node/get-value (:graph @project-state) resource label {:project project-state}))

(defn get-resource-value [project-state resource label]
  (if-let [cache-key (get-in @project-state [:cache-keys (:_id resource) label])]
    (let [cache (:cache @project-state)]
      (if (cache/has? cache cache-key)
          (hit-cache  project-state cache-key (get cache cache-key))
          (miss-cache project-state cache-key (produce-value project-state resource label))))
    (produce-value project-state resource label)))

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
  [state]
  {:state           state
   :graph           (:graph state)
   :cache-keys      (:cache-keys state)
   :tempids         {}
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

;(defn transact
;  [tx]
;  (dosync
;    (let [tx-result (apply-tx (new-transaction-context (:graph @project-state)) tx)]
;      (alter project-state assoc :graph (:graph tx-result))
;      (alter project-state update-in [:cache-keys] merge (:cache-keys tx-result))
;      (let [affected-subgraph   (dg/tclosure (:graph tx-result) (:modified-nodes tx-result))
;            affected-cache-keys (keep identity (mapcat #(vals (get-in @project-state [:cache-keys %])) affected-subgraph))
;            old-cache           (:cache @project-state)
;            affected-values     (map #(get old-cache %) affected-cache-keys)]
;        (dispose-values! (:disposal-queue @project-state) affected-values)
;        (alter project-state update-in [:cache] evict-values! affected-cache-keys)
;        (doseq [expired-output (pairwise :on-update (map #(dg/node (:graph tx-result) %) affected-subgraph))]
;          (apply get-resource-value expired-output))
;        (assoc tx-result :status :ok)))))

(defn- affected-nodes
  [{:keys [graph modified-nodes] :as ctx}]
  (assoc ctx :affected-nodes (dg/tclosure graph modified-nodes)))

(defn- determine-obsoletes
  [{:keys [state affected-nodes] :as ctx}]
  (assoc ctx :obsolete-cache-keys
         (keep identity (mapcat #(vals (get-in state [:cache-keys %])) affected-nodes))))

(defn- dispose-obsoletes
  [{:keys [state obsolete-cache-keys] :as ctx}]
  (assoc ctx :values-to-dispose (keep identity (filter #(satisfies? IDisposable (get-in state [:cache %])) obsolete-cache-keys))))

(defn- evict-obsolete-caches
  [{:keys [state obsolete-cache-keys] :as ctx}]
  (update-in ctx [:state :cache] (fn [c] (reduce cache/evict c obsolete-cache-keys))))

(defn- determine-autoupdates
  [{:keys [graph affected-nodes] :as ctx}]
  (assoc ctx :expired-outputs (pairwise :on-update (map #(dg/node graph %) affected-nodes))))

(defn- recompute-autoupdates
  [{:keys [expired-outputs] :as ctx}]
  (doseq [expired-output expired-outputs]
    (apply get-resource-value expired-output))
  ctx)

(defn- finalize-update
  [{:keys [graph cache-keys] :as ctx}]
  (-> ctx
    (assoc-in [:state :graph] graph)
    (assoc-in [:state :cache-keys] cache-keys)
    (assoc :status :ok)))

(defn- send-to-tx-queue
  [project-state tx-result]
  (put! (:tx-report-chan @project-state)
        (assoc tx-result :project-state project-state))
  tx-result)

(defn transact*
  [current-state tx]
  (-> current-state
    new-transaction-context
    (apply-tx tx)
    affected-nodes
    determine-obsoletes
    dispose-obsoletes
    evict-obsolete-caches
    determine-autoupdates
    finalize-update))

(defn transact
  [project-state txs]
  (send-to-tx-queue project-state
    (dosync
      (let [tx-result (transact* @project-state txs)]
        (ref-set project-state (:state tx-result))
        tx-result))))


(doseq [[v doc]
       {#'register-loader
        "Associate a filetype (extension) with a loader function. The given loader will be
used any time a file with that type is opened."

          #'load-resource
        "Load a resource, usually from file. This looks up a suitable loader based on the filename.
Loaders must be registered via register-loader before they can be used.

This will invoke the loader function with the filename and a reader to supply the file contents."

        ;; TODO - much more doco.
        }]
  (alter-meta! v assoc :doc doc))
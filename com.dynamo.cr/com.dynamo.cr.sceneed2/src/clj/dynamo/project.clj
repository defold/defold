(ns dynamo.project
  "Functions for performing transactional changes to a project and inspecting its current state."
  (:require [clojure.java.io :as io]
            [clojure.core.async :as a :refer [put! onto-chan]]
            [clojure.core.cache :as cache]
            [clojure.tools.namespace.file :refer [read-file-ns-decl]]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.graph.query :as q]
            [internal.java :as j]
            [dynamo.types :as t]
            [dynamo.node :as node :refer [defnode]]
            [dynamo.file :as file]
            [internal.cache :refer [make-cache]]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [eclipse.markers :as markers])
  (:import [org.eclipse.core.resources IFile]))

(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextkey (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn new-cache-key [] (.getAndIncrement nextkey))

(def ^:dynamic *current-project* nil)

(defmacro with-current-project
  "Gives forms wrapped `with-current-project` access to *current-project*, a ref containing the current project state."
  [& forms]
  `(binding [dynamo.project/*current-project* (internal.system/current-project)]
     ~@forms))

(defrecord UnloadableNamespace [ns-decl]
  IDisposable
  (dispose [this] (remove-ns (second ns-decl))))

(defnk load-project-file
  [project this g]
  (let [source  (:resource this)
        ns-decl (read-file-ns-decl source)
        source-file (file/eclipse-file source)]
    (binding [*current-project* project]
      (markers/remove-markers source-file)
      (try
        (do
          (Compiler/load (io/reader source) (file/local-path source) (.getName source-file))
          (UnloadableNamespace. ns-decl))
        (catch clojure.lang.Compiler$CompilerException compile-error
          (markers/compile-error source-file (.getMessage (.getCause compile-error)) (.line compile-error))
          {:compile-error (.getMessage (.getCause compile-error))})))))

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
  [eclipse-project tx-report-chan]
  {:loaders         {"clj" on-load-code}
   :graph           (dg/empty-graph)
   :cache-keys      {}
   :cache           (make-cache)
   :tx-report-chan  tx-report-chan
   :eclipse-project eclipse-project})

(defn dispose-project
  [project-state]
  (let [report-ch (:tx-report-chan @project-state)
        cached-vals (vals (:cache @project-state))]
    (onto-chan report-ch (filter #(satisfies? IDisposable %) cached-vals) false)))

(defn register-loader
  [project-state filetype loader]
  (dosync (alter project-state assoc-in [:loaders filetype] loader)))

(defn- loader-for
  [project-state ext]
  (let [lfs (:loaders @project-state)]
    (or
      (some (fn [[filetype lf]] (when (= filetype ext) lf)) lfs)
      (fn [_ _ _] (throw (ex-info (str "No loader has been registered that can handle " ext) {}))))))

(defn load-resource
  [project-state path]
  ((loader-for project-state (file/extension path)) project-state path (io/reader path)))

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
  "*transaction step* - creates a resource in the project. Expects a map of properties, including transformations. May include an `:_id` key containing a
tempid if the resource will be referenced again in the same transaction. If included, _input_ and _output_ are sets of input and output labels, respectively.
If not included, the `:input` and `:output` keys in the property may will be assigned as the resource's inputs and outputs."
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
  [{:keys [graph tempids cache-keys modified-nodes] :as ctx} m]
  (let [next-id (dg/next-node graph)]
    (assoc ctx
      :graph          (lg/add-labeled-node graph (:inputs m) (:outputs m) (assoc (:node m) :_id next-id))
      :tempids        (assoc tempids    (get-in m [:node :_id]) next-id)
      :cache-keys     (assoc cache-keys next-id (resource->cache-keys (:node m)))
      :modified-nodes (conj modified-nodes next-id))))

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
(prn "affected-nodes: " affected-nodes)
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

(defn- transact*
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

(defn query
  [project-state & clauses]
  (map #(dg/node (:graph @project-state) %)
       (q/query (:graph @project-state) clauses)))

(doseq [[v doc]
       {#'*current-project*
        "When used within a [[with-current-project]], contains a ref of the current project. Otherwise nil."
        #'ClojureSourceFile
        "Behavior included in `ClojureSourceNode`."

        #'perform
        "A multimethod used for defining methods that perform the individual actions within a
transaction. Expects to receive transaction context (ctx) and a map (m) containing a value for keyword `:type`, and other keys and
values appropriate to the transformation it represents. An example, handling the `:update-node` type:

    (defmethod perform :update-node
      [{:keys [graph modified-nodes] :as ctx} m]
      (let [n (resolve-tempid ctx (:node-id m))]
        (assoc ctx
               :graph          (apply dg/transform-node graph n (:fn m) (:args m))
               :modified-nodes (conj modified-nodes n))))

In this case, the map passed to perform would look like `{:type :update-node :id tempid :fn update-fn :args update-fn-args}` All calls
to `perform` return a new (updated) transaction context.

Calls to perform are typically executed by [[transact]]. The data required for `perform` calls are typically constructed in action functions,
such as [[update-resource]]."

        #'connect
        "*transaction step* - Creates a transaction step connecting a source label and resource id (`from-resource from-label`) and a target label resource id
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]. Ids passed to `connect` may be tempids."

        #'disconnect
        "*transaction step* - The reverse of [[connect]]. Creates a transaction step disconnecting a source label and resource id
(`from-resource from-label`) from a target label resource id
(`to-resource to-label`). It returns a value suitable for consumption by [[perform]]. Ids passed to `disconnect` may be tempids."

        #'update-resource
        "*transaction step* - Expects a map containing `:node-id` key as resource and function f (with optional args) to be performed on the
resource indicated by the node id. The node id may be a tempid."

        #'register-loader
        "Associate a filetype (extension) with a loader function. The given loader will be
used any time a file with that type is opened."

        #'load-resource
        "Load a resource, usually from file. This looks up a suitable loader based on the filename.
Loaders must be registered via register-loader before they can be used.

This will invoke the loader function with the filename and a reader to supply the file contents."

        #'query
        "Query the project for resources that match all the clauses. Clauses are implicitly anded together.
A clause may be one of the following forms:

[:attribute value] - returns nodes that contain the given attribute and value.
(protocol protocolname) - returns nodes that satisfy the given protocol
(input fromnode)        - returns nodes that have 'fromnode' as an input
(output tonode)         - returns nodes that have 'tonode' as an output

All the list forms look for symbols in the first position. Be sure to quote the list
to distinguish it from a function call."
        ;; TODO - much more doco.
        }]
  (alter-meta! v assoc :doc doc))
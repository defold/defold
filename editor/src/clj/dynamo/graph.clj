;; Copyright 2020-2025 The Defold Foundation
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

(ns dynamo.graph
  "Main api for graph and node"
  (:refer-clojure :exclude [constantly deftype])
  (:require [clojure.tools.macro :as ctm]
            [cognitect.transit :as transit]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.low-memory :as low-memory]
            [internal.node :as in]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.util :as util]
            [potemkin.namespaces :as namespaces]
            [schema.core :as s]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [internal.graph.error_values ErrorValue]
           [internal.graph.types Arc]
           [java.io ByteArrayInputStream ByteArrayOutputStream]
           [java.util.concurrent.atomic AtomicInteger]))

(set! *warn-on-reflection* true)

(namespaces/import-vars [internal.graph.types node-id->graph-id node->graph-id sources targets connected? dependencies Node node-id node-id? produce-value node-by-id-at endpoint endpoint-node-id endpoint-label])

(namespaces/import-vars [internal.graph.error-values ->error error-aggregate error-fatal error-fatal? error-info error-info? error-message error-package? error-warning error-warning? error-value? error? flatten-errors map->error package-errors precluding-errors unpack-errors worse-than package-if-error])

(namespaces/import-vars [internal.node value-type-schema value-type? node-type? value-type-dispatch-value inherits? has-input? has-output? has-property? type-compatible? merge-display-order NodeType supertypes declared-properties declared-property-labels declared-inputs declared-outputs cached-outputs input-dependencies input-cardinality cascade-deletes substitute-for input-type output-type input-labels output-labels abstract-output-labels property-display-order])

(namespaces/import-vars [internal.graph arc explicit-arcs-by-source explicit-arcs-by-target node-ids pre-traverse])

(namespaces/import-vars [internal.system endpoint-invalidated-since? evaluation-context-invalidate-counters])

(let [graph-id ^AtomicInteger (AtomicInteger. 0)]
  (defn next-graph-id [] (.getAndIncrement graph-id)))

;; ---------------------------------------------------------------------------
;; State handling
;; ---------------------------------------------------------------------------

;; Only marked dynamic so tests can rebind. Should never be rebound "for real".
(defonce ^:dynamic *the-system* (atom nil))

(def ^:dynamic *tps-debug* nil)

(defn now
  "The basis at the current point in time"
  []
  (is/basis @*the-system*))

(defn clone-system
  ([] (clone-system @*the-system*))
  ([sys] (is/clone-system sys)))

(defn system= [s1 s2]
  (is/system= s1 s2))

(defmacro with-system [sys & body]
  `(binding [*the-system* (atom ~sys)]
     ~@body))

(defn node-by-id
  "Returns a node given its id. If the basis is provided, it returns the value of the node using that basis.
   Otherwise, it uses the current basis."
  ([node-id]
   (let [graph-id (node-id->graph-id node-id)]
     (ig/node-id->node (is/graph @*the-system* graph-id) node-id)))
  ([basis node-id]
   (gt/node-by-id-at basis node-id)))

(defn node-exists?
  ([node-id]
   (node-exists? (now) node-id))
  ([basis node-id]
   (some? (gt/node-by-id-at basis node-id))))

(defn node-type*
  "Return the node-type given a node-id.  Uses the current basis if not provided."
  ([node-id]
   (node-type* (now) node-id))
  ([basis node-id]
   (when-let [n (gt/node-by-id-at basis node-id)]
     (gt/node-type n))))

(defn node-type
  "Return the node-type given a node."
  [node]
  (when node
    (gt/node-type node)))

(defn node-type-kw
  "Return the fully-qualified keyword that corresponds to the node type of the
  specified node id, or nil if the node does not exist."
  ([node-id]
   (:k (node-type* (now) node-id)))
  ([basis node-id]
   (:k (node-type* basis node-id))))

(defn node-override? [node]
  (some? (gt/original node)))

(defn own-property-values
  "Returns a map of property-label property-value for the specified node. If the
  queried node is an override node, the map will contain overridden properties
  only. Otherwise, the map will include assigned properties and defaults."
  [node]
  (or (if (node-override? node)
        (gt/overridden-properties node)
        (coll/merge
          (in/defaults (gt/node-type node))
          (gt/assigned-properties node)))
      {}))

(defn invalidate-counters
  "The current state of the invalidate counters in the system."
  []
  (is/invalidate-counters @*the-system*))

(defn flag-nodes-as-migrated! [evaluation-context migrated-node-ids]
  (let [tx-data-context (:tx-data-context evaluation-context)]
    (swap! tx-data-context update :migrated-node-ids coll/into-set migrated-node-ids)
    nil))

(defn endpoint-invalidated-pred
  "Return a predicate function that takes an Endpoint and returns a boolean
  signalling if it has been invalidated since the supplied
  snapshot-invalidate-counters based on the system-invalidate-counters. If
  snapshot-invalidate-counters is nil, returns fn/constantly-false. If
  system-invalidate-counters is not supplied, use the invalidate-counters
  from the current system."
  ([snapshot-invalidate-counters]
   (endpoint-invalidated-pred snapshot-invalidate-counters (invalidate-counters)))
  ([snapshot-invalidate-counters system-invalidate-counters]
   {:pre [(or (nil? snapshot-invalidate-counters) (map? snapshot-invalidate-counters))
          (map? system-invalidate-counters)]}
   (if (or (nil? snapshot-invalidate-counters)
           (identical? snapshot-invalidate-counters system-invalidate-counters))
     fn/constantly-false
     (fn endpoint-invalidated? [endpoint]
       (endpoint-invalidated-since? endpoint snapshot-invalidate-counters system-invalidate-counters)))))

(defn cache "The system cache of node values"
  []
  (is/system-cache @*the-system*))

(defn clear-system-cache!
  "Clears a cache (default *the-system* cache), useful when debugging"
  ([] (clear-system-cache! *the-system*))
  ([sys-atom]
   (let [cleared-cache (c/cache-clear (:cache @sys-atom))]
     (swap! sys-atom assoc :cache cleared-cache)
     nil))
  ([sys-atom node-id]
   (let [outputs (cached-outputs (node-type* node-id))
         entries (map (partial endpoint node-id) outputs)]
     (swap! sys-atom update :cache c/cache-invalidate entries)
     nil)))

(defn cache-info
  "Return a map detailing cache utilization."
  ([] (c/cache-info (cache)))
  ([cache] (c/cache-info cache)))

(defn graph "Given a graph id, returns the particular graph in the system at the current point in time"
  [graph-id]
  (is/graph @*the-system* graph-id))

(when *tps-debug*
  (def tps-counter (agent (long-array 3 0)))

  (defn tick [^longs tps-counts now]
    (let [last-report-time (aget tps-counts 1)
          transaction-count (inc (aget tps-counts 0))]
      (aset-long tps-counts 0 transaction-count)
      (when (> now (+ last-report-time 1000000000))
        (let [elapsed-time (/ (- now last-report-time) 1000000000.00)]
         (do (println "TPS" (/ transaction-count elapsed-time))))
        (aset-long tps-counts 1 now)
        (aset-long tps-counts 0 0)))
    tps-counts))

(defn make-transaction-context [opts]
  (let [system (deref *the-system*)
        basis (is/basis system)
        id-generators (is/id-generators system)
        override-id-generator (is/override-id-generator system)
        tx-data-context-map (or (:tx-data-context-map opts) {})
        metrics-collector (:metrics opts)
        track-changes (:track-changes opts true)]
    (it/new-transaction-context basis id-generators override-id-generator tx-data-context-map metrics-collector track-changes)))

(defn commit-tx-result!
  [tx-result transact-opts]
  (when (and (not (:dry-run transact-opts))
             (= :ok (:status tx-result)))
    (swap! *the-system* is/merge-graphs (get-in tx-result [:basis :graphs]) (:graphs-modified tx-result) (:outputs-modified tx-result) (:nodes-deleted tx-result))
    nil))

(defn transact
  "Provides a way to run a transaction against the graph system.  It takes a list of transaction steps.

  Example:

      (g/transact
         [(g/connect n1 output-name n :xs)
         (g/connect n2 output-name n :xs)])

  It returns the transaction result, (tx-result),  which is a map containing keys about the transaction.
  Transaction result-keys:
  `[:status :basis :graphs-modified :nodes-added :nodes-modified :nodes-deleted :outputs-modified :label :sequence-label]`
  "
  ([txs]
   (transact nil txs))
  ([opts txs]
   (when *tps-debug*
     (send-off tps-counter tick (System/nanoTime)))
   (let [transaction-context (make-transaction-context opts)
         tx-result (it/transact* transaction-context txs)]
     (commit-tx-result! tx-result opts)
     tx-result)))

;; ---------------------------------------------------------------------------
;; Using transaction data
;; ---------------------------------------------------------------------------

(defn tx-data-step-types
  "Given a sequence of possibly nested transaction steps, returns a sequence of
  keywords identifying the type of each encountered transaction step. Used in
  tests."
  [txs]
  (sequence
    (comp coll/flatten-xf
          (map it/tx-step-type))
    txs))

(defn tx-data-added-arcs
  "Given a sequence of possibly nested transaction steps, returns a sequence of
  Arcs that will be added by any encountered :tx-step/connect steps."
  [txs]
  (sequence
    (comp coll/flatten-xf
          (keep it/tx-step-added-arc))
    txs))

(defn tx-data-added-nodes
  "Given a sequence of possibly nested transaction steps, returns a sequence of
  Nodes that will be added by any encountered :tx-step/add-node steps."
  [txs]
  (sequence
    (comp coll/flatten-xf
          (keep it/tx-step-added-node))
    txs))

(defn tx-data-added-node-ids
  "Given a sequence of possibly nested transaction steps, returns a sequence of
  node-ids that will be added by any encountered :tx-step/add-node steps."
  [txs]
  (map gt/node-id
       (tx-data-added-nodes txs)))

;; ---------------------------------------------------------------------------
;; Using transaction values
;; ---------------------------------------------------------------------------
(defn tx-result? [x]
  (and (map? x)
       (contains? x :status)
       (contains? x :basis)
       (contains? x :graphs-modified)
       (contains? x :nodes-added)))

(defn tx-nodes-added
 "Returns a list of the node-ids added given a result from a transaction, (tx-result)."
  [tx-result]
  (:nodes-added tx-result))

(defn transaction-basis
  "Returns the final basis from the result of a transaction given a tx-result"
  [tx-result]
  (:basis tx-result))

(defn pre-transaction-basis
  "Returns the original, starting basis from the result of a transaction given a tx-result"
  [tx-result]
  (:original-basis tx-result))

(defn migrated-node-ids
  "Returns the set of node-ids that were flagged as migrated from the result of a transaction given a tx-result."
  [tx-result]
  (-> tx-result :tx-data-context-map (:migrated-node-ids #{})))

;; ---------------------------------------------------------------------------
;; Intrinsics
;; ---------------------------------------------------------------------------
(defn strip-alias
  "If the list ends with :as _something_, return _something_ and the
  list without the last two elements"
  [argv]
  (if (and
       (<= 2 (count argv))
       (= :as (nth argv (- (count argv) 2))))
    [(last argv) (take (- (count argv) 2) argv)]
    [nil argv]))

(defmacro fnk
  [argv & tail]
  (let [param        (gensym "m")
        [alias argv] (strip-alias (vec argv))
        kargv        (mapv keyword argv)
        annotations  (into {} (map (juxt keyword meta)) argv)
        arglist      (interleave argv (map #(list `get param %) kargv))]
    (if alias
      `(with-meta
         (fn [~param]
           (let [~alias (select-keys ~param ~kargv)
                 ~@(vec arglist)]
             ~@tail))
         {:arguments (quote ~kargv)
          :annotations (quote ~annotations)})
      `(with-meta
         (fn [~param]
           (let ~(vec arglist) ~@tail))
         {:arguments (quote ~kargv)
          :annotations (quote ~annotations)}))))

(defmacro defnk
  [symb & body]
  (let [[name args]  (ctm/name-with-attributes symb body)]
    (assert (symbol? name) (str "Name for defnk is not a symbol:" name))
    `(def ~name (fnk ~@args))))

(defmacro constantly [v] `(let [ret# ~v] (dynamo.graph/fnk [] ret#)))

(defmacro deftype
  [symb & body]
  (let [fully-qualified-node-type-symbol (symbol (str *ns*) (str symb))
        key (keyword fully-qualified-node-type-symbol)]
    `(do
       (in/register-value-type '~fully-qualified-node-type-symbol ~key)
       (def ~symb (in/register-value-type ~key (in/make-value-type '~symb ~key ~@body))))))

(deftype Any        s/Any)
(deftype Bool       s/Bool)
(deftype Str        String)
(deftype Int        s/Int)
(deftype Num        s/Num)
(deftype NodeID     s/Int)
(deftype Keyword    s/Keyword)
(deftype KeywordMap {s/Keyword s/Any})
(deftype IdPair     [(s/one s/Str "id") (s/one s/Int "node-id")])
(deftype Dict       {s/Str s/Int})
(deftype Properties
    {:properties {s/Keyword {:node-id                              s/Int
                             (s/optional-key :validation-problems) s/Any
                             :value                                s/Any ; Can be property value or ErrorValue
                             :type                                 s/Any
                             s/Keyword                             s/Any}}
     (s/optional-key :node-id) s/Int
     (s/optional-key :display-order) [(s/conditional vector? [(s/one s/Any "category") s/Keyword] keyword? s/Keyword)]})
(deftype Err ErrorValue)

;; ---------------------------------------------------------------------------
;; Definition
;; ---------------------------------------------------------------------------
(defn construct
  "Creates an instance of a node. The node type must have been
  previously defined via `defnode`.

  The node's properties will all have their default values. The caller
  may pass key/value pairs to override properties.

  A node that has been constructed is not connected to anything and it
  doesn't exist in any graph yet.

  Example:
  (defnode GravityModifier
    (property acceleration Int (default 32))

  (construct GravityModifier :acceleration 16)"
  [node-type-ref & {:as args}]
  (in/construct node-type-ref args))

(defmacro defnode
  "Given a name and a specification of behaviors, creates a node,
   and attendant functions.

  Allowed clauses are:

  (inherits _symbol_)

  Compose the behavior from the named node type

  (input _symbol_ _schema_ [:array]?)

  Define an input with the name, whose values must match the schema.

  If the :array flag is present, then this input can have multiple
  outputs connected to it. Without the :array flag, this input can
  only have one incoming connection from an output.

  (property _symbol_ _property-type_ & _options_)

  Define a property with schema and, possibly, default value and
  constraints. Property options include the following:
    (default _value_)
    Set a default value for the property. If no default is set, then the property will be nil.

    (value _getter_)
    Define a custom getter. It must be an fnk. The fnk's arguments are magically wired the same as an output producer.

    (dynamic _label_ _evaluator_)
    Define a dynamic attribute of the property. The label is a symbol. The evaluator is an fnk like the getter.

    (set (fn [evaluation-context self old-value new-value]))
    Define a custom setter. This is _not_ an fnk, but a strict function of 4 arguments.

  (output _symbol_ _type_ (:cached)? _producer_)

  Define an output to produce values of type. The ':cached' flag is
  optional. _producer_ may be a var that names an fn, or fnk.  It may
  also be a function tail as [arglist] + forms.

  In the arglist, you can specify ^:raw or ^:try metadata tags on arguments to
  control how dependencies are evaluated. Tagging a property argument with ^:raw
  will bypass any (value _getter_) declared for the property, and instead
  produce the raw value assigned to the property map in the node or its override
  chain. Tagging an argument with ^:try allows the computation of the output
  even when some of its arguments are errors. Note that adding ^:try metadata on
  an :array input will not supply an ErrorValue to the fnk; instead, it will
  provide an array where some items might be ErrorValues.

  Values produced on an output with the :cached flag will be cached in
  memory until the node is affected by some change in inputs or
  properties. At that time, the cached value will be sent for
  disposal.

  Example (from [[editors.atlas]]):

    (defnode TextureCompiler
      (input    textureset TextureSet)
      (property texture-filename Str (default \"\"))
      (output   texturec Any compile-texturec)))

    (defnode TextureSetCompiler
      (input    textureset TextureSet)
      (property textureset-filename Str (default \"\"))
      (output   texturesetc Any compile-texturesetc)))

    (defnode AtlasCompiler
      (inherit TextureCompiler)
      (inherit TextureSetCompiler))

  This will produce a record `AtlasCompiler`. `defnode` merges the
  behaviors appropriately.

    A node may also implement protocols or interfaces, using a syntax
  identical to `deftype` or `defrecord`. A node may implement any
  number of such protocols.

  Every node always implements dynamo.graph/Node."
  [symb & body]
  (let [[symb forms] (ctm/name-with-attributes symb body)
        fully-qualified-node-type-symbol (symbol (str *ns*) (str symb))
        node-type-def (in/process-node-type-forms fully-qualified-node-type-symbol forms)
        fn-paths (in/extract-def-fns node-type-def)
        fn-defs (for [[path func] fn-paths]
                  (list `def (in/dollar-name symb path) func))
        node-type-def (util/update-paths node-type-def fn-paths
                                         (fn [path func curr]
                                           (assoc curr :fn (list `var (in/dollar-name symb path)))))
        node-key (:key node-type-def)
        derivations (for [tref (:supertypes node-type-def)]
                      `(when-not (contains? (descendants ~(:key (deref tref))) ~node-key)
                         (derive ~node-key ~(:key (deref tref)))))
        node-type-def (update node-type-def :supertypes #(list `quote %))
        runtime-definer (symbol (str symb "*"))
        ;; TODO - investigate if we even need to register these types
        ;; in release builds, since we don't do schema checking?
        type-regs (for [[key-form value-type-form] (:register-type-info node-type-def)]
                    `(in/register-value-type ~key-form ~value-type-form))
        node-type-def (dissoc node-type-def :register-type-info)]
    `(do
       ~@type-regs
       ~@fn-defs
       (defn ~runtime-definer [] ~node-type-def)
       (def ~symb (in/register-node-type ~node-key (in/map->NodeTypeImpl (~runtime-definer))))
       ~@derivations)))



;; ---------------------------------------------------------------------------
;; Transactions
;; ---------------------------------------------------------------------------

(defmacro make-nodes
  "Create a number of nodes in a graph, binding them to local names
   to wire up connections. The resulting code will return a collection
   of transaction steps, including the steps to construct nodes from the
   bindings.

   If the right side of the binding is a node type, it is used directly. If it
   is a vector, it is treated as a node type followed by initial property values.

  Example:

  (make-nodes view [render     AtlasRender
                    scene      scene/SceneRenderer
                    background background/Gradient
                    camera     [c/CameraController :camera (c/make-orthographic)]]
     (g/connect background   :renderable scene :renderables)
     (g/connect atlas-render :renderable scene :renderables))"
  [graph-id binding-expr & body-exprs]
  (assert (vector? binding-expr) "make-nodes requires a vector for its binding")
  (assert (even? (count binding-expr)) "make-nodes requires an even number of forms in binding vector")
  (let [locals (take-nth 2 binding-expr)
        ctors  (take-nth 2 (next binding-expr))
        ids    (repeat (count locals) `(internal.system/next-node-id @*the-system* ~graph-id))]
    `(let [~@(interleave locals ids)]
       (concat
        ~@(map
           (fn [ctor id]
             (list `it/new-node
                   (if (sequential? ctor)
                     (if (= 2 (count ctor))
                       `(apply construct ~(first ctor) :_node-id ~id (mapcat identity ~(second ctor)))
                       `(construct ~@ctor :_node-id ~id))
                     `(construct  ~ctor :_node-id ~id))))
           ctors locals)
        ~@body-exprs))))

(defn operation-label
  "Set a human-readable label (MessagePattern or string) to describe the current transaction."
  [label]
  (it/label label))

(defn operation-sequence
  "Set a machine-readable label. Successive transactions with the same
  label will be coalesced into a single undo point."
  [label]
  (it/sequence-label label))

(defn prev-sequence-label [graph-id]
  (let [sys @*the-system*]
    (when-let [prev-step (some-> (is/graph-history sys graph-id)
                                 (is/undo-stack)
                                 (last))]
      (:sequence-label prev-step))))

(defn take-node-ids
  "Given a count, returns a realized sequence of claimed, unique node-ids in the
  specified graph."
  [^long graph-id ^long node-id-count]
  (when (pos? node-id-count)
    (is/take-node-ids @*the-system* graph-id node-id-count)))

(def add-node
  "Returns the transaction step for adding a node to the graph. The node will
  typically have been constructed beforehand using the construct function.

  Example:

  `(transact (add-node (construct SimpleTestNode)))`"
  it/new-node)

(defn- construct-node-with-id
  [graph-id node-type args]
  (apply construct node-type :_node-id (is/next-node-id @*the-system* graph-id) (mapcat identity args)))

(defn make-node
  "Returns the transaction step for creating a new node.
  Needs to be executed within a transact to actually create the node
  on a graph.

  Example:

  `(transact (make-node world SimpleTestNode))`"
  [graph-id node-type & args]
  (let [args (if (empty? args)
               {}
               (if (= 1 (count args))
                 (first args)
                 (apply assoc {} args)))]
    (it/new-node (construct-node-with-id graph-id node-type args))))

(defn make-node!
  "Creates the transaction step and runs it in a transaction, returning the resulting node.

  Example:

  `(make-node! world SimpleTestNode)`"
  [graph-id node-type & args]
  (first (tx-nodes-added (transact (apply make-node graph-id node-type args)))))

(defn delete-node
 "Returns the transaction step for deleting a node.
  Needs to be executed within a transact to actually create the node on a graph.

  Example:

  `(transact (delete-node node-id))`"
  [node-id]
  (assert node-id)
  (it/delete-node node-id))

(defn delete-node!
  "Creates the transaction step for deleting a node and runs it in a transaction.
  It returns the transaction results, tx-result

  Example:

  `(delete-node! node-id)`"
  [node-id]
  (assert node-id)
  (transact (delete-node node-id)))

(defn callback
  "Call the specified callback-fn with args when reaching the transaction step."
  [callback-fn & args]
  (it/callback callback-fn args nil))

(defn callback-ec
  "Same as callback, but injects the in-transaction evaluation-context
  as the first argument to the callback-fn."
  [callback-fn & args]
  (it/callback callback-fn args it/inject-evaluation-context-opts))

(defn expand
  "Call the specified function when reaching the transaction step and apply the
  returned transaction steps in the same transaction"
  [f & args]
  (it/expand f args nil))

;; SDK api
(defn expand-ec
  "Call the specified function when reaching the transaction step with an
  in-transaction evaluation context as a first argument and apply the
  returned transaction steps in the same transaction

  NOTE: When used by outline's :tx-attach-fn, avoid creating g/connect txs in
  g/expand-ec to connect self to the child node. Outline inspects txs and
  filters out duplicate connections, but it can't see connection txs if they are
  created only when transaction is executed, as is the case with g/expand-ec"
  [f & args]
  (it/expand f args it/inject-evaluation-context-opts))

(defn connect
  "Make a connection from an output of the source node to an input on the target node.
   Takes effect when a transaction is applied.

  Example:

  `(transact (connect content-node :scalar view-node :first-name))`"
  [source-id source-label target-id target-label]
  (assert source-id)
  (assert target-id)
  (it/connect source-id source-label target-id target-label))

(defn connect!
 "Creates the transaction step to make a connection from an output of the source node to an input on the target node
  and applies it in a transaction

  Example:

  `(connect! content-node :scalar view-node :first-name)`"
  [source-id source-label target-id target-label]
  (assert source-id)
  (assert target-id)
  (transact (connect source-id source-label target-id target-label)))

(defn disconnect
  "Creates the transaction step to remove a connection from an output of the source node to the input on the target node.
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.  Takes effect when a transaction
  is applied with transact.

  Example:

  (`transact (disconnect aux-node :scalar view-node :last-name))`"
  [source-id source-label target-id target-label]
  (assert source-id)
  (assert target-id)
  (it/disconnect source-id source-label target-id target-label))

(defn disconnect!
 "Creates the transaction step to remove a connection from an output of the source node to the input on the target node.
  It also applies it in transaction, returning the transaction result, (tx-result).
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.

  Example:

  `(disconnect aux-node :scalar view-node :last-name)`"
  [source-id source-label target-id target-label]
  (transact (disconnect source-id source-label target-id target-label)))

(defn disconnect-sources
  ([target-id target-label]
   (disconnect-sources (now) target-id target-label))
  ([basis target-id target-label]
   (assert target-id)
   (it/disconnect-sources basis target-id target-label)))

(defn set-properties
  "Creates transaction steps to assign multiple values to a node's properties.
  You must call the transact function on the return value to see the effects.

  Example:
  `(transact (set-properties node-id :name \"item\" :opacity 0.5))`"
  [node-id & kvs]
  (coll/transfer kvs []
    (partition-all 2)
    (mapcat (fn [[property-label new-value]]
              (it/set-property node-id property-label new-value)))))

(defn set-properties!
  "Creates transaction steps to assign multiple values to a node's properties,
  then executes them in a transaction. Returns the result of the transaction,
  (tx-result)"
  [node-id & kvs]
  (transact (apply set-properties node-id kvs)))

(defn set-property
  "Creates a transaction step to assign a value to a node property. You must
  call the transact function on the return value to see the effects.

  Example:
  `(transact (set-property node-id :opacity 0.5))`"
  [node-id property-label value]
  (it/set-property node-id property-label value))

(defn set-property!
  "Creates a transaction step to assign a value to a node property, then
  executes it in a transaction. Returns the result of the transaction,
  (tx-result).

  Example:
  `(set-property! node-id :opacity 0.5)`"
  [node-id property-label value]
  (transact (set-property node-id property-label value)))

(defn update-property
  "Create the transaction step to apply a function to a node's property in a transaction. The
  function f will be invoked as if by (apply f current-value args).  It will take effect when the transaction is
  applied in a transact.

  Example:

  `(transact (g/update-property node-id :int-prop inc))`"
  [node-id p f & args]
  (it/update-property node-id p f args nil))

(defn update-property-ec
  "Same as update-property, but injects the in-transaction evaluation-context
  as the first argument to the update-fn."
  [node-id p f & args]
  (it/update-property node-id p f args it/inject-evaluation-context-opts))

(defn update-property!
  "Create the transaction step to apply a function to a node's property in a transaction. Then it applies the transaction.
   The function f will be invoked as if by (apply f current-value args).  The transaction results, (tx-result), are returned.

  Example:

  `g/update-property! node-id :int-prop inc)`"
  [node-id p f & args]
  (assert node-id)
  (transact (apply update-property node-id p f args)))

(defn clear-property
  [node-id p]
  (assert node-id)
  (it/clear-property node-id p))

(defn clear-property!
  [node-id p]
  (transact (clear-property node-id p)))

(defn update-graph-value [graph-id k f & args]
  (it/update-graph-value graph-id update (into [k f] args)))

(defn set-graph-value
 "Create the transaction step to attach a named value to a graph. It will take effect when the transaction is
  applied in a transact.

  Example:

  `(transact (set-graph-value 0 :string-value \"A String\"))`"
  [graph-id k v]
  (assert graph-id)
  (it/update-graph-value graph-id assoc [k v]))

(defn set-graph-value!
  "Create the transaction step to attach a named value to a graph and applies the transaction.
  Returns the transaction result, (tx-result).

  Example:

  (set-graph-value! 0 :string-value \"A String\")"
  [graph-id k v]
  (assert graph-id)
  (transact (set-graph-value graph-id k v)))

(defn user-data [node-id key]
  (is/user-data @*the-system* node-id key))

(defn user-data! [node-id key value]
  (swap! *the-system* is/assoc-user-data node-id key value)
  value)

(defn user-data-swap! [node-id key f & args]
  (-> (swap! *the-system* (fn [sys] (apply is/update-user-data sys node-id key f args)))
      (is/user-data node-id key)))

(defn user-data-merge! [values-by-key-by-node-id]
  (swap! *the-system* is/merge-user-data values-by-key-by-node-id)
  nil)

(defn invalidate
 "Creates the transaction step to invalidate all the outputs of the node.  It will take effect when the transaction is
  applied in a transact.

  Example:

  `(transact (invalidate node-id))`"
  [node-id]
  (assert node-id)
  (it/invalidate node-id))

(defn invalidate-output
  "Creates a transaction step to invalidate the specified output of the node.
  It will take effect when the transaction is applied in a transact call.

   Example:

   `(transact (invalidate-output node-id :output-label))`"
  [node-id output-label]
  {:pre [(node-id? node-id)
         (keyword? output-label)]}
  (it/invalidate-output node-id output-label))

(defn mark-defective
  "Creates the transaction step to mark a node as _defective_.
  This means that all the outputs of the node will be replace by the defective value.
  It will take effect when the transaction is applied in a transact.

  Example:

  `(transact (mark-defective node-id (g/error-fatal \"Resource Not Found\")))`"
  ([node-id defective-value]
   (assert node-id)
   (mark-defective node-id (node-type* node-id) defective-value))
  ([node-id node-type defective-value]
   (assert node-id)
   (let [jammable-outputs (in/jammable-output-labels node-type)]
     (list
      (set-property node-id :_output-jammers
                    (zipmap jammable-outputs
                            (repeat defective-value)))
      (invalidate node-id)))))

(defn mark-defective!
  "Creates the transaction step to mark a node as _defective_.
  This means that all the outputs of the node will be replace by the defective value.
  It will take effect when the transaction is applied in a transact.

  Example:

  `(mark-defective! node-id (g/error-fatal \"Resource Not Found\"))`"
  [node-id defective-value]
  (assert node-id)
  (transact (mark-defective node-id defective-value)))

(defn defective?
  "Returns true if the specified node-id has been marked as _defective_, or
  false otherwise. The node-id must refer to a valid node."
  ([node-id]
   (defective? (now) node-id))
  ([basis node-id]
   (let [node (node-by-id basis node-id)]
     (assert node)
     (pos? (count (gt/get-property node basis :_output-jammers))))))

;; ---------------------------------------------------------------------------
;; Tracing
;; ---------------------------------------------------------------------------
;;
;; Run several tracers using (juxt (g/make-print-tracer) (g/make-tree-tracer result))
;;

(defn make-print-tracer
  "Prints an indented tree of all eval steps taken."
  []
  (let [depth (atom 0)]
    (fn [state node output-type label]
      (when (or (= :end state) (= :fail state))
        (swap! depth dec))
      (println (str (apply str (take @depth (repeat "  "))) state " " node " " output-type " " label))
      (when (= :begin state)
        (swap! depth inc)))))

(defn make-tree-tracer
  "Creates a tree trace of the evaluation of the form
  {:node-id ... :output-type ... :label ... :state ... :dependencies [{...} ...]}

  You can also pass in a function to decorate the steps.

    Timing:
    (defn timing-decorator [step state]
      (case state
        :begin (assoc step :start-time (System/currentTimeMillis))
        (:end :fail) (-> step
                         (assoc :elapsed (- (System/currentTimeMillis) (:start-time step)))
                         (dissoc :start-time))))

    Weight:
    (defn weight-decorator [step state]
      (case state
        :begin step
        :end (assoc step :weight (+ 1 (reduce + 0 (map :weight (:dependencies step)))))
        :fail (assoc step :weight 0)))

    Depth:
    (defn- depth-decorator [step state]
      (case state
        :begin step
        :end (assoc step :depth (+ 1 (or (:depth (first (:dependencies step))) 0)))
        :fail (assoc step :depth 0)))

    (g/node-value node :output (g/make-evaluation-context {:tracer (g/make-tree-tracer result-atom timing-decorator)}))"
  ([result-atom]
   (make-tree-tracer result-atom (fn [step _] step)))
  ([result-atom step-decorator]
   (let [stack (atom '())]
     (fn [state node output-type label]
       (case state
         :begin
         (swap! stack conj (step-decorator {:node-id node :output-type output-type :label label :dependencies []} state))

         (:end :fail)
         (let [step (step-decorator (assoc (first @stack) :state state) state)]
           (swap! stack rest)
           (let [parent (first @stack)]
             (if parent
               (swap! stack #(conj (rest %) (update parent :dependencies conj step)))
               (reset! result-atom step)))))))))

(defn tree-trace-seq [result]
  (tree-seq :dependencies :dependencies result))

;; ---------------------------------------------------------------------------
;; Values
;; ---------------------------------------------------------------------------
(defn make-evaluation-context
  ([] (is/default-evaluation-context @*the-system*))
  ([options] (is/custom-evaluation-context @*the-system* options)))

(defn pruned-evaluation-context
  "Selectively filters out cache entries from the supplied evaluation context.
  Returns a new evaluation context with only the cache entries that passed the
  cache-entry-pred predicate. The predicate function will be called with
  node-id, output-label, evaluation-context and should return true if the
  cache entry for the output-label should remain in the cache."
  [evaluation-context cache-entry-pred]
  (in/pruned-evaluation-context evaluation-context cache-entry-pred))

(defn update-cache-from-evaluation-context!
  [evaluation-context]
  (swap! *the-system* is/update-cache-from-evaluation-context evaluation-context)
  nil)

(defmacro with-auto-evaluation-context [ec & body]
  `(let [~ec (make-evaluation-context)
         result# (do ~@body)]
     (update-cache-from-evaluation-context! ~ec)
     result#))

(def fake-system (is/make-system {:cache-size 0}))

(defmacro with-auto-or-fake-evaluation-context [ec & body]
  `(let [real-system# @*the-system*
         ~ec (is/default-evaluation-context (or real-system# fake-system))
         result# (do ~@body)]
     (when (some? real-system#)
       (update-cache-from-evaluation-context! ~ec))
     result#))

(defn node-value
  "Pull a value from a node's output, property or input, identified by `label`.
  The value may be cached, or it may be computed on demand. This is
  transparent to the caller.

  This uses the value of the node and its output at the time the
  evaluation context was created. If the evaluation context is left
  out, a context will be created from the current state of the system
  and the caller will receive a value consistent with the most
  recently committed transaction.

  The system cache is only updated automatically if the context was left
  out. If passed explicitly, you will need to update the cache
  manually by calling update-cache-from-evaluation-context!.

  Example:

  `(node-value node-id :chained-output)`"
  ([node-id label]
   (with-auto-evaluation-context evaluation-context
     (node-value node-id label evaluation-context)))
  ([node-id label evaluation-context]
   (when (some? node-id)
     (let [basis (:basis evaluation-context)
           node (gt/node-by-id-at basis node-id)]
       (in/node-value node label evaluation-context)))))

(defn valid-node-value
  "Like the node-value function, but throws an exception if evaluation produced
  an ErrorValue."
  ([node-id label]
   (with-auto-evaluation-context evaluation-context
     (valid-node-value node-id label evaluation-context)))
  ([node-id label evaluation-context]
   (let [value (node-value node-id label evaluation-context)]
     (if-not (error? value)
       value
       (let [node-type-kw (node-type-kw (:basis evaluation-context) node-id)]
         (throw
           (ex-info
             (format "Evaluation produced an ErrorValue from %s on %s %d."
                     label
                     (symbol node-type-kw)
                     node-id)
             {:node-type-kw node-type-kw
              :label label
              :error value})))))))

(defn maybe-node-value
  "Like the node-value function, but returns nil if evaluation produced an
  ErrorValue or the label does not exist on the node."
  ([node-id label]
   (with-auto-evaluation-context evaluation-context
     (maybe-node-value node-id label evaluation-context)))
  ([node-id label evaluation-context]
   (when (some? node-id)
     (let [basis (:basis evaluation-context)
           node (gt/node-by-id-at basis node-id)]
       (when (some-> node gt/node-type (in/behavior label))
         (let [value (in/node-value node label evaluation-context)]
           (when-not (error? value)
             value)))))))

(defmacro tx-cached-value!
  "Finds and returns a cached value at the specified key path of the
  :tx-data-context in the supplied evaluation-context. If no existing value was
  found in the :tx-data-context, evaluate the value-expr and store the value at
  the specified key path in the :tx-data-context, then return the value."
  [evaluation-context-expr tx-data-context-key-path-expr value-expr]
  `(let [tx-data-context-atom# (:tx-data-context ~evaluation-context-expr)
         tx-data-context-key-path# ~tx-data-context-key-path-expr
         cached-value# (-> tx-data-context-atom#
                           (deref)
                           (get-in tx-data-context-key-path# :dynamo.graph/not-found))]
     (case cached-value#
       :dynamo.graph/not-found
       (let [evaluated-value# ~value-expr]
         (swap! tx-data-context-atom# assoc-in tx-data-context-key-path# evaluated-value#)
         evaluated-value#)
       cached-value#)))

(defn tx-cached-node-value!
  "Like the node-value function, but caches the result in the :tx-data-context
  of the supplied evaluation-context if it doesn't have a cache. This is mostly
  a workaround for the fact that the evaluation-context supplied to property
  setters inside transactions does not contain a cache. Some operations, like
  resource lookups, can benefit hugely from caching the lookup maps. However,
  beware that this function bypasses the regular cache invalidation mechanism.
  Don't use it on outputs that might otherwise have been invalidated between two
  calls in the same transaction. The lack of a cache inside transactions has
  previously been reported as DEFEDIT-1411."
  [node-id label evaluation-context]
  (cond
    (nil? node-id)
    nil

    (some? (:cache evaluation-context))
    (node-value node-id label evaluation-context)

    :else
    (let [tx-data-context-key-path (pair :tx-node-value-cache (gt/endpoint node-id label))]
      (tx-cached-value! evaluation-context tx-data-context-key-path
        (node-value node-id label evaluation-context)))))

(definline raw-property-value*
  "Fast raw property access to the given node. Will bypass the property value
  fnk if present, but does traverse the override hierarchy. This can be more
  efficient than the node-value function, as it doesn't need to create an
  evaluation-context, check output jammers, and so on. It will simply return the
  value in the property map for the first node in the override chain that
  provides it. You should only really use this with properties that don't have a
  value fnk on nodes that you're sure haven't been marked defective."
  [basis node property-label]
  `(gt/get-property ~node ~basis ~property-label))

(definline raw-property-value
  "Fast raw property access to the given node-id. Will bypass the property value
  fnk if present, but does traverse the override hierarchy. This can be more
  efficient than the node-value function, as it doesn't need to create an
  evaluation-context, check output jammers, and so on. It will simply return the
  value in the property map for the first node in the override chain that
  provides it. You should only really use this with properties that don't have a
  value fnk on nodes that you're sure haven't been marked defective."
  [basis node-id property-label]
  `(let [basis# ~basis
         node# (gt/node-by-id-at basis# ~node-id)]
     (raw-property-value* basis# node# ~property-label)))

(defn graph-value
  "Returns the graph from the system given a graph-id and key.  It returns the graph at the point in time of the bais, if provided.
  If the basis is not provided, it will take it from the current point of time in the system.

  Example:

  `(graph-value (node->graph-id view) :renderer)`"
  ([graph-id k]
   (graph-value (now) graph-id k))
  ([basis graph-id k]
   (get-in basis [:graphs graph-id :graph-values k])))

;; ---------------------------------------------------------------------------
;; Interrogating the Graph
;; ---------------------------------------------------------------------------
(defn arcs->tuples
  [arcs]
  (ig/arcs->tuples arcs))

(defn inputs
  "Return the inputs to this node. Returns a collection like
  [[source-id output target-id input] [source-id output target-id input]...].

  If there are no inputs connected, returns an empty collection."
  ([node-id]       (inputs (now) node-id))
  ([basis node-id] (arcs->tuples (ig/inputs basis node-id))))

(defn labelled-inputs
  ([node-id label]       (labelled-inputs (now) node-id label))
  ([basis node-id label] (arcs->tuples (ig/inputs basis node-id label))))

(defn outputs
  "Return the outputs from this node. Returns a collection like
  [[source-id output target-id input] [source-id output target-id input]...].

  If there are no outputs connected, returns an empty collection."
  ([node-id]       (outputs (now) node-id))
  ([basis node-id] (arcs->tuples (ig/outputs basis node-id))))

(defn labelled-outputs
  ([node-id label]       (labelled-outputs (now) node-id label))
  ([basis node-id label] (arcs->tuples (ig/outputs basis node-id label))))

(defn explicit-inputs
  ([node-id]
   (explicit-inputs (now) node-id))
  ([basis node-id]
   (arcs->tuples (ig/explicit-inputs basis node-id))))

(defn explicit-outputs
  ([node-id]
   (explicit-outputs (now) node-id))
  ([basis node-id]
   (arcs->tuples (ig/explicit-outputs basis node-id))))

(defn node-feeding-into
  "Find the one-and-only node ID that sources this input on this node.
   Should you use this on an input label with multiple connections,
   the result is undefined."
  ([node-id label]
   (node-feeding-into (now) node-id label))
  ([basis node-id label]
   (ffirst (sources basis node-id label))))

(defn sources-of
  "Find the [node-id label] pairs for all connections into the given
  node's input label. The result is a sequence of pairs."
  ([node-id label]
   (sources-of (now) node-id label))
  ([basis node-id label]
   (gt/sources basis node-id label)))

(defn targets-of
  "Find the [node-id label] pairs for all connections out of the given
  node's output label. The result is a sequence of pairs."
  ([node-id label]
   (targets-of (now) node-id label))
  ([basis node-id label]
   (gt/targets basis node-id label)))

(defn find-node
  "Looks up nodes with a property that matches the given value. Exact
  equality is used. At present, this does a linear scan of all
  nodes. Future enhancements may offer indexing for faster access of
  some properties."
  [basis property-label expected-value]
  (gt/node-by-property basis property-label expected-value))

(defn invalidate-outputs!
  "Invalidate the given outputs and _everything_ that could be
  affected by them. Outputs are specified as a seq of Endpoints
  for both the argument and return value."
  ([outputs]
   (when-not (coll/empty? outputs)
     (swap! *the-system* is/invalidate-outputs outputs)
     nil)))

(defn cache-output-values!
  "Write the supplied key-value pairs to the cache. Downstream endpoints will be
  invalidated if the value differs from the previously cached entry."
  [endpoint+value-pairs]
  (when-not (coll/empty? endpoint+value-pairs)
    (swap! *the-system* is/cache-output-values endpoint+value-pairs)
    nil))

(defn node-instance*?
  "Returns true if the node is a member of a given type, including
   supertypes."
  [type node]
  (if-let [nt (and type (gt/node-type node))]
    (in/inherits? nt type)
    false))

(defn node-instance?
  "Returns true if the node is a member of a given type, including
   supertypes."
  ([type node-id]
   (node-instance? (now) type node-id))
  ([basis type node-id]
   (node-instance*? type (gt/node-by-id-at basis node-id))))

(defn node-kw-instance?
  "Returns true if the node is a member of a given type keyword, including
   supertypes."
  [basis type-kw node-id]
  (isa? (node-type-kw basis node-id) type-kw))

(defn node-instance-match*
  "Returns the first node-type from the provided sequence of node-types that
  matches the node or one of its supertypes, or nil if no match was found."
  [node node-types]
  (when-some [node-type-key (-> node gt/node-type deref :key)]
    (some (fn [type]
            (let [type-key (:key @type)]
              (when (isa? node-type-key type-key)
                type)))
          node-types)))

(defn node-instance-match
  "Returns the first node-type from the provided sequence of node-types that
  matches the node or one of its supertypes, or nil if no match was found."
  ([node-id node-types]
   (node-instance-match (now) node-id node-types))
  ([basis node-id node-types]
   (node-instance-match* (gt/node-by-id-at basis node-id) node-types)))

;; ---------------------------------------------------------------------------
;; Support for serialization, copy & paste, and drag & drop
;; ---------------------------------------------------------------------------
(def ^:private write-handlers (transit/record-write-handlers internal.node.NodeTypeRef internal.node.ValueTypeRef))
(def ^:private read-handlers  (transit/record-read-handlers  internal.node.NodeTypeRef internal.node.ValueTypeRef))

(defn read-graph
  "Read a graph fragment from a string. Returns a fragment suitable
  for pasting."
  ([s] (read-graph s {}))
  ([^String s extra-handlers]
   (let [handlers (merge read-handlers extra-handlers)
         reader   (transit/reader (ByteArrayInputStream. (.getBytes s "UTF-8")) :json {:handlers handlers})]
     (transit/read reader))))

(defn write-graph
  "Return a serialized string representation of the graph fragment."
  ([fragment]
   (write-graph fragment {}))
  ([fragment extra-handlers]
   (let [handlers (merge write-handlers extra-handlers)
         out      (ByteArrayOutputStream. 4096)
         writer   (transit/writer out :json {:handlers handlers})]
     (transit/write writer fragment)
     (.toString out "UTF-8"))))

(defn- serialize-arc [id-dictionary ^Arc arc]
  [(id-dictionary (.source-id arc))
   (.source-label arc)
   (id-dictionary (.target-id arc))
   (.target-label arc)])

(defn- in-same-graph? [_ ^Arc arc]
  (= (node-id->graph-id (.source-id arc))
     (node-id->graph-id (.target-id arc))))

(defn- every-arc-pred [& preds]
  (fn [basis ^Arc arc]
    (reduce (fn [_ pred]
              (if (pred basis arc)
                true
                (reduced false)))
            true
            preds)))

(defn- predecessors [pred basis node-id]
  (into []
        (keep (fn [^Arc arc]
                (when (pred basis arc)
                  (.source-id arc))))
        (ig/inputs basis node-id)))

(defn override-predecessors
  "This is an optimized version of the predecessors function above that is used
  when processing overrides. We do this a lot, since a complex project can have
  a lot of override nodes. Instead of taking a general traversal predicate, this
  optimized function inlines some aspects of the graph traversal, with a few
  minor optimizations that apply specifically to overrides. Specifically, this
  function will only traverse :cascade-delete inputs, and will ignore all
  connections that span multiple graphs."
  [pred basis ^long target-id]
  (when-some [target-node (gt/node-by-id-at basis target-id)]
    (let [graph-id (gt/node-id->graph-id target-id)
          graph (ig/node-id->graph basis target-id)
          graph-tarcs (:tarcs graph)]
      (loop [result []
             node-id target-id
             override-chain '()
             followed-inputs (in/cascade-deletes (gt/node-type target-node))]
        (let [arcs-by-input-label (graph-tarcs node-id)
              explicit-arcs (when arcs-by-input-label
                              (into []
                                    (comp
                                      (mapcat arcs-by-input-label)
                                      (filter (fn [^Arc arc]
                                                (and (= graph-id (gt/node-id->graph-id (.source-id arc)))
                                                     (pred basis arc)))))
                                    followed-inputs))
              source-node-ids (into []
                                    (comp
                                      (map gt/source-id)
                                      (distinct))
                                    explicit-arcs)
              result' (if (zero? (count source-node-ids))
                        result
                        (into result
                              (reduce (fn [source-node-ids override-id]
                                        (mapv (fn [source-node-id]
                                                (or (ig/override-of graph source-node-id override-id)
                                                    source-node-id))
                                              source-node-ids))
                                      source-node-ids
                                      override-chain)))
              node (ig/node-id->node graph node-id)
              original-node-id (gt/original node)]
          (if (nil? original-node-id)
            result'
            (recur result'
                   (long original-node-id)
                   (conj override-chain (gt/override-id node))
                   (persistent!
                     (transduce (map gt/target-label)
                                disj!
                                (transient followed-inputs)
                                explicit-arcs)))))))))

(defn- input-traverse
  [basis pred root-ids]
  (ig/pre-traverse basis root-ids (partial predecessors (every-arc-pred in-same-graph? pred))))

(defn default-node-serializer [node]
  (let [node-id                (gt/node-id node)
        all-node-properties    (into {} (map (fn [[key value]] [key (:value value)])
                                             (:properties (node-value node-id :_declared-properties))))
        properties-without-fns (util/filterm (comp not fn? val) all-node-properties)]
    {:node-type  (gt/node-type node)
     :properties properties-without-fns}))

(def opts-schema {(s/optional-key :traverse?) Runnable
                  (s/optional-key :serializer) Runnable
                  (s/optional-key :external-refs) {s/Int s/Keyword}
                  (s/optional-key :external-labels) {s/Int #{s/Keyword}}})

(defn override-originals
  "Given a node id, returns a sequence of node ids starting with the
  non-override node and proceeding with every override node leading
  up to and including the supplied node id."
  ([node-id]
   (override-originals (now) node-id))
  ([basis node-id]
   (ig/override-originals basis node-id)))

(defn- deep-arcs-by-source
  "Like arcs-by-source, but also includes connections from nodes earlier in the
  override chain. Note that arcs-by-target already does this."
  ([source-id]
   (deep-arcs-by-source (now) source-id))
  ([basis source-id]
   (into []
         (mapcat (partial gt/arcs-by-source basis))
         (override-originals basis source-id))))

(defn copy
  "Given a vector of root ids, and an options map that can contain an
  `:traverse?` predicate and a `serializer` function, returns a copy
  graph fragment that can be serialized or pasted.  Works on the
  current basis, if a basis is not provided.

   The `:traverse?` predicate determines whether the target node will be
  included at all. If it returns a falsey value, then traversal stops
  there. That node and all arcs to it will be left behind. If the
  predicate returns true, then that node --- or a stand-in for it ---
  will be included in the fragment.

  `:traverse?` will be called with the basis and an Arc.

   The `:serializer` function determines _how_ to represent the node
  in the fragment.  `dynamo.graph/default-node-serializer` adds a map
  with the original node's properties and node type. `paste` knows how
  to turn that map into a new (copied) node.

   You would use a `:serializer` function if you wanted to record a
  memo that could later be used to resolve and connect to an existing
  node rather than copy it.

  `:serializer` will be called with the node value. It must return an
   associative data structure (e.g., a map or a record).

   Note that connections to or from any nodes along the override chain
  will be flattened to source or target the serialized node instead of
  the nodes that it overrides.

   You can use `:external-refs` and `:external-labels` to add
  connections to nodes that are not copied, even when there exist
  connections to these nodes. To do this, need to specify a mapping
  from external node id to it's referenced id keyword (`:external-refs`)
  and then specify which labels of that node we are interested in when
  performing a copy (`:external-labels`). Note that you need to provide
  an inverse map of external refs (a map from reference id keyword to
  node id) when doing paste for it to succeed.

  Example:

  `(g/copy root-ids {:traverse? (comp not resource-node-id? (fn [basis ^Arc arc] (.target-id arc))
                     :serializer (some-fn custom-serializer default-node-serializer %)
                     :external-refs {project :project}
                     :external-labels {project #{:settings}}})"
  ([root-ids opts]
   (copy (now) root-ids opts))
  ([basis root-ids {:keys [traverse? serializer external-refs external-labels]
                    :or {traverse? fn/constantly-false
                         serializer default-node-serializer}
                    :as opts}]
   (s/validate opts-schema opts)
   (let [arcs-by-source (partial deep-arcs-by-source basis)
         arcs-by-target (partial gt/arcs-by-target basis)
         serializer #(assoc (serializer (gt/node-by-id-at basis %2)) :serial-id %1)
         original-ids (input-traverse basis traverse? root-ids)
         replacements (zipmap original-ids (map-indexed serializer original-ids))
         serial-ids (into {}
                          (mapcat (fn [[original-id {serial-id :serial-id}]]
                                    (map #(vector % serial-id)
                                         (override-originals basis original-id))))
                          replacements)
         include-arc? (partial ig/arc-endpoints-p
                               (fn [id label]
                                 (or (contains? serial-ids id)
                                     (and (contains? external-refs id)
                                          (contains? (get external-labels id) label)))))
         serialize-dictionary (into serial-ids external-refs)
         serialize-arc (partial serialize-arc serialize-dictionary)
         incoming-arcs (mapcat arcs-by-target original-ids)
         outgoing-arcs (mapcat arcs-by-source original-ids)
         fragment-arcs (into []
                             (comp (filter include-arc?)
                                   (map serialize-arc)
                                   (distinct))
                             (concat incoming-arcs outgoing-arcs))]
     {:roots (mapv serial-ids root-ids)
      :nodes (vec (vals replacements))
      :arcs fragment-arcs
      :node-id->serial-id serial-ids})))

(defn- deserialize-arc
  [id-dictionary arc]
  (-> arc
      (update 0 id-dictionary)
      (update 2 id-dictionary)
      (->> (apply connect))))

(defn default-node-deserializer
  [basis graph-id {:keys [node-type properties]}]
  (construct-node-with-id graph-id node-type properties))

(defn paste
  "Given a `graph-id` and graph fragment from copying, provides the
  transaction data to create the nodes on the graph and connect all
  the new nodes together with the same arcs in the fragment.  It will
  take effect when it is applied with a transact.

  Any nodes that were replaced during the copy must be resolved into
  real nodes here. That is the job of the `:deserializer` function. It
  receives the current basis, the graph-id being pasted into, and
  whatever data the :serializer function returned.

  `dynamo.graph/default-node-deserializer` creates new nodes (copies)
  from the fragment. Yours may look up nodes in the world, create new
  instances, or anything else. The deserializer _must_ return valid
  transaction data, even if that data is just an empty vector.

  If you serialized any arcs to external nodes when doing a copy, you
  need to provide `:external-refs` here to resolve references to
  external nodes - a map from reference id keyword to a referenced
  node id.

  Example:

  `(g/paste (graph project) fragment {:deserializer default-node-deserializer
                                      :external-refs {:project project}})"
  ([graph-id fragment opts]
   (paste (now) graph-id fragment opts))
  ([basis graph-id fragment {:keys [deserializer external-refs]
                             :or {deserializer default-node-deserializer
                                  external-refs {}}}]
   (let [deserializer  (partial deserializer basis graph-id)
         nodes         (map deserializer (:nodes fragment))
         new-nodes     (remove #(gt/node-by-id-at basis (gt/node-id %)) nodes)
         node-txs      (vec (mapcat it/new-node new-nodes))
         node-ids      (map gt/node-id nodes)
         id-dictionary (zipmap (map :serial-id (:nodes fragment)) node-ids)
         deserialize-dictionary (into id-dictionary external-refs)
         connect-txs   (mapcat #(deserialize-arc deserialize-dictionary %) (:arcs fragment))]
     {:root-node-ids      (map id-dictionary (:roots fragment))
      :nodes              node-ids
      :tx-data            (into node-txs connect-txs)
      :serial-id->node-id id-dictionary})))

;; ---------------------------------------------------------------------------
;; Sub-graph instancing
;; ---------------------------------------------------------------------------

(defn make-override-traverse-fn [traverse?]
  (partial override-predecessors traverse?))

(def always-override-traverse-fn
  (make-override-traverse-fn
    (fn always-override-traverse-fn [_basis ^Arc _arc]
      true)))

(def never-override-traverse-fn
  (make-override-traverse-fn
    (fn never-override-traverse-fn [_basis ^Arc _arc]
      false)))

(defn default-override-init-fn [_evaluation-context _original-node-id->override-node-id]
  [])

(defn default-override-properties-by-node-id [_node-id]
  {})

(defn override
  ([root-id]
   (override root-id {} default-override-init-fn))
  ([root-id opts]
   (override root-id opts default-override-init-fn))
  ([root-id opts init-fn]
   (let [{:keys [traverse-fn init-props-fn properties-by-node-id]
          :or {traverse-fn always-override-traverse-fn
               properties-by-node-id default-override-properties-by-node-id}} opts]
     (it/override root-id traverse-fn init-props-fn init-fn properties-by-node-id))))

(defn transfer-overrides [from-id->to-id]
  (it/transfer-overrides from-id->to-id))

(defn overrides
  ([root-id]
   (overrides (now) root-id))
  ([basis root-id]
   (ig/get-overrides basis root-id)))

(defn override-original
  ([node-id]
   (override-original (now) node-id))
  ([basis node-id]
   (ig/override-original basis node-id)))

(defn override-root
  ([node-id]
   (override-root (now) node-id))
  ([basis node-id]
   (if-some [original (some->> node-id (override-original basis))]
     (recur basis original)
     node-id)))

(defn override?
  ([node-id]
   (override? (now) node-id))
  ([basis node-id]
   (some? (override-original basis node-id))))

(defn override-id
  ([node-id]
   (override-id (now) node-id))
  ([basis node-id]
   (when-some [node (node-by-id basis node-id)]
     (:override-id node))))

(defn node-property-overridden? [node property]
  (gt/property-overridden? node property))

(defn property-overridden?
  ([node-id property]
   (property-overridden? (now) node-id property))
  ([basis node-id property]
   (if-let [node (node-by-id basis node-id)]
     (node-property-overridden? node property)
     false)))

(defn property-value-origin?
  ([node-id prop-kw]
   (property-value-origin? (now) node-id prop-kw))
  ([basis node-id prop-kw]
   (if (override? basis node-id)
     (property-overridden? basis node-id prop-kw)
     true)))

(defn node-property-dynamic
  "Returns the value of a dynamic associated with a specific node property
  If the dynamic does not exist, returns the provided not-found value, or throws
  an assertion error if there was no not-found value provided"
  ([node property-label dynamic-label evaluation-context]
   (let [ret (node-property-dynamic node property-label dynamic-label ::not-found evaluation-context)]
     (assert (not (identical? ret ::not-found))
             (format "Dynamic %s not found on property %s in node-type %s." dynamic-label property-label (:k (node-type node))))
     ret))
  ([node property-label dynamic-label not-found evaluation-context]
   (let [node-type (node-type node)
         property-def (get (in/all-properties node-type) property-label)
         _ (assert property-def (format "Property %s not found in node-type %s." property-label (:k node-type)))
         dynamic-fnk (-> property-def (get :dynamics) (get dynamic-label))]
     (if dynamic-fnk
       (let [dynamic-fn (:fn dynamic-fnk)
             dynamic-args (coll/transfer (:arguments dynamic-fnk) {}
                            (map (fn [arg-label]
                                     (let [arg-value (case arg-label
                                                       :_this node
                                                       (in/node-value node arg-label evaluation-context))]
                                       (pair arg-label arg-value)))))]
         (dynamic-fn dynamic-args))
       not-found))))

(defmulti node-key
  "Used to identify a node uniquely within a scope. This has various uses,
  among them is that we will restore overridden properties during resource sync
  for nodes that return a non-nil node-key. Usually this only happens for
  ResourceNodes, but this also enables us to restore overridden properties on
  nodes produced by the resource :load-fn."
  (fn [node-id {:keys [basis] :as _evaluation-context}]
    (if-some [node-type-kw (node-type-kw basis node-id)]
      node-type-kw
      (throw (ex-info (str "Unknown node id: " node-id)
                      {:node-id node-id})))))

(defmethod node-key :default [_node-id _evaluation-context] nil)

(defn overridden-properties
  "Returns a map of overridden prop-keywords to property values. Values will be
  produced by property value functions if possible."
  [node-id {:keys [basis] :as evaluation-context}]
  (if-let [node (node-by-id basis node-id)]
    (let [node-type (gt/node-type node)]
      (into {}
            (map (fn [[prop-kw raw-prop-value]]
                   [prop-kw (if (has-property? node-type prop-kw)
                              (in/node-value node prop-kw evaluation-context)
                              raw-prop-value)]))
            (in/node-value node :_overridden-properties evaluation-context)))
    {}))

(defn collect-overridden-properties
  "Collects overridden property values from override nodes originating from the
  scope of the specified source-node-id. The idea is that one should be able to
  delete source-node-id, recreate it from disk, and re-apply the collected
  property values to the freshly created override nodes resulting from the
  resource :load-fn. Overridden properties will be collected from nodes whose
  node-key multi-method implementation returns a non-nil value. This will be
  used as a key along with the override-id to uniquely identify the node among
  the new set of nodes created by the :load-fn."
  ([source-node-id]
   (with-auto-evaluation-context evaluation-context
     (collect-overridden-properties source-node-id evaluation-context)))
  ([source-node-id {:keys [basis] :as evaluation-context}]
   (persistent!
     (reduce
       (fn [properties-by-override-node-key override-node-id]
         (or (when-some [override-id (override-id basis override-node-id)]
               (when-some [node-key (node-key override-node-id evaluation-context)]
                 (let [override-node-key [override-id node-key]
                       overridden-properties (overridden-properties override-node-id evaluation-context)]
                   (if (contains? properties-by-override-node-key override-node-key)
                     (let [node-type-kw (node-type-kw basis override-node-id)]
                       (throw
                         (ex-info
                           (format "Duplicate node key `%s` from %s"
                                   node-key
                                   node-type-kw)
                           {:node-key node-key
                            :node-type node-type-kw
                            :source-node-id source-node-id
                            :override-node-id override-node-id
                            :override-node-key override-node-key
                            :properties-by-override-node-key (persistent! properties-by-override-node-key)})))
                     (when (seq overridden-properties)
                       (assoc! properties-by-override-node-key
                               override-node-key overridden-properties))))))
             properties-by-override-node-key))
       (transient {})
       (ig/pre-traverse basis [source-node-id] ig/cascade-delete-sources)))))

(defn restore-overridden-properties
  "Restores collected-properties obtained from the collect-overridden-properties
  function to override nodes originating from the scope of the specified
  target-node-id. Returns a sequence of transaction steps. Target nodes are
  identified by the value returned from their node-key multi-method
  implementation along with the override-id that produced them."
  ([target-node-id collected-properties]
   (with-auto-evaluation-context evaluation-context
     (restore-overridden-properties target-node-id collected-properties evaluation-context)))
  ([target-node-id collected-properties {:keys [basis] :as evaluation-context}]
   (for [node-id (ig/pre-traverse basis [target-node-id] ig/cascade-delete-sources)]
     (when-some [override-id (override-id basis node-id)]
       (when-some [node-key (node-key node-id evaluation-context)]
         (let [override-node-key [override-id node-key]
               overridden-properties (collected-properties override-node-key)]
           (for [[prop-kw prop-value] overridden-properties]
             (set-property node-id prop-kw prop-value))))))))

;; ---------------------------------------------------------------------------
;; Boot, initialization, and facade
;; ---------------------------------------------------------------------------
(defn initialize!
  "Set up the initial system including graphs, caches, and disposal queues"
  [config]
  (reset! *the-system* (is/make-system config))
  (low-memory/add-callback!
    (fn low-memory-callback! []
      (log/info :message "Clearing the system cache in desperation due to low-memory conditions.")
      (clear-system-cache!))))

(defn make-graph!
  "Create a new graph in the system with optional values of `:history` and `:volatility`. If no
  options are provided, the history ability is false and the volatility is 0

  Example:

  `(make-graph! :history true :volatility 1)`"
  [& {:keys [history volatility] :or {history false volatility 0}}]
  (let [g (assoc (ig/empty-graph) :_volatility volatility)
        s (swap! *the-system* (if history is/attach-graph-with-history is/attach-graph) g)]
    (:last-graph s)))

(defn last-graph-added
  "Retuns the last graph added to the system"
  []
  (is/last-graph @*the-system*))

(defn graph-version
  "Returns the latest version of a graph id"
  [graph-id]
  (is/graph-time @*the-system* graph-id))

(defn delete-graph!
  "Given a `graph-id`, deletes it from the system

  Example:

  ` (delete-graph! agraph-id)`"
  [graph-id]
  (when-let [graph (is/graph @*the-system* graph-id)]
    (transact (mapv it/delete-node (ig/node-ids graph)))
    (swap! *the-system* is/detach-graph graph-id)
    nil))

(defn undo!
  "Given a `graph-id` resets the graph back to the last _step_ in time.

  Example:

  (undo gid)"
  [graph-id]
  (swap! *the-system* is/undo-history graph-id)
  nil)

(defn has-undo?
  "Returns true/false if a `graph-id` has an undo available"
  [graph-id]
  (let [undo-stack (is/undo-stack (is/graph-history @*the-system* graph-id))]
    (not (empty? undo-stack))))

(defn undo-stack-count
  "Returns the number of entries in the undo stack for `graph-id`"
  [graph-id]
  (let [undo-stack (is/undo-stack (is/graph-history @*the-system* graph-id))]
    (count undo-stack)))

(defn redo!
  "Given a `graph-id` reverts an undo of the graph

  Example: `(redo gid)`"
  [graph-id]
  (swap! *the-system* is/redo-history graph-id)
  nil)

(defn has-redo?
  "Returns true/false if a `graph-id` has an redo available"
  [graph-id]
  (let [redo-stack (is/redo-stack (is/graph-history @*the-system* graph-id))]
    (not (empty? redo-stack))))

(defn reset-undo!
  "Given a `graph-id`, clears all undo history for the graph

  Example:
  `(reset-undo! gid)`"
  [graph-id]
  (swap! *the-system* is/clear-history graph-id)
  nil)

(defn cancel!
  "Given a `graph-id` and a `sequence-id` _cancels_ any sequence of undos on the graph as
  if they had never happened in the history.

  Example:
  `(cancel! gid :a)`"
  [graph-id sequence-id]
  (swap! *the-system* is/cancel graph-id sequence-id)
  nil)

(defn evaluation-context?
  "Check if a value is an evaluation context"
  [x]
  (and (map? x)
       (contains? x :basis)
       (contains? x :in-production)
       (contains? x :local)
       (contains? x :hits)))

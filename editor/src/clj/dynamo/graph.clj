;; Copyright 2020-2022 The Defold Foundation
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
  (:refer-clojure :exclude [deftype constantly])
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
            [schema.core :as s])
  (:import [internal.graph.error_values ErrorValue]
           [internal.graph.types Arc]
           [java.io ByteArrayInputStream ByteArrayOutputStream]))

(set! *warn-on-reflection* true)

(namespaces/import-vars [internal.graph.types node-id->graph-id node->graph-id sources targets connected? dependencies Node node-id node-id? produce-value node-by-id-at endpoint-node-id endpoint-label])

(namespaces/import-vars [internal.graph.error-values ->error error-aggregate error-fatal error-fatal? error-info error-info? error-message error-package? error-warning error-warning? error? flatten-errors map->error package-errors precluding-errors unpack-errors worse-than package-if-error])

(namespaces/import-vars [internal.node value-type-schema value-type? isa-node-type? value-type-dispatch-value has-input? has-output? has-property? type-compatible? merge-display-order NodeType supertypes declared-properties declared-property-labels declared-inputs declared-outputs cached-outputs input-dependencies input-cardinality cascade-deletes substitute-for input-type output-type input-labels output-labels property-display-order])

(namespaces/import-vars [internal.graph arc node-ids pre-traverse])

(let [graph-id ^java.util.concurrent.atomic.AtomicInteger (java.util.concurrent.atomic.AtomicInteger. 0)]
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

(defn node-type*
  "Return the node-type given a node-id.  Uses the current basis if not provided."
  ([node-id]
   (node-type* (now) node-id))
  ([basis node-id]
   (when-let [n (gt/node-by-id-at basis node-id)]
     (gt/node-type n))))

(defn node-type
  "Return the node-type given a node. Uses the current basis if not provided."
  [node]
  (when node
    (gt/node-type node)))

(defn node-override? [node]
  (some? (gt/original node)))

(defn cache "The system cache of node values"
  []
  (is/system-cache @*the-system*))

(defn endpoint [node-id label]
  (gt/endpoint node-id label))

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
  ([metrics-collector txs]
   (when *tps-debug*
     (send-off tps-counter tick (System/nanoTime)))
   (let [system (deref *the-system*)
         basis (is/basis system)
         id-generators (is/id-generators system)
         override-id-generator (is/override-id-generator system)
         tx-result (it/transact* (it/new-transaction-context basis id-generators override-id-generator metrics-collector) txs)]
     (when (= :ok (:status tx-result))
       (swap! *the-system* is/merge-graphs (get-in tx-result [:basis :graphs]) (:graphs-modified tx-result) (:outputs-modified tx-result) (:nodes-deleted tx-result)))
     tx-result)))

;; ---------------------------------------------------------------------------
;; Using transaction data
;; ---------------------------------------------------------------------------
(defn tx-data-nodes-added
  "Returns a list of the node-ids added given a list of transaction steps, (tx-data)."
  [txs]
  (keep (fn [tx-data]
          (case (:type tx-data)
            :create-node (-> tx-data :node :_node-id)
            nil))
        (flatten txs)))

;; ---------------------------------------------------------------------------
;; Using transaction values
;; ---------------------------------------------------------------------------
(defn tx-nodes-added
 "Returns a list of the node-ids added given a result from a transaction, (tx-result)."
  [transaction]
  (:nodes-added transaction))

(defn is-added?
  "Returns a boolean if a node was added as a result of a transaction given a tx-result and node."
  [transaction node-id]
  (contains? (:nodes-added transaction) node-id))

(defn is-deleted?
  "Returns a boolean if a node was delete as a result of a transaction given a tx-result and node."
  [transaction node-id]
  (contains? (:nodes-deleted transaction) node-id))

(defn transaction-basis
  "Returns the final basis from the result of a transaction given a tx-result"
  [transaction]
  (:basis transaction))

(defn pre-transaction-basis
  "Returns the original, starting basis from the result of a transaction given a tx-result"
  [transaction]
  (:original-basis transaction))

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
        arglist      (interleave argv (map #(list `get param %) kargv))]
    (if alias
      `(with-meta
         (fn [~param]
           (let [~alias (select-keys ~param ~kargv)
                 ~@(vec arglist)]
             ~@tail))
         {:arguments (quote ~kargv)})
      `(with-meta
         (fn [~param]
           (let ~(vec arglist) ~@tail))
         {:arguments (quote ~kargv)}))))

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
     (s/optional-key :display-order) [(s/conditional vector? [(s/one String "category") s/Keyword] keyword? s/Keyword)]})
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
  "Set a human-readable label to describe the current transaction."
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
  "Call the specified function with args when reaching the transaction step"
  [f & args]
  (it/callback f args))

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

(defn set-property
  "Creates the transaction step to assign a value to a node's property (or properties) value(s).  It will take effect when the transaction
  is applies in a transact.

  Example:

  `(transact (set-property root-id :touched 1))`"
  [node-id & kvs]
  (assert node-id)
  (mapcat
   (fn [[p v]]
     (it/update-property node-id p (clojure.core/constantly v) []))
   (partition-all 2 kvs)))

(defn set-property!
  "Creates the transaction step to assign a value to a node's property (or properties) value(s) and applies it in a transaction.
  It returns the result of the transaction, (tx-result).

  Example:

  `(set-property! root-id :touched 1)`"
  [node-id & kvs]
  (assert node-id)
  (transact (apply set-property node-id kvs)))

(defn update-property
  "Create the transaction step to apply a function to a node's property in a transaction. The
  function f will be invoked as if by (apply f current-value args).  It will take effect when the transaction is
  applied in a transact.

  Example:

  `(transact (g/update-property node-id :int-prop inc))`"
  [node-id p f & args]
  (assert node-id)
  (it/update-property node-id p f args))

(defn update-property-ec
  "Same as update-property, but injects the in-transaction evaluation-context
  as the first argument to the update-fn."
  [node-id p f & args]
  (assert node-id)
  (it/update-property-ec node-id p f args))

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

(defn invalidate
 "Creates the transaction step to invalidate all the outputs of the node.  It will take effect when the transaction is
  applied in a transact.

  Example:

  `(transact (invalidate node-id))`"
  [node-id]
  (assert node-id)
  (it/invalidate node-id))

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

(defn- do-node-value [node-id label evaluation-context]
  (is/node-value @*the-system* node-id label evaluation-context))

(defn node-value
  "Pull a value from a node's output, property or input, identified by `label`.
  The value may be cached or it may be computed on demand. This is
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
     (do-node-value node-id label evaluation-context)))
  ([node-id label evaluation-context]
   (do-node-value node-id label evaluation-context)))

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
  affected by them. Outputs are specified as pairs of [node-id label]
  for both the argument and return value."
  ([outputs]
   (swap! *the-system* is/invalidate-outputs outputs)
   nil))

(defn node-instance*?
  "Returns true if the node is a member of a given type, including
   supertypes."
  [type node]
  (if-let [nt (and type (gt/node-type node))]
    (isa? (:key @nt) (:key @type))
    false))

(defn node-instance?
  "Returns true if the node is a member of a given type, including
   supertypes."
  ([type node-id]
   (node-instance? (now) type node-id))
  ([basis type node-id]
   (node-instance*? type (gt/node-by-id-at basis node-id))))

(defn node-instance-of-any*?
  "Returns true if the node is a member of any of the given types, including
   their supertypes."
  [node types]
  (let [node-type-key (-> node gt/node-type deref :key)]
    (some? (and node-type-key
                (some (fn [type]
                        (let [type-key (:key @type)]
                          (when (isa? node-type-key type-key)
                            type)))
                      types)))))

(defn node-instance-of-any?
  "Returns true if the node is a member of any of the given types, including
  their supertypes."
  ([node-id types]
   (node-instance-of-any? (now) node-id types))
  ([basis node-id types]
   (node-instance-of-any*? (gt/node-by-id-at basis node-id) types)))

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
                  (s/optional-key :serializer) Runnable})

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

  Example:

  `(g/copy root-ids {:traverse? (comp not resource-node-id? (fn [basis ^Arc arc] (.target-id arc))
                     :serializer (some-fn custom-serializer default-node-serializer %)})"
  ([root-ids opts]
   (copy (now) root-ids opts))
  ([basis root-ids {:keys [traverse? serializer] :or {traverse? (clojure.core/constantly false) serializer default-node-serializer} :as opts}]
   (s/validate opts-schema opts)
   (let [arcs-by-source (partial deep-arcs-by-source basis)
         arcs-by-target (partial gt/arcs-by-target basis)
         serializer     #(assoc (serializer (gt/node-by-id-at basis %2)) :serial-id %1)
         original-ids   (input-traverse basis traverse? root-ids)
         replacements   (zipmap original-ids (map-indexed serializer original-ids))
         serial-ids     (into {}
                              (mapcat (fn [[original-id {serial-id :serial-id}]]
                                        (map #(vector % serial-id)
                                             (override-originals basis original-id))))
                              replacements)
         include-arc?   (partial ig/arc-endpoints-p (partial contains? serial-ids))
         serialize-arc  (partial serialize-arc serial-ids)
         incoming-arcs  (mapcat arcs-by-target original-ids)
         outgoing-arcs  (mapcat arcs-by-source original-ids)
         fragment-arcs  (into []
                              (comp (filter include-arc?)
                                    (map serialize-arc)
                                    (distinct))
                              (concat incoming-arcs outgoing-arcs))]
     {:roots              (mapv serial-ids root-ids)
      :nodes              (vec (vals replacements))
      :arcs               fragment-arcs
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

  Example:

  `(g/paste (graph project) fragment {:deserializer default-node-deserializer})"
  ([graph-id fragment opts]
   (paste (now) graph-id fragment opts))
  ([basis graph-id fragment {:keys [deserializer] :or {deserializer default-node-deserializer} :as opts}]
   (let [deserializer  (partial deserializer basis graph-id)
         nodes         (map deserializer (:nodes fragment))
         new-nodes     (remove #(gt/node-by-id-at basis (gt/node-id %)) nodes)
         node-txs      (vec (mapcat it/new-node new-nodes))
         node-ids      (map gt/node-id nodes)
         id-dictionary (zipmap (map :serial-id (:nodes fragment)) node-ids)
         connect-txs   (mapcat #(deserialize-arc id-dictionary %) (:arcs fragment))]
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
   (let [{:keys [traverse-fn properties-by-node-id]
          :or {traverse-fn always-override-traverse-fn
               properties-by-node-id default-override-properties-by-node-id}} opts]
     (it/override root-id traverse-fn init-fn properties-by-node-id))))

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
   (not (nil? (override-original basis node-id)))))

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

(defn node-type-kw
  "Returns the fully-qualified keyword that corresponds to the node type of the
  specified node id, or nil if the node does not exist."
  ([node-id]
   (:k (node-type* node-id)))
  ([basis node-id]
   (:k (node-type* basis node-id))))

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
  (let [node-type (node-type* basis node-id)]
    (into {}
          (map (fn [[prop-kw raw-prop-value]]
                 [prop-kw (if (has-property? node-type prop-kw)
                            (node-value node-id prop-kw evaluation-context)
                            raw-prop-value)]))
          (node-value node-id :_overridden-properties evaluation-context))))

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
                            :node-type node-type-kw})))
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
  (low-memory/add-callback! clear-system-cache!))

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

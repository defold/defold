(ns dynamo.graph
  "Main api for graph and node"
  (:refer-clojure :exclude [deftype constantly])
  (:require [clojure.set :as set]
            [clojure.tools.macro :as ctm]
            [cognitect.transit :as transit]
            [internal.util :as util]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [internal.node :as in]
            [internal.system :as is]
            [internal.transaction :as it]
            [plumbing.core :as pc]
            [potemkin.namespaces :as namespaces]
            [schema.core :as s])
  (:import [internal.graph.types Arc]
           [internal.graph.error_values ErrorValue]
           [java.io ByteArrayInputStream ByteArrayOutputStream]))

(set! *warn-on-reflection* true)

(namespaces/import-vars [internal.graph.types node-id->graph-id node->graph-id sources targets connected? dependencies Node node-id produce-value node-by-id-at])

(namespaces/import-vars [internal.graph.error-values error-info error-warning error-fatal ->error error? error-info? error-warning? error-fatal? error-aggregate flatten-errors package-errors precluding-errors unpack-errors worse-than])

(namespaces/import-vars [internal.node value-type-schema value-type? isa-node-type? value-type-dispatch-value has-input? has-output? has-property? type-compatible? merge-display-order NodeType supertypes internal-property-labels declared-properties declared-property-labels externs declared-inputs declared-outputs cached-outputs input-dependencies input-cardinality cascade-deletes substitute-for input-type output-type input-labels output-labels property-display-order])

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

(defmacro with-system [sys & body]
  `(binding [*the-system* (atom ~sys)]
     ~@body))

(defn node-by-id
  "Returns a node given its id. If the basis is provided, it returns the value of the node using that basis.
   Otherwise, it uses the current basis."
  ([node-id]
   (let [graph-id (node-id->graph-id node-id)]
     (ig/graph->node (is/graph @*the-system* graph-id) node-id)))
  ([basis node-id]
   (gt/node-by-id-at basis node-id)))

(defn node-type*
  "Return the node-type given a node-id.  Uses the current basis if not provided."
  ([node-id]
   (node-type* (now) node-id))
  ([basis node-id]
   (when-let [n (gt/node-by-id-at basis node-id)]
     (gt/node-type n basis))))

(defn node-type
  "Return the node-type given a node. Uses the current basis if not provided."
  ([node]
    (node-type (now) node))
  ([basis node]
    (when node
      (gt/node-type node basis))))

(defn cache "The system cache of node values"
  []
  (is/system-cache @*the-system*))

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
  [txs]
  (when *tps-debug*
    (send-off tps-counter tick (System/nanoTime)))
  (let [basis     (is/basis @*the-system*)
        id-gens   (is/id-generators @*the-system*)
        tx-result (it/transact* (it/new-transaction-context basis id-gens) txs)]
    (when (= :ok (:status tx-result))
      (is/merge-graphs! @*the-system* (get-in tx-result [:basis :graphs]) (:graphs-modified tx-result) (:outputs-modified tx-result) (:nodes-deleted tx-result)))
    tx-result))

;; ---------------------------------------------------------------------------
;; Using transaction values
;; ---------------------------------------------------------------------------
(defn tx-nodes-added
 "Returns a list of the node-ids added given a result from a transaction, (tx-result)."
  [transaction]
  (:nodes-added transaction))

(defn is-modified?
  "Returns a boolean if a node, or node and output, was modified as a result of a transaction given a tx-result."
  ([transaction node-id]
    (boolean (contains? (:outputs-modified transaction) node-id)))
  ([transaction node-id output]
    (boolean (get-in transaction [:outputs-modified node-id output]))))

(defn is-added? [transaction node-id]
  "Returns a boolean if a node was added as a result of a transaction given a tx-result and node."
  (contains? (:nodes-added transaction) node-id))

(defn is-deleted? [transaction node-id]
  "Returns a boolean if a node was delete as a result of a transaction given a tx-result and node."
  (contains? (:nodes-deleted transaction) node-id))

(defn outputs-modified
  "Returns the pairs of node-id and label of the outputs that were modified for a node as the result of a transaction given a tx-result and node"
  [transaction node-id]
  (get-in transaction [:outputs-modified node-id]))

(defn transaction-basis
  "Returns the final basis from the result of a transaction given a tx-result"
  [transaction]
  (:basis transaction))

(defn pre-transaction-basis
  "Returns the original, statrting basis from the result of a transaction given a tx-result"
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
  (let [fqs           (symbol (str *ns*) (str symb))
        key           (keyword fqs)]
    `(do
       (in/register-value-type '~fqs ~key)
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
                             :value                                (s/either s/Any ErrorValue)
                             :type                                 s/Any
                             s/Keyword                             s/Any}}
     (s/optional-key :node-id) s/Int
     (s/optional-key :display-order) [(s/either s/Keyword [(s/one String "category") s/Keyword])]})
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

(defn- var-it [it] (list `var it))

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
  (binding [in/*autotypes* (atom {})]
    (let [[symb forms]  (ctm/name-with-attributes symb body)
          fqs           (symbol (str *ns*) (str symb))
          node-type-def (in/process-node-type-forms fqs forms)
          fn-paths      (in/extract-functions node-type-def)
          fn-defs       (for [[path func] fn-paths]
                          (list `def (in/dollar-name symb path) func))
          fwd-decls     (map (fn [d] (list `def (second d))) fn-defs)
          node-type-def (util/update-paths node-type-def fn-paths
                                           (fn [path func curr]
                                             (assoc curr :fn (var-it (in/dollar-name symb path)))))
          node-key      (:key node-type-def)
          derivations   (for [tref (:supertypes node-type-def)]
                          `(when-not (contains? (descendants ~(:key (deref tref))) ~node-key)
                             (derive ~node-key ~(:key (deref tref)))))
          node-type-def (update node-type-def :supertypes #(list `quote %))
          type-name (str symb)
          runtime-definer (symbol (str symb "*"))
          type-regs     (for [[vtr ctor] @in/*autotypes*] `(in/register-value-type ~vtr ~ctor))]
      ;; This try-block was an attempt to catch "Code too large" errors when method size exceeded 64kb in the JVM.
      ;; Surprisingly, the addition of the try-block stopped the error from happening, so leaving it here.
      ;; "Problem solved!" lol
      `(try
         (do
           (declare ~symb)
           ~@type-regs
           ~@fwd-decls
           ~@fn-defs
           (defn ~runtime-definer [] ~node-type-def)
           (def ~symb (in/register-node-type ~node-key (in/map->NodeTypeImpl (~runtime-definer))))
           ~@derivations
           (var ~symb))
         (catch RuntimeException e#
           (prn (format "defnode exception while generating code for %s" ~type-name))
           (throw e#))))))

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

(defn prev-sequence-label [graph]
  (let [sys @*the-system*]
    (when-let [prev-step (some-> (is/graph-history sys graph)
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

(defn connect
  "Make a connection from an output of the source node to an input on the target node.
   Takes effect when a transaction is applied.

  Example:

  `(transact (connect content-node :scalar view-node :first-name))`"
  [source-node-id source-label target-node-id target-label]
  (assert source-node-id)
  (assert target-node-id)
  (it/connect source-node-id source-label target-node-id target-label))

(defn connect!
 "Creates the transaction step to make a connection from an output of the source node to an input on the target node
  and applies it in a transaction

  Example:

  `(connect! content-node :scalar view-node :first-name)`"
  [source-node-id source-label target-node-id target-label]
  (assert source-node-id)
  (assert target-node-id)
  (transact (connect source-node-id source-label target-node-id target-label)))

(defn disconnect
  "Creates the transaction step to remove a connection from an output of the source node to the input on the target node.
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.  Takes effect when a transaction
  is applied with transact.

  Example:

  (`transact (disconnect aux-node :scalar view-node :last-name))`"
  [source-node-id source-label target-node-id target-label]
  (assert source-node-id)
  (assert target-node-id)
  (it/disconnect source-node-id source-label target-node-id target-label))

(defn disconnect!
 "Creates the transaction step to remove a connection from an output of the source node to the input on the target node.
  It also applies it in transaction, returning the transaction result, (tx-result).
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.

  Example:

  `(disconnect aux-node :scalar view-node :last-name)`"
  [source-node-id source-label target-node-id target-label]
  (transact (disconnect source-node-id source-label target-node-id target-label)))

(defn disconnect-sources
  ([target-node-id target-label]
    (disconnect-sources (now) target-node-id target-label))
  ([basis target-node-id target-label]
   (assert target-node-id)
    (it/disconnect-sources basis target-node-id target-label)))

(defn become
  "Creates the transaction step to turn one kind of node into another, in a transaction. All properties and their values
   will be carried over from source-node to new-node. The resulting node will still have
   the same node-id.

  Example:

  `(transact (become counter (construct StringSource))`

   Any input or output connections to labels that exist on both
  source-node and new-node will continue to exist. Any connections to
  labels that don't exist on new-node will be disconnected in the same
  transaction."
  [node-id new-node]
  (assert node-id)
  (it/become node-id new-node))

(defn become!
  "Creates the transaction step to turn one kind of node into another and applies it in a transaction. All properties and their values
   will be carried over from source-node to new-node. The resulting node will still have
   the same node-id.  Returns the transaction-result, (tx-result).

  Example:

  `(become! counter (construct StringSource)`

   Any input or output connections to labels that exist on both
  source-node and new-node will continue to exist. Any connections to
  labels that don't exist on new-node will be disconnected in the same
  transaction."
  [source-node new-node]
  (transact (become source-node new-node)))

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
  (is/user-data! @*the-system* node-id key value))

(defn user-data-swap! [node-id key f & args]
  (apply is/user-data-swap! @*the-system* node-id key f args))

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
   (let [outputs   (in/output-labels node-type)
         externs   (in/externs node-type)]
     (list
      (set-property node-id :_output-jammers
                    (zipmap (remove externs outputs)
                            (repeat (clojure.core/constantly defective-value))))
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
;; Values
;; ---------------------------------------------------------------------------
(defn make-evaluation-context
  ([] (make-evaluation-context {}))
  ([options] (is/make-evaluation-context @*the-system* options)))

(defn- do-node-value [node-id label evaluation-context]
  (is/node-value @*the-system* node-id label evaluation-context))

(defn update-cache-from-evaluation-context!
  [evaluation-context]
  (is/update-cache-from-evaluation-context! @*the-system* evaluation-context)
  nil)

(defn node-value
  "Pull a value from a node's output, identified by `label`.
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

  The label must exist as a defined transform on the node, or an
  AssertionError will result.

  Example:

  `(node-value node-id :chained-output)`"
  ([node-id label]
   (let [evaluation-context (make-evaluation-context)
         result (do-node-value node-id label evaluation-context)]
     (update-cache-from-evaluation-context! evaluation-context)
     result))
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
;; Constructing property maps
;; ---------------------------------------------------------------------------
(defn adopt-properties
  [node-id ps]
  (update ps :properties #(util/map-vals (fn [prop] (assoc prop :node-id node-id)) %)))

(defn aggregate-properties
  [x & [y & ys]]
  (if ys
    (apply aggregate-properties (aggregate-properties x y) ys)
    (-> x
        (update :properties merge (:properties y))
        (update :display-order #(into (or % []) (:display-order y))))))

;; ---------------------------------------------------------------------------
;; Interrogating the Graph
;; ---------------------------------------------------------------------------
(defn arcs->tuples
  [arcs]
  (util/project arcs [:source :sourceLabel :target :targetLabel]))

(defn inputs
  "Return the inputs to this node. Returns a collection like
  [[source-id output target-id input] [source-id output target-id input]...].

  If there are no inputs connected, returns an empty collection."
  ([node-id]       (inputs (now) node-id))
  ([basis node-id] (arcs->tuples (gt/arcs-by-tail basis node-id))))

(defn outputs
  "Return the outputs from this node. Returns a collection like
  [[source-id output target-id input] [source-id output target-id input]...].

  If there are no outputs connected, returns an empty collection."
  ([node-id]       (outputs (now) node-id))
  ([basis node-id] (arcs->tuples (gt/arcs-by-head basis node-id))))

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
    (is/invalidate-outputs! @*the-system* outputs)))

(defn node-instance*?
  "Returns true if the node is a member of a given type, including
   supertypes."
  ([type node]
    (node-instance*? (now) type node))
  ([basis type node]
    (if-let [nt (and type (node-type basis node))]
      (isa? (:key @nt) (:key @type))
      false)))

(defn node-instance?
  "Returns true if the node is a member of a given type, including
   supertypes."
  ([type node-id]
    (node-instance? (now) type node-id))
  ([basis type node-id]
    (node-instance*? basis type (gt/node-by-id-at basis node-id))))

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

(defn- serialize-arc [id-dictionary arc]
  (let [[src-id src-label]  (gt/head arc)
        [tgt-id tgt-label]  (gt/tail arc)]
    [(id-dictionary src-id) src-label (id-dictionary tgt-id) tgt-label]))

(defn- in-same-graph? [_ arc]
  (apply = (map node-id->graph-id (take-nth 2 arc))))


(defn- every-arc-pred [& preds]
  (fn [basis arc]
    (reduce (fn [v pred] (and v (pred basis arc))) true preds)))

(defn- predecessors [pred basis node-id]
  (into []
        (comp (filter #(pred basis %))
              (map first))
        (inputs basis node-id)))

(defn- input-traverse
  [basis pred root-ids]
  (ig/pre-traverse basis root-ids (partial predecessors (every-arc-pred in-same-graph? pred))))

(defn default-node-serializer
  [basis node]
  (let [node-id                (gt/node-id node)
        all-node-properties    (into {} (map (fn [[key value]] [key (:value value)])
                                             (:properties (node-value node-id :_declared-properties))))
        properties-without-fns (util/filterm (comp not fn? val) all-node-properties)]
    {:node-type  (node-type basis node)
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

(defn- deep-arcs-by-head
  "Like arcs-by-head, but also includes connections from nodes earlier in the
  override chain. Note that arcs-by-tail already does this."
  ([source-node-id]
   (deep-arcs-by-head (now) source-node-id))
  ([basis source-node-id]
   (into []
         (mapcat (partial gt/arcs-by-head basis))
         (override-originals basis source-node-id))))

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

  `:traverse?` will be called with the basis and arc data.

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

  `(g/copy root-ids {:traverse? (comp not resource? #(nth % 3))
                     :serializer (some-fn custom-serializer default-node-serializer %)})"
  ([root-ids opts]
   (copy (now) root-ids opts))
  ([basis root-ids {:keys [traverse? serializer] :or {traverse? (clojure.core/constantly false) serializer default-node-serializer} :as opts}]
    (s/validate opts-schema opts)
   (let [arcs-by-head   (partial deep-arcs-by-head basis)
         arcs-by-tail   (partial gt/arcs-by-tail basis)
         serializer     #(assoc (serializer basis (gt/node-by-id-at basis %2)) :serial-id %1)
         original-ids   (input-traverse basis traverse? root-ids)
         replacements   (zipmap original-ids (map-indexed serializer original-ids))
         serial-ids     (into {}
                              (mapcat (fn [[original-id {serial-id :serial-id}]]
                                        (map #(vector % serial-id)
                                             (override-originals basis original-id))))
                              replacements)
         include-arc?   (partial ig/arc-endpoints-p (partial contains? serial-ids))
         serialize-arc  (partial serialize-arc serial-ids)
         incoming-arcs  (mapcat arcs-by-tail original-ids)
         outgoing-arcs  (mapcat arcs-by-head original-ids)
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
(defn- traverse-cascade-delete [basis [src-id src-label tgt-id tgt-label]]
  (get (cascade-deletes (node-type* basis tgt-id)) tgt-label))

(defn- make-override-node
  [graph-id override-id original-node-id properties]
  (in/make-override-node override-id (is/next-node-id @*the-system* graph-id) original-node-id properties))

(defn override
  ([root-id]
   (override root-id {}))
  ([root-id opts]
   (override (now) root-id opts))
  ([basis root-id {:keys [traverse? properties-by-node-id] :or {traverse? (clojure.core/constantly true) properties-by-node-id (clojure.core/constantly {})}}]
   (let [graph-id (node-id->graph-id root-id)
         preds [traverse-cascade-delete traverse?]
         traverse-fn (partial predecessors (every-arc-pred in-same-graph? traverse-cascade-delete traverse?))
         node-ids (ig/pre-traverse basis [root-id] traverse-fn)
         override-id (is/next-override-id @*the-system* graph-id)
         overrides (mapv (partial make-override-node graph-id override-id) node-ids (map properties-by-node-id node-ids))
         override-node-ids (map gt/node-id overrides)
         original-node-id->override-node-id (zipmap node-ids override-node-ids)
         new-override-nodes-tx-data (map it/new-node overrides)
         new-override-tx-data (concat
                                (it/new-override override-id root-id traverse-fn)
                                (map (fn [node-id new-id] (it/override-node node-id new-id)) node-ids override-node-ids))]
     {:id-mapping original-node-id->override-node-id
      :tx-data (concat new-override-nodes-tx-data new-override-tx-data)})))

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

(defn property-overridden?
  ([node-id property]
   (property-overridden? (now) node-id property))
  ([basis node-id property]
   (if-let [node (node-by-id basis node-id)]
     (and (has-property? (node-type node) property) (gt/property-overridden? node property))
     false)))

(defn property-value-origin?
  ([node-id prop-kw]
   (property-value-origin? (now) node-id prop-kw))
  ([basis node-id prop-kw]
   (if (override? basis node-id)
     (property-overridden? basis node-id prop-kw)
     true)))

;; ---------------------------------------------------------------------------
;; Boot, initialization, and facade
;; ---------------------------------------------------------------------------
(defn initialize!
  "Set up the initial system including graphs, caches, and dispoal queues"
  [config]
  (reset! *the-system* (is/make-system config)))

(defn make-graph!
  "Create a new graph in the system with optional values of `:history` and `:volatility`. If no
  options are provided, the history ability is false and the volatility is 0

  Example:

  `(make-graph! :history true :volatility 1)`"
  [& {:keys [history volatility] :or {history false volatility 0}}]
  (let [g (assoc (ig/empty-graph) :_volatility volatility)
        s (swap! *the-system* (if history is/attach-graph-with-history! is/attach-graph!) g)]
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
    (swap! *the-system* is/detach-graph graph-id)))

(defn undo!
  "Given a `graph-id` resets the graph back to the last _step_ in time.

  Example:

  (undo gid)"
  [graph-id]
  (let [snapshot @*the-system*]
    (is/undo-history! snapshot graph-id)))

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
  (let [snapshot @*the-system*]
    (is/redo-history! snapshot graph-id)))

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
  (is/clear-history! @*the-system* graph-id))

(defn cancel!
  "Given a `graph-id` and a `sequence-id` _cancels_ any sequence of undos on the graph as
  if they had never happened in the history.

  Example:
  `(cancel! gid :a)`"
  [graph-id sequence-id]
  (let [snapshot @*the-system*]
    (is/cancel! snapshot graph-id sequence-id)))

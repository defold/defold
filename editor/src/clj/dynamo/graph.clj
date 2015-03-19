(ns dynamo.graph
  (:require [clojure.tools.macro :as ctm]
            [dynamo.node :as dn]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [internal.graph :as ig]
            [internal.node :as in]
            [potemkin.namespaces :refer [import-vars]]
            [internal.system :as is]))

(import-vars [plumbing.core <- ?> ?>> aconcat as->> assoc-when conj-when cons-when count-when defnk dissoc-in distinct-by distinct-fast distinct-id fn-> fn->> fnk for-map frequencies-fast get-and-set! grouped-map if-letk indexed interleave-all keywordize-map lazy-get letk map-from-keys map-from-vals map-keys map-vals mapply memoized-fn millis positions rsort-by safe-get safe-get-in singleton sum swap-pair! unchunk update-in-when when-letk])

(import-vars [internal.graph.types Node node-type transforms transform-types properties inputs injectable-inputs input-types outputs cached-outputs output-dependencies NodeType supertypes interfaces protocols method-impls triggers transforms' transform-types' properties' inputs' injectable-inputs' outputs' cached-outputs' event-handlers' output-dependencies' MessageTarget process-one-event node-ref])

;; ---------------------------------------------------------------------------
;; Definition
;; ---------------------------------------------------------------------------
(defmacro defnode
  "Given a name and a specification of behaviors, creates a node,
   and attendant functions.

  Allowed clauses are:

  (inherits _symbol_)

  Compose the behavior from the named node type

  (input _symbol_ _schema_)

  Define an input with the name, whose values must match the schema.

  (property _symbol_ _property-type_ & _options_)

  Define a property with schema and, possibly, default value and
  constraints.  Property type and options have the same syntax as for
  `dynamo.property/defproperty`.

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
      (property texture-filename t/Str (default \"\"))
      (output   texturec t/Any compile-texturec)))

    (defnode TextureSetCompiler
      (input    textureset TextureSet)
      (property textureset-filename t/Str (default \"\"))
      (output   texturesetc t/Any compile-texturesetc)))

    (defnode AtlasCompiler
      (inherit TextureCompiler)
      (inherit TextureSetCompiler))

  This will produce a record `AtlasCompiler`. `defnode` merges the
  behaviors appropriately.

  Every node can receive messages. The node declares message handlers
  with a special syntax:

  (trigger _symbol_ _type_ _action_)

  A trigger is invoked during transaction execution, when a node of
  the type is touched by the transaction. _symbol_ is a label for the
  trigger. Triggers are inherited, colliding labels are overwritten by
  the descendant.

  _type_ is a keyword, one of:

    :added             - The node was added in this transaction.
    :input-connections - One or more inputs to the node were connected to or disconnected from
    :property-touched  - One or more properties on the node were changed.
    :deleted           - The node was deleted in this transaction.

  For :added and :deleted triggers, _action_ is a function of five
  arguments:

    1. The current transaction context.
    2. The new graph as it has been modified during the transaction
    3. The node itself
    4. The label, as a keyword
    5. The trigger type

  The :input-connections and :property-touched triggers each have an
  additional argument, which is a collection of
  labels. For :input-connections, those are the inputs that were
  affected. For :property-touched, those are the properties that were
  modified.

  The trigger returns a collection of additional transaction
  steps. These effects will be applied within the current transaction.

  It is allowed for a trigger to cause changes that activate more
  triggers, up to a limit.  Triggers should not be used for timed
  actions or automatic counters. So they will only cascade until the
  limit `internal.transaction/maximum-retrigger-count` is reached.

  (on _message_type_ _form_)

  The form will be evaluated inside a transactional body. This means
  that it can use the special clauses to create nodes, change
  connections, update properties, and so on.

  A node definition allows any number of 'on' clauses.

  A node may also implement protocols or interfaces, using a syntax
  identical to `deftype` or `defrecord`. A node may implement any
  number of such protocols.

  Every node always implements dynamo.types/Node.

  If there are any event handlers defined for the node type, then it
  will also implement MessageTarget."
  [symb & body]
  (let [[symb forms] (ctm/name-with-attributes symb body)
        record-name  (in/classname-for symb)
        ctor-name    (symbol (str 'map-> record-name))]
    `(do
       (declare ~ctor-name ~symb)
       (let [description#    ~(in/node-type-sexps symb (concat dn/node-intrinsics forms))
             replacing#      (if-let [x# (and (resolve '~symb) (var-get (resolve '~symb)))]
                               (when (satisfies? NodeType x#) x#))
             whole-graph#    (is/world-graph @ds/the-system)
             to-be-replaced# (when (and whole-graph# replacing#)
                               (filterv #(= replacing# (node-type %)) (ig/node-values whole-graph#)))
             ctor#           (fn [args#] (~ctor-name (merge (in/defaults ~symb) args#)))]
         (def ~symb (in/make-node-type (assoc description# :dynamo.node/ctor ctor#)))
         (in/define-node-record  '~record-name '~symb ~symb)
         (in/define-print-method '~record-name '~symb ~symb)
         (when (< 0 (count to-be-replaced#))
           (ds/transactional* (fn []
                                (doseq [r# to-be-replaced#]
                                  (ds/become r# (dn/construct ~symb))))))
         (var ~symb)))))

;; ---------------------------------------------------------------------------
;; Transactions
;; ---------------------------------------------------------------------------
(defmacro transactional
  "Executes the body within a project transaction. All actions
  described in the body will happen atomically at the end of the
  transactional block.

  Transactional blocks nest nicely. The transaction will happen when
  the outermost block ends."
  [& forms]
  `(ds/transactional* (fn [] ~@forms)))

(defn operation-label
  "Set a human-readable label to describe the current transaction."
  [label]
  (ds/tx-label label))

(defn add
  "Add a newly created node to the world. This will attach the node to
  the current scope."
  [n]
  (ds/add n))

(defn delete
  "Remove a node from the world."
  [n]
  (ds/delete n))

(defn connect
  "Make a connection from an output of the source node to an input on the target node.
   Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (ds/connect source-node source-label target-node target-label))

(defn disconnect
  "Remove a connection from an output of the source node to the input on the target node.
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.  Takes effect when a transaction
  is applied."
  [source-node source-label target-node target-label]
  (ds/disconnect source-node source-label target-node target-label))

(defn become
  "Turn one kind of node into another, in a transaction. All properties and their values
   will be carried over from source-node to new-node. The resulting node will still have
   the same node-id.

   Any input or output connections to labels that exist on both
  source-node and new-node will continue to exist. Any connections to
  labels that don't exist on new-node will be disconnected in the same
  transaction."
  [source-node new-node]
  (ds/become source-node new-node))

(defn set-property
  "Assign a value to a node's property (or properties) value(s) in a
  transaction."
  [n & kvs]
  (apply ds/set-property n kvs))

(defn update-property
  "Apply a function to a node's property in a transaction. The
  function f will be invoked as if by (apply f current-value args)"
  [n p f & args]
  (apply ds/update-property n p f args))


;; ---------------------------------------------------------------------------
;; Values
;; ---------------------------------------------------------------------------
(defn node-value
  "Pull a value from a node's output, identified by `label`.
  The value may be cached or it may be computed on demand. This is
  transparent to the caller.

  This uses the \"current\" value of the node and its output.
  That means the caller will receive a value consistent with the most
  recently committed transaction.

  The label must exist as a defined transform on the node, or an
  AssertionError will result."
  ([node label]
   (ds/node-value node label))
  ([graph cache node label]
   (ds/node-value graph cache node label)))

(defn node-by-id
  [node-id]
  (ds/node-by-id node-id))

(defn make-graph
  [nodes]
  (loop [g  (ig/empty-graph)
         i  1
         ns nodes]
    (if-let [n (first ns)]
      (recur
       (ig/add-labeled-node g (inputs n) (outputs n) (assoc n :_id i))
       (inc i)
       (rest ns))
      g)))

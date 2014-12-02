(ns dynamo.node
  "Define new node types"
  (:require [clojure.set :as set]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [dynamo.system :as ds]
            [dynamo.types :refer :all]
            [internal.node :as in]
            [internal.graph.lgraph :as lg]))

(set! *warn-on-reflection* true)

(defn get-node-value [node label]
  (in/get-node-value node label))

(defnk selfie [this] this)

(def node-intrinsics
  [(list 'output 'self `s/Any `selfie)])

(defmacro defnode
  "Given a name and a specification of behaviors, creates a node,
and attendant functions.

Allowed clauses are:

(inherits  _symbol_)
Compose the behavior from the named node type

(input    _symbol_ _schema_)
Define an input with the name, whose values must match the schema.

(property _symbol_ _property-type_ & _options_)
Define a property with schema and, possibly, default value and constraints.
Property type and options have the same syntax as for `dynamo.property/defproperty`.

(output   _symbol_ _type_ (:cached)? (:on-update)? _producer_)
Define an output to produce values of type. Flags ':cached' and
':on-update' are optional. _producer_ may be a var that names an fn, or fnk.
It may also be a function tail as [arglist] + forms.

Values produced on an output with the :cached flag will be cached in memory until
the node is affected by some change in inputs or properties. At that time, the
cached value will be sent for disposal.

Ordinarily, an output value is not produced until it is requested. However, an
output with the :on-update flag will be updated as soon as possible after the
previous value is invalidated.

Example (from [[atlas.core]]):

    (defnode TextureCompiler
      (input    textureset TextureSet)
      (property texture-filename s/Str (default \"\"))
      (output   texturec s/Any compile-texturec)))

    (defnode TextureSetCompiler
      (input    textureset TextureSet)
      (property textureset-filename s/Str (default \"\"))
      (output   texturesetc s/Any compile-texturesetc)))

    (defnode AtlasCompiler
      (inherit TextureCompiler)
      (inherit TextureSetCompiler))

This will produce a record `AtlasCompiler`. `defnode` merges the behaviors appropriately.

Every node can receive messages. The node declares message handlers with a special syntax:

(on _message_type_ _form_)

The form will be evaluated inside a transactional body. This means that it can
use the special clauses to create nodes, change connections, update properties, and so on.

A node definition allows any number of 'on' clauses.

A node may also implement protocols or interfaces, using a syntax identical
to `deftype` or `defrecord`. A node may implement any number of such protocols.

Every node always implements dynamo.types/Node.

If there are any event handlers defined for the node type, then it will also
implement dynamo.types/MessageTarget."
  [name & specs]
  (let [descriptor (in/compile-specification name (concat node-intrinsics specs))]
    `(do
       ~(in/generate-descriptor   name descriptor)
       ~(in/generate-defrecord    name descriptor)
       ~(in/generate-constructor  name descriptor)
       ~(in/generate-print-method name)
       (in/validate-descriptor ~(str name) ~name))))

(defn abort
  "Abort production function and use substitute value."
  [msg & args]
  (throw (apply ex-info msg args)))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'."
  [node type & {:as body}]
  (process-one-event node (assoc body :type type)))

(defn get-node-inputs
  "Sometimes a production function needs direct access to an input source.
   This function takes any number of input labels and returns a vector of their
   values, in the same order."
  [node & labels]
  (map (in/collect-inputs node (-> node :world-ref deref :graph) (zipmap labels (repeat :ok))) labels))

; ---------------------------------------------------------------------------
; Bootstrapping the core node types
; ---------------------------------------------------------------------------
(defn inject-new-nodes
  [graph self transaction]
  (let [existing-nodes           (cons self (in/get-inputs self graph :nodes))
        out-from-new-connections (in/injection-candidates existing-nodes (:nodes-added transaction))
        in-to-new-connections    (in/injection-candidates (:nodes-added transaction) existing-nodes)
        all-possible             (set/union out-from-new-connections in-to-new-connections)
        not-already-connected    (remove (fn [[out out-label in in-label]]
                                           (lg/connected? graph (:_id out) out-label (:_id in) in-label))
                                         all-possible)]
    (doseq [connection not-already-connected]
      (apply ds/connect connection))))

(defn dispose-nodes
  [graph self transaction]
  (when (some #{(:_id self)} (map :_id (:nodes-removed transaction)))
    (let [graph-before-deletion (-> transaction :world-ref deref :graph)
          nodes-to-delete       (:nodes (in/collect-inputs self graph-before-deletion {:nodes :ok}))]
      (doseq [n nodes-to-delete]
        (ds/delete n)))))

(defnode Scope
  (input nodes [s/Any])

  (property tag      s/Keyword)
  (property parent   NamingContext)
  (property triggers Triggers (default [#'inject-new-nodes #'dispose-nodes]))

  (output dictionary s/Any in/scope-dictionary)

  NamingContext
  (lookup [this nm] (-> (get-node-value this :dictionary) (get nm))))

(defnode RootScope
  (inherits Scope)
  (property tag s/Keyword (default :root)))

(defmethod print-method RootScope__
  [^RootScope__ v ^java.io.Writer w]
  (.write w (str "<RootScope{:_id " (:_id v) "}>")))

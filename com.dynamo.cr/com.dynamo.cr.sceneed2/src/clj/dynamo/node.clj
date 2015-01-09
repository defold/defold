(ns dynamo.node
  "Define new node types"
  (:require [clojure.set :as set]
            [clojure.tools.macro :as ctm]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.system :as ds]
            [dynamo.types :refer :all]
            [dynamo.util :refer :all]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.node :as in]
            [internal.outline :as outline]))

(set! *warn-on-reflection* true)

(defn get-node-value [node label]
  (in/get-node-value node label))

(defnk selfie [this] this)

(defn- gather-property [this prop]
  {:node-id (:_id this)
   :value (get this prop)
   :type  (-> this :descriptor :properties prop)})

(defnk gather-properties :- Properties
  [this]
  (let [property-names (-> this :descriptor :properties keys)]
    (zipmap property-names (map (partial gather-property this) property-names))))

(def node-intrinsics
  [(list 'output 'self `s/Any `selfie)
   (list 'output 'properties `Properties `gather-properties)])

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

(defprotocol Builder
  (construct [this args])
  (defaults [this]))

(defrecord NodeTypeImpl [supers constructor inputs properties transforms transform-types event-handlers methods record-methods impls interfaces]
  Builder
  (defaults [this]       (map-vals default-property-value properties))
  (construct [this args] (apply constructor :_id (in/tempid) :descriptor this (mapcat identity (merge (defaults this) args)))))

(defn attribute-merge [m [k v]]
  (merge-with in/deep-merge m {k v}))

(defmacro defnode3
  [symb & args]
  (let [[symb body-forms] (ctm/name-with-attributes symb args)
        base-attrs        (mapcat (partial in/compile-defnode-form symb) node-intrinsics)
        override-attrs    (mapcat (partial in/compile-defnode-form symb) body-forms)
        ]
    (println :override-attrs override-attrs)
    `(do
       (declare ~symb)
       (let [record-ctor#   (ns-resolve *ns* (in/record-constructor-name '~symb))

             supers#            (mapcat #(when (= :supers (first %)) (second %)) ~override-attrs)
             _# (println :supers supers#)
             super-attrs#       (map (comp var-get resolve) supers#)
             _# (println :super-attrs super-attrs#)
             ctor#              {:constructor (fn [& {:as property-values#}]
                                                (assert record-ctor# (str "Internal error, no record for " '~symb " node. Try redefining the node."))
                                                (record-ctor# property-values#))}
             _# (println :ctor ctor#)
             attrs#             (reduce attribute-merge ctor# (list ~@base-attrs super-attrs# ~@override-attrs))
             _# (println :attrs attrs#)
             ]
        #_(in/emit-defrecord (in/classname-for ~symb) attrs#)
        (def ~symb (map->NodeTypeImpl attrs#))
        #_(defn ~(in/constructor-name symb) [& {:as args#}] (construct ~`~symb args#))
        #_(var ~symb)))))

;; TODO - evaluate property and output types in the generated code instead of compile-defnode-form
;; TODO - create function wrappers for event handlers in the generated code
;; TODO - reimplement protocol and interface methods

(defnode3 BasicNode)

(make-basic-node)

(defnode3 BasicNode2
  (inherits BasicNode)
  (property foo s/Str))

(defnode3 GChildNode (inherits BasicNode2))

(construct BasicNode2 {})

(make-basic-node-2)

(ns-unmap 'dynamo.node 'BasicNode)
(ns-unmap 'dynamo.node 'make-basic-node)
(ns-unmap 'dynamo.node 'make-basic-node-2)
(ns-unmap 'dynamo.node 'BasicNode2)

(satisfies? Builder BasicNode)

(macroexpand '(defnode3 BasicNode))
(macroexpand '(defnode3 BasicNode2 (inherits BasicNode)))

(defn abort
  "Abort production function and use substitute value."
  [msg & args]
  (throw (apply ex-info msg args)))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'."
  [node type & {:as body}]
  (process-one-event (ds/refresh node) (assoc body :type type)))

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
        nodes-added              (map #(dg/node graph %) (:nodes-added transaction))
        out-from-new-connections (in/injection-candidates existing-nodes nodes-added)
        in-to-new-connections    (in/injection-candidates nodes-added existing-nodes)
        candidates               (set/union out-from-new-connections in-to-new-connections)
        candidates               (remove (fn [[out out-label in in-label]]
                                           (or
                                             (= (:_id out) (:_id in))
                                             (lg/connected? graph (:_id out) out-label (:_id in) in-label)))
                                   candidates)]
    (doseq [connection candidates]
      (apply ds/connect connection))))

(defn dispose-nodes
  [graph self transaction]
  (when (ds/is-removed? transaction self)
    (let [graph-before-deletion (-> transaction :world-ref deref :graph)
          nodes-to-delete       (:nodes (in/collect-inputs self graph-before-deletion {:nodes :ok}))]
      (doseq [n nodes-to-delete]
        (ds/delete n)))))

(defproperty Triggers Callbacks (visible false))

(defnode Scope
  (input nodes [s/Any])

  (property tag      s/Keyword)
  (property parent   (s/protocol NamingContext))
  (property triggers Triggers (visible false) (default [#'inject-new-nodes #'dispose-nodes]))

  (output dictionary s/Any in/scope-dictionary)

  NamingContext
  (lookup [this nm] (-> (get-node-value this :dictionary) (get nm))))

(defn find-node
  ([attr val]
    (find-node (ds/current-scope) attr val))
  ([scope attr val]
    (when scope
      (filter #(= val (get % attr)) (flatten (get-node-inputs scope :nodes))))))

(defnode RootScope
  (inherits Scope)
  (property tag s/Keyword (default :root)))

(defmethod print-method RootScope__
  [^RootScope__ v ^java.io.Writer w]
  (.write w (str "<RootScope{:_id " (:_id v) "}>")))

(defn mark-dirty
  [graph self transaction]
  (when (and (ds/is-modified? transaction self) (not (ds/is-added? transaction self)))
    (ds/set-property self :dirty true)))

(defnode DirtyTracking
  (property triggers Triggers (default [#'mark-dirty]))
  (property dirty s/Bool
    (default false)
    (visible false)))

(defnode Saveable
  (output save s/Keyword :abstract))

(defnode ResourceNode
  (property filename (s/protocol PathManipulation) (visible false)))



(defnode OutlineNode
  (input  children [OutlineItem])
  (output tree     [OutlineItem] outline/outline-tree-producer))

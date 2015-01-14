(ns dynamo.node
  "Define new node types"
  (:require [clojure.set :as set]
            [clojure.core.match :refer [match]]
            [clojure.tools.macro :refer [name-with-attributes]]
            [plumbing.core :refer [defnk fnk]]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [dynamo.util :refer :all]
            [internal.node :as in]
            [internal.property :as ip]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]))

(set! *warn-on-reflection* true)

(defn get-node-value [node label]
  (in/get-node-value node label))

(defnk selfie [this] this)

(defn- gather-property [this prop]
  {:node-id (:_id this)
   :value (get this prop)
   :type  (-> this t/properties prop)})

(defnk gather-properties :- Properties
  [this]
  (let [property-names (-> this t/properties keys)]
    (zipmap property-names (map (partial gather-property this) property-names))))

(def node-intrinsics
  [(list 'output 'self `s/Any `selfie)
   (list 'output 'properties `Properties `gather-properties)])

; ------------------------------------------------------
; New defnode starts here
; ------------------------------------------------------

(defrecord NodeTypeImpl
  [supertypes interfaces protocols functions properties inputs injectable-inputs transforms transform-types cached event-handlers auto-update output-dependencies]

  t/NodeType
  (supertypes           [_] supertypes)
  (interfaces           [_] interfaces)
  (protocols            [_] protocols)
  (functions            [_] functions)
  (transforms'          [_] transforms)
  (transform-types'     [_] transform-types)
  (properties'          [_] properties)
  (inputs'              [_] inputs)
  (outputs'             [_] (set (keys transforms)))
  (events'              [_] (set (keys event-handlers)))
  (output-dependencies' [_] output-dependencies))

(defn construct [node-type & {:as args}]
  ((:ctor node-type) (merge {:_id (in/tempid)} args)))

(defn- from-supertypes [local op]                (map op (:supertypes local)))
(defn- combine-with    [local op zero into-coll] (op (reduce op zero into-coll) local))

(defn- pfnk? [f] (contains? (meta f) :schema))

(defn- invert-map
  [m]
  (apply merge-with into
         (for [[k vs] m
               v vs]
           {v #{k}})))

(defn inputs-for
  [transform]
  (let [production-fn (-> transform :production-fn)]
    (if (pfnk? production-fn)
      (into #{} (keys (dissoc (pf/input-schema production-fn) s/Keyword :this :g)))
      #{})))

(defn description->output-dependencies
   [{:keys [transforms properties] :as description}]
   (assoc description :output-dependencies
     (let [outs (dissoc transforms :self)
           outs (zipmap (keys outs) (map inputs-for (vals outs)))
           outs (assoc outs :properties (set (keys properties)))]
       (invert-map outs))))

(defn make-node-type
  [description]
  (-> description
    (update-in [:inputs]              combine-with merge      {} (from-supertypes description t/inputs'))
    (update-in [:injectable-inputs]   combine-with set/union #{} (from-supertypes description :injectable-inputs))
    (update-in [:properties]          combine-with merge      {} (from-supertypes description t/properties'))
    (update-in [:transforms]          combine-with merge      {} (from-supertypes description :transforms))
    (update-in [:transform-types]     combine-with merge      {} (from-supertypes description :transform-types))
    (update-in [:cached]              combine-with set/union #{} (from-supertypes description :cached))
    (update-in [:auto-update]         combine-with set/union #{} (from-supertypes description :auto-update))
    (update-in [:event-handlers]      combine-with set/union #{} (from-supertypes description :event-handlers))
    (update-in [:interfaces]          combine-with set/union #{} (from-supertypes description t/interfaces))
    (update-in [:protocols]           combine-with set/union #{} (from-supertypes description t/protocols))
    (update-in [:functions]           combine-with merge      {} (from-supertypes description t/functions))
    description->output-dependencies
    map->NodeTypeImpl))

(defn attach-supertype
  [description supertype]
  (assoc description :supertypes (conj (:supertypes description []) supertype)))

(defn attach-input
  [description label schema flags]
  (cond->
    (assoc-in description [:inputs label] schema)

    (some #{:inject} flags)
    (update-in [:injectable-inputs] #(conj (or % #{}) label))))

(defn- abstract-function
  [label type]
  (fn [this g]
    (throw (AssertionError.
             (format "Node %d does not supply a production function for the abstract '%s' output. Add (output %s %s your-function) to the definition of %s"
               (:_id this) label
               label type this)))))

(defn attach-output
  [description label schema properties options & [args]]
  (cond-> (update-in description [:transform-types] assoc label schema)

    (:substitute-value options)
    (update-in [:transforms] assoc-in [label :substitute-value-fn] (:substitute-value options))

    (:abstract properties)
    (update-in [:transforms] assoc-in [label :production-fn] (abstract-function label schema))

    (:cached properties)
    (update-in [:cached] #(conj (or % #{}) label))

    (:on-update properties)
    (update-in [:auto-update] #(conj (or % #{}) label))

    (not (:abstract properties))
    (update-in [:transforms] assoc-in [label :production-fn] args)))

(defn attach-property
  [description label property-type passthrough]
  (-> description
    (update-in [:properties] assoc label property-type)
    (update-in [:transforms] assoc-in [label :production-fn] passthrough)
    (update-in [:transform-types] assoc label (:value-type property-type))))

(defn attach-event-handler
  [description label handler]
  (assoc-in description [:event-handlers label] handler))

(defn attach-interface
  [description interface]
  (update-in description [:interfaces] #(conj (or % #{}) interface)))

(defn attach-protocol
  [description protocol]
  (update-in description [:protocols] #(conj (or % #{}) protocol)))

(defn attach-function
  [description sym argv fn-def]
  (assoc-in description [:functions sym] [argv fn-def]))

(def ^:private property-flags #{:cached :on-update :abstract})
(def ^:private option-flags #{:substitute-value})

(defn parse-output-options [args]
  (loop [properties #{}
         options {}
         args args]
    (if-let [[arg & remainder] (seq args)]
      (cond
        (contains? property-flags arg) (recur (conj properties arg) options remainder)
        (contains? option-flags arg)   (do (assert remainder (str "Expected value for option " arg))
                                         (recur properties (assoc options arg (first remainder)) (rest remainder)))
        :else [properties options args])
      [properties options args])))


(defn fqsymbol
  [s]
  (if (symbol? s)
    (let [{:keys [ns name]} (meta (resolve s))]
      (symbol (str ns) (str name)))))

(defn node-type-form
  [form]
  (match [form]
    [(['inherits supertype] :seq)]
    `(attach-supertype ~supertype)

    [(['input label schema & flags] :seq)]
    `(attach-input ~(keyword label) ~schema #{~@flags})

    [(['output label schema & remainder] :seq)]
    (let [[properties options args] (parse-output-options remainder)]
      `(attach-output ~(keyword label) ~schema ~properties ~options ~@args))

    [(['property label tp & options] :seq)]
    `(attach-property ~(keyword label) ~(ip/property-type-descriptor label tp options) (fnk [~label] ~label))

    [(['on label & fn-body] :seq)]
    `(attach-event-handler ~(keyword label) (fn [~'self ~'event] (dynamo.system/transactional ~@fn-body)))

    ;; Interface or protocol function
    [([nm [& argvec] & remainder] :seq)]
    `(attach-function '~nm '~argvec (fn ~argvec ~@remainder))

    [impl :guard symbol?]
    `(cond->
        (class? ~impl)
        (attach-interface '~impl)

        (not (class? ~impl))
        (attach-protocol (fqsymbol '~impl)))))

(defn node-type [forms]
  (list* `-> {}
    (map node-type-form forms)))

(defn state-vector
  [node-type]
  (vec (map (comp symbol name) (keys (t/properties' node-type)))))


(defn message-processors
  [node-type-name node-type]
  (when (not-empty (t/events' node-type))
    `[t/MessageTarget
      (dynamo.types/process-one-event
       [~'self ~'event]
       (case (:type ~'event)
         ~@(mapcat (fn [e] [e `((get-in ~node-type-name [:event-handlers ~e]) ~'self ~'event)]) (t/events' node-type))
         nil))]))

(defn generate-node-record-sexps
  [record-name node-type-name node-type]
  `(defrecord ~record-name ~(state-vector node-type)
     t/Node
     (inputs              [_]    ~(set (keys (t/inputs' node-type))))
     (input-types         [_]    ~(t/inputs' node-type))
     (injectable-inputs   [_]    ~(:injectable-inputs node-type))
     (outputs             [_]    ~(t/outputs' node-type))
     (transforms          [_]    (:transforms ~node-type-name))
     (transform-types     [_]    ~(t/transform-types' node-type))
     (cached-outputs      [_]    ~(:cached node-type))
     (auto-update?        [_ l#] (contains? ~(:auto-update node-type) l#))
     (properties          [_]    ~(t/properties' node-type))
     (output-dependencies [_]    ~(t/output-dependencies' node-type))
     ~@(map #(symbol %) (t/interfaces node-type))
     ~@(map #(symbol %) (t/protocols node-type))
     ~@(map (fn [[fname [argv _]]] `(~fname ~argv ((second (get-in ~node-type-name [:functions '~fname])) ~@argv))) (t/functions node-type))
     ~@(message-processors node-type-name node-type)))

(defn define-node-record
  [record-name node-type-name node-type]
  ;(println (generate-node-record-sexps record-name node-type-name node-type))
  (eval (generate-node-record-sexps record-name node-type-name node-type)))

(defn interpose-every
  [n elt coll]
  (mapcat (fn [l r] (conj l r)) (partition-all n coll) (repeat elt)))

(defn- print-method-sexps
  [record-name node-type-name node-type]
  (let [tagged-arg (vary-meta (gensym "v") assoc :tag (resolve record-name))]
    `(defmethod print-method ~record-name
       [~tagged-arg w#]
       (.write
         ^java.io.Writer w#
         (str "#" '~node-type-name "{:_id " (:_id ~tagged-arg)
           ~@(interpose-every 3 ", " (mapcat (fn [prop] `[~prop " " (pr-str (get ~tagged-arg ~prop))]) (keys (t/properties' node-type))))
           "}>")))))

(defn define-print-method
  [record-name node-type-name node-type]
  ;(println (print-method-sexps record-name node-type-name node-type))
  (eval (print-method-sexps record-name node-type-name node-type)))

(defn- classname-for [prefix]  (symbol (str prefix "__")))

(defn defaults
  [node-type]
  (map-vals t/default-property-value (t/properties' node-type)))

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

Example (from [[editors.atlas]]):

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
  [symb & body]
  (let [[symb forms] (name-with-attributes symb body)
        record-name  (classname-for symb)
        ctor-name    (symbol (str 'map-> record-name))]
    `(do
       (declare ~ctor-name ~symb)
       (let [description# ~(node-type (concat node-intrinsics forms))
            ctor#        (fn [args#] (~ctor-name (merge (defaults ~symb) args#)))
            node-type#   (make-node-type (assoc description# :ctor ctor#))]
        (def ~symb node-type#)
        (define-node-record '~record-name '~symb ~symb)
        (define-print-method '~record-name '~symb ~symb)
        (var ~symb)))))


; ---------------------------------------------------------------------------
; Old defnode
; ---------------------------------------------------------------------------

#_(defmacro defnode-old
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
  (process-one-event (ds/refresh node) (assoc body :type type)))

; ---------------------------------------------------------------------------
; End of old defnode
; ---------------------------------------------------------------------------


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



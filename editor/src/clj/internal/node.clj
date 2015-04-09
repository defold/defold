(ns internal.node
  (:require [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.cache :as c]
            [internal.either :as e]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.property :as ip]
            [plumbing.core :refer [fnk]]
            [plumbing.fnk.pfnk :as pf])
  (:import [internal.graph.types IBasis]))

(defn resource-property?
  [property-type]
  (some-> property-type t/property-tags (->> (some #{:dynamo.property/resource}))))

(defn- pfnk? [f] (contains? (meta f) :schema))

;; ---------------------------------------------------------------------------
;; New evaluation
;; ---------------------------------------------------------------------------
(defn abort
  [why _ in-production node-id label & _]
  (throw (ex-info (str why " Trying to produce [" node-id label "]")
                  {:node-id node-id :label label :in-production in-production})))

(def not-found (partial abort "No such property, input or output."))
(def cycle-detected (partial abort "Production cycle detected."))

(defn- chain-eval
  "Start a chain of evaluators"
  [evaluators ^IBasis basis in-production node-id label]
  (let [chain-head    (first evaluators)
        chain-tail    (next evaluators)]
    (chain-head basis in-production node-id label evaluators chain-tail)))

(defn- continue
  "Continue evaluation with the next link in the chain."
  [^IBasis basis in-production node-id label chain-head chain-next]
  ((first chain-next) basis in-production node-id label chain-head (next chain-next)))

(def ^:private multivalued?  vector?)
(def ^:private exists?       (comp not nil?))
(def ^:private has-property? contains?)
(defn- has-output? [node label] (some-> node gt/transforms label boolean))
(def ^:private first-source (comp first gt/sources))
(defn- currently-producing [in-production node-id label] (= [node-id label] (last in-production)))

(defn- evaluate-input-internal
  "Gather an named argument for a production function. Depending on
  the name, this could mean one of five things:

  1. It names an input label. Evaluate the sources to get their values.
  2. It names a property of the node itself. Return the value of the property.
  3. It names another output of the same node. (The node has functions that depend on other
     outputs of the same node.) Evaluate that output first, then use its value.
  4. It is the special name :this. Use the entire node as an argument."
  [basis in-production node-id label chain-head]
  (let [node             (gt/node-by-id basis node-id)
        input-schema     (some-> node gt/input-types label)
        output-transform (some-> node gt/transforms  label)]
    (cond
      (and (has-output? node label) (not (currently-producing in-production node-id label)))
      (chain-eval chain-head basis in-production node-id label)

      (multivalued? input-schema)
      (e/bind
       (reduce
        (fn [input-vals source]
          (conj
           input-vals
           (e/result
            (chain-eval chain-head basis in-production (first source) (second source)))))
        []
        (gt/sources basis node-id label)))

      (exists? input-schema)
      (if-let [source (first-source basis node-id label)]
        (chain-eval chain-head basis in-production (first source) (second source))
        (e/bind nil))

      (has-property? node label)
      (e/bind (get node label))

      (= :this label)
      (e/bind node)

      (has-output? node label)
      (chain-eval chain-head basis in-production node-id label))))

(defn- collect-inputs
  "Return a map of all inputs needed for the input-schema."
  [^IBasis basis in-production node-id label chain-head input-schema]
  (reduce-kv
   (fn [inputs desired-input-name desired-input-schema]
     (assoc inputs desired-input-name
            (evaluate-input-internal basis in-production node-id desired-input-name chain-head)))
   {}
   (dissoc input-schema t/Keyword)))

(defn- produce-with-schema
  "Helper function: if the production function has schema information,
  use it to collect the required arguments."
  [^IBasis basis in-production node-id label chain-head production-fn]
  (e/bind
   (production-fn
    (map-vals #(when % (e/result %))
              (collect-inputs basis in-production node-id label chain-head
                              (pf/input-schema production-fn))))))

(defn apply-transform-or-substitute
  "Attempt to invoke the production function for an output. If it
  fails, call the transform's substitute value function."
  [^IBasis basis in-production node-id label chain-head transform]
  (let [producer (:production-fn transform)
        fallback (:substitute-value-fn transform)]
    (e/or-else
     (cond
       (pfnk? producer)
       (produce-with-schema basis in-production node-id label chain-head producer)

       (fn? producer)
       (e/bind (producer (gt/node-by-id basis node-id) basis))

       :else
       (e/bind producer))
     (fn [e]
       (cond
         (fn? fallback)
         (fallback {:exception e :node-id node-id :label label})

         fallback
         fallback

         :else
         (throw e))))))

(defn mark-in-production
  "This evaluation function checks whether the requested value is
  already being computed. If so, it means there is a cycle in the
  graph somewhere, so we must abort."
  [^IBasis basis in-production node-id label chain-head chain-next]
  (if (some #{[node-id label]} in-production)
    (cycle-detected basis in-production node-id label)
    (continue basis (conj in-production [node-id label]) node-id label chain-head chain-next)))

(defn evaluate-production-function
  "This evaluation function looks for a production function to create
  the output on the node. If there is a production function, it
  gathers the arguments needed for the production function and invokes
  it."
  [^IBasis basis in-production node-id label chain-head chain-next]
  (if-let [transform (some->> node-id (gt/node-by-id basis) gt/transforms label)]
    (apply-transform-or-substitute basis in-production node-id label chain-head transform)
    (continue basis in-production node-id label chain-head chain-next)))

(defn lookup-property
  "This evaluation function looks for a property on the node. If there
  is no such property, evaluation continues with the rest of the
  chain."
  [^IBasis basis in-production node-id label chain-head chain-next]
  (let [node (gt/node-by-id basis node-id)]
    (if (has-property? node label)
      (e/bind (get node label))
      (continue basis in-production node-id label chain-head chain-next))))

(defn read-input
  "This evaluation function looks for an input to the node. If the
  input exists and is multivalued, then all the incoming values are
  conjed together. If the input does not exist, evaluation continues
  with the rest of the chain."
  [^IBasis basis in-production node-id label chain-head chain-next]
  (let [node             (gt/node-by-id basis node-id)
        input-schema     (some-> node gt/input-types label)]
    (cond
      (multivalued? input-schema)
      (e/bind
       (reduce
        (fn [input-vals source]
          (conj
           input-vals
           (e/result
            (chain-eval chain-head basis in-production (first source) (second source)))))
        []
        (gt/sources basis node-id label)))

      (exists? input-schema)
      (if-let [source (first-source basis node-id label)]
        (chain-eval chain-head basis in-production (first source) (second source))
        (e/bind nil))

      :else
      (continue basis in-production node-id label chain-head chain-next))))

(def world-evaluation-chain
  [mark-in-production
   evaluate-production-function
   lookup-property
   read-input
   not-found])

(defn- delta [deltas node-id label v] (swap! deltas conj [[node-id label] v]) v)
(defn- local [deltas node-id label]   (first (keep (fn [[i l v]] (when (and (= node-id i) (= l label)) v)) @deltas)))

(defn local-deltas
  "Returns an evaluator. The evaluator is an evaluation function that
  looks for any values that have already been produced during this
  computation.

  If no local delta is found, the evaluator continues the
  chain. Whatever the chain returns gets recorded as a local delta for
  possible use during the remainder of the evaluation."
  [deltas ^IBasis basis in-production node-id label chain-head chain-next]
  (if-some [r (local deltas node-id label)]
    r
    (delta deltas node-id label
           (continue basis in-production node-id label chain-head chain-next))))

(defn- hit [hits node-id label v] (swap! hits conj [node-id label]) v)

(defn cache-lookup
  "Returns an evaluation function.

  The evaluator collects records of values that were 'hits' in the
  atom of the same name."
  [snapshot hits ^IBasis basis in-production node-id label chain-head chain-next]
  (if-some [v (get snapshot [node-id label])]
    (hit hits node-id label v)
    (continue basis in-production node-id label chain-head chain-next)))

(defn fork
  "Return a thunk for an evaluation chain. When invoked during
  evaluation, it will call `test` with 6 arguments: the basis, the
  in-production set, the node-id and label, the head of the evaluation
  chain and the rest of the current chain.

  The rest of the current chain should be 2 items long. Each one is a
  nested sequence of evaluators."
  [test]
  (fn [^IBasis basis in-production node-id label chain-head chain-next]
    (assert (= 2 (count chain-next)))
    (let [[consequent alternate & _] chain-next
          branch?                    (test basis node-id label)
          next-chain                 (if branch? consequent alternate)]
      ((first next-chain) basis in-production node-id label chain-head (next next-chain)))))

(defn- cacheable?
  "Check the node type to see if the given output should be cached
  once computed."
  [^IBasis basis node-id label]
  (if-let [node (gt/node-by-id basis node-id)]
    ((gt/cached-outputs node) label)
    false))

(def fork-cacheable (fork cacheable?))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [^IBasis basis cache node label]
  (let [node-id    (if (gt/node? node) (gt/node-id node) node)
        hits       (atom [])
        deltas     (atom [])
        evaluators (if cache
                     [fork-cacheable
                      (list* (partial local-deltas deltas)
                             (partial cache-lookup (c/cache-snapshot cache) hits)
                             world-evaluation-chain)
                      world-evaluation-chain]
                     world-evaluation-chain)
        result     (chain-eval evaluators basis [] node-id label)]
    (when cache
      (c/cache-hit cache @hits)
      (c/cache-encache cache @deltas))
    (e/result result)))

;; ---------------------------------------------------------------------------
;; Definition handling
;; ---------------------------------------------------------------------------
(defrecord NodeTypeImpl
  [name supertypes interfaces protocols method-impls triggers transforms transform-types properties inputs injectable-inputs cached-outputs event-handlers input-dependencies]

  gt/NodeType
  (supertypes           [_] supertypes)
  (interfaces           [_] interfaces)
  (protocols            [_] protocols)
  (method-impls         [_] method-impls)
  (triggers             [_] triggers)
  (transforms'          [_] transforms)
  (transform-types'     [_] transform-types)
  (properties'          [_] properties)
  (inputs'              [_] (map-vals #(if (satisfies? t/PropertyType %) (t/property-value-type %) %) inputs))
  (injectable-inputs'   [_] injectable-inputs)
  (outputs'             [_] (set (keys transforms)))
  (cached-outputs'      [_] cached-outputs)
  (event-handlers'      [_] event-handlers)
  (input-dependencies'  [_] input-dependencies))

(defmethod print-method NodeTypeImpl
  [^NodeTypeImpl v ^java.io.Writer w]
  (.write w (str "<NodeTypeImpl{:name " (:name v) ", :supertypes " (mapv :name (:supertypes v)) "}>")))

(defn- from-supertypes [local op]                (map op (:supertypes local)))
(defn- combine-with    [local op zero into-coll] (op (reduce op zero into-coll) local))

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
      (into #{} (keys (dissoc (pf/input-schema production-fn) t/Keyword :this :g)))
      #{})))

(defn dependency-seq
  ([desc inputs]
    (dependency-seq desc #{} inputs))
  ([desc seen inputs]
    (mapcat
      (fn [x]
        (if (not (seen x))
          (if-let [recursive (get-in desc [:transforms x])]
            (dependency-seq desc (conj seen x) (inputs-for recursive))
            #{x})
          seen))
      inputs)))

(defn description->input-dependencies
   [{:keys [transforms properties] :as description}]
   (let [outs (dissoc transforms :self)
         outs (zipmap (keys outs) (map #(dependency-seq description (inputs-for %)) (vals outs)))
         outs (assoc outs :properties (set (keys properties)))]
     (invert-map outs)))

(defn attach-input-dependencies
  [description]
  (assoc description :input-dependencies (description->input-dependencies description)))

(def ^:private map-merge (partial merge-with merge))

(defn make-node-type
  "Create a node type object from a maplike description of the node.
This is really meant to be used during macro expansion of `defnode`,
not called directly."
  [description]
  (-> description
    (update-in [:inputs]              combine-with merge      {} (from-supertypes description gt/inputs'))
    (update-in [:injectable-inputs]   combine-with set/union #{} (from-supertypes description gt/injectable-inputs'))
    (update-in [:properties]          combine-with merge      {} (from-supertypes description gt/properties'))
    (update-in [:transforms]          combine-with merge      {} (from-supertypes description gt/transforms'))
    (update-in [:transform-types]     combine-with merge      {} (from-supertypes description gt/transform-types'))
    (update-in [:cached-outputs]      combine-with set/union #{} (from-supertypes description gt/cached-outputs'))
    (update-in [:event-handlers]      combine-with set/union #{} (from-supertypes description gt/event-handlers'))
    (update-in [:interfaces]          combine-with set/union #{} (from-supertypes description gt/interfaces))
    (update-in [:protocols]           combine-with set/union #{} (from-supertypes description gt/protocols))
    (update-in [:method-impls]        combine-with merge      {} (from-supertypes description gt/method-impls))
    (update-in [:triggers]            combine-with map-merge  {} (from-supertypes description gt/triggers))
    attach-input-dependencies
    map->NodeTypeImpl))

(defn attach-supertype
  "Update the node type description with the given supertype."
  [description supertype]
  (assoc description :supertypes (conj (:supertypes description []) supertype)))

(defn attach-input
  "Update the node type description with the given input."
  [description label schema flags]
  (cond->
    (assoc-in description [:inputs label] schema)

    (some #{:inject} flags)
    (update-in [:injectable-inputs] #(conj (or % #{}) label))))

(defn- abstract-function
  [label type]
  (fn [this & _]
    (throw (AssertionError.
             (format "Node %d does not supply a production function for the abstract '%s' output. Add (output %s %s your-function) to the definition of %s"
               (gt/node-id this) label
               label type this)))))

(defn attach-output
  "Update the node type description with the given output."
  [description label schema properties options & [args]]
  (cond-> (update-in description [:transform-types] assoc label schema)

    (:substitute-value options)
    (update-in [:transforms] assoc-in [label :substitute-value-fn] (:substitute-value options))

    (:cached properties)
    (update-in [:cached-outputs] #(conj (or % #{}) label))

    (:abstract properties)
    (update-in [:transforms] assoc-in [label :production-fn] (abstract-function label schema))

    (not (:abstract properties))
    (update-in [:transforms] assoc-in [label :production-fn] args)))

(defn attach-property
  "Update the node type description with the given property."
  [description label property-type passthrough]
  (cond-> (update-in description [:properties] assoc label property-type)

    (resource-property? property-type)
    (assoc-in [:inputs label] property-type)

    true
    (update-in [:transforms] assoc-in [label :production-fn] passthrough)

    true
    (update-in [:transform-types] assoc label (:value-type property-type))))

(defn attach-event-handler
  "Update the node type description with the given event handler."
  [description label handler]
  (assoc-in description [:event-handlers label] handler))

(defn attach-trigger
  "Update the node type description with the given trigger."
  [description label kinds action]
  (reduce
    (fn [description kind] (assoc-in description [:triggers kind label] action))
    description
    kinds))

(defn attach-interface
  "Update the node type description with the given interface."
  [description interface]
  (update-in description [:interfaces] #(conj (or % #{}) interface)))

(defn attach-protocol
  "Update the node type description with the given protocol."
  [description protocol]
  (update-in description [:protocols] #(conj (or % #{}) protocol)))

(defn attach-method-implementation
  "Update the node type description with the given function, which
must be part of a protocol or interface attached to the description."
  [description sym argv fn-def]
  (assoc-in description [:method-impls sym] [argv fn-def]))

(def ^:private property-flags #{:cached :abstract})
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

(defn classname
  [^Class c]
  (.getName c))

(defn fqsymbol
  [s]
  (assert (symbol? s))
  (let [{:keys [ns name]} (meta (resolve s))]
    (symbol (str ns) (str name))))

(def ^:private valid-trigger-kinds #{:added :deleted :property-touched :input-connections})

(defn- node-type-form
  "Translate the sugared `defnode` forms into function calls that
build the node type description (map). These are emitted where you invoked
`defnode` so that symbols and vars resolve correctly."
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
    `(attach-event-handler ~(keyword label) (fn [~'self ~'event] ~@fn-body))

    [(['trigger label & rest] :seq)]
    (let [kinds (vec (take-while keyword? rest))
          action (drop-while keyword? rest)]
      (assert (every? valid-trigger-kinds kinds) (apply str "Invalid trigger kind. Valid trigger kinds are: " (interpose ", " valid-trigger-kinds)))
      `(attach-trigger ~(keyword label) ~kinds ~@action))

    ;; Interface or protocol function
    [([nm [& argvec] & remainder] :seq)]
    `(attach-method-implementation '~nm '~argvec (fn ~argvec ~@remainder))

    [impl :guard symbol?]
    `(cond->
        (class? ~impl)
        (attach-interface (symbol (classname ~impl)))

        (not (class? ~impl))
        (attach-protocol (fqsymbol '~impl)))))

(defn node-type-sexps
  "Given all the forms in a defnode macro, emit the forms that will build the node type description."
  [symb forms]
  (list* `-> {:name (str symb)}
    (map node-type-form forms)))

(defn defaults
  "Return a map of default values for the node type."
  [node-type]
  (map-vals t/property-default-value (gt/properties' node-type)))

(defn classname-for [prefix] (symbol (str prefix "__")))

(defn- state-vector
  [node-type]
  (conj (mapv (comp symbol name) (keys (gt/properties' node-type))) '_id ))

(defn- message-processor
  [node-type-name node-type]
  (when (not-empty (gt/event-handlers' node-type))
    `[gt/MessageTarget
      (gt/process-one-event
       [~'self ~'event]
       (case (:type ~'event)
         ~@(mapcat (fn [e] [e `((get (gt/event-handlers' ~node-type-name) ~e) ~'self ~'event)]) (keys (gt/event-handlers' node-type)))
         nil))]))

(defn- node-record-sexps
  [record-name node-type-name node-type]
  `(defrecord ~record-name ~(state-vector node-type)
     gt/Node
     (node-id             [this#] (:_id this#))
     (node-type           [_]    ~node-type-name)
     (inputs              [_]    (set (keys (gt/inputs' ~node-type-name))))
     (input-types         [_]    (gt/inputs' ~node-type-name))
     (injectable-inputs   [_]    (gt/injectable-inputs' ~node-type-name))
     (outputs             [_]    (gt/outputs' ~node-type-name))
     (transforms          [_]    (gt/transforms' ~node-type-name))
     (transform-types     [_]    (gt/transform-types' ~node-type-name))
     (cached-outputs      [_]    (gt/cached-outputs' ~node-type-name))
     (properties          [_]    (gt/properties' ~node-type-name))
     (input-dependencies  [_]    (gt/input-dependencies' ~node-type-name))
     ~@(gt/interfaces node-type)
     ~@(gt/protocols node-type)
     ~@(map (fn [[fname [argv _]]] `(~fname ~argv ((second (get (gt/method-impls ~node-type-name) '~fname)) ~@argv))) (gt/method-impls node-type))
     ~@(message-processor node-type-name node-type)))

(defn define-node-record
  "Create a new class for the node type. This builds a defrecord with
the node's properties as fields. The record will implement all the interfaces
and protocols that the node type requires."
  [record-name node-type-name node-type]
  (eval (node-record-sexps record-name node-type-name node-type)))

(defn- interpose-every
  [n elt coll]
  (mapcat (fn [l r] (conj l r)) (partition-all n coll) (repeat elt)))

(defn- print-method-sexps
  [record-name node-type-name node-type]
  (let [node (vary-meta 'node assoc :tag (resolve record-name))]
    `(defmethod print-method ~record-name
       [~node w#]
       (.write
        ^java.io.Writer w#
        (str "#" '~node-type-name "{" (:_id ~node)
             ~@(interpose-every 3 ", "
                                (mapcat (fn [prop] `[~prop " " (pr-str (get ~node ~prop))])
                                        (keys (gt/properties' node-type))))
             "}")))))

(defn define-print-method
  "Create a nice print method for a node type. This avoids infinitely recursive output in the REPL."
  [record-name node-type-name node-type]
  (eval (print-method-sexps record-name node-type-name node-type)))

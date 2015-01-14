(ns internal.node
  (:require [camel-snake-kebab :refer [->kebab-case]]
            [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.metrics :as metrics]
            [internal.property :as ip]
            [internal.query :as iq]
            [internal.either :as e]
            [service.log :as log]
            [inflections.core :refer [plural]]
            [camel-snake-kebab :refer [->kebab-case]]))

(set! *warn-on-reflection* true)

; ---------------------------------------------------------------------------
; Value handling
; ---------------------------------------------------------------------------
(defn- abstract-function
  [nodetype label type]
  (fn [this g]
    (throw (AssertionError.
             (format "Node %d (type %s) inherits %s, but does not supply a production function for the abstract '%s' output. Add (output %s %s your-function) to the definition of %s"
               (:_id this) (some-> this :descriptor :name) nodetype label
               label type (some-> this :descriptor :name))))))

(defn- find-enclosing-scope
  [tag node]
  (when-let [scope (iq/node-consuming node :self)]
    (if (= tag (:tag scope))
      scope
      (recur tag scope))))

(defn missing-input [n l]
  {:error     :input-not-declared
   :node      (:_id n)
   :node-type (class n)
   :label     l})

(declare get-node-value)

(defn get-inputs [target-node g target-label]
  (if (contains? target-node target-label)
    (get target-node target-label)
    (let [schema (some-> target-node t/input-types target-label)
          output-transform (some-> target-node t/transforms target-label)]
      (cond
        (vector? schema)     (mapv (fn [[source-node source-label]]
                                     (get-node-value (dg/node g source-node) source-label))
                                  (lg/sources g (:_id target-node) target-label))
        (not (nil? schema))  (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
                               (when first-source-node
                                 (get-node-value (dg/node g first-source-node) first-source-label)))
        (not (nil? output-transform)) (get-node-value target-node target-label)
        :else                (let [missing (missing-input target-node target-label)]
                               (service.log/warn :missing-input missing)
                               missing)))))

(defn collect-inputs [node g input-schema]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        :world     (assoc m k (:world-ref node))
        :project   (assoc m k (find-enclosing-scope :project node))
        s/Keyword  m
        (assoc m k (get-inputs node g k))))
    {} input-schema))

(defn- pfnk? [f] (contains? (meta f) :schema))

(defn- perform-with-inputs [production-fn node g]
  (if (pfnk? production-fn)
    (production-fn (collect-inputs node g (pf/input-schema production-fn)))
    (t/apply-if-fn production-fn node g)))

(defn- default-substitute-value-fn [v]
  (throw (:exception v)))

(defn perform* [transform node g]
  (let [production-fn       (-> transform :production-fn t/var-get-recursive)
        substitute-value-fn (get transform :substitute-value-fn default-substitute-value-fn)]
    (-> (e/bind (perform-with-inputs production-fn node g))
        (e/or-else (fn [e] (t/apply-if-fn substitute-value-fn {:exception e :node node}))))))

(def ^:dynamic *perform-depth* 200)

(defn perform [transform node g]
  {:pre [(pos? *perform-depth*)]}
  (binding [*perform-depth* (dec *perform-depth*)]
    (perform* transform node g)))

(defn- hit-cache [world-state cache-key value]
  (dosync (alter world-state update-in [:cache] cache/hit cache-key))
  value)

(defn- miss-cache [world-state cache-key value]
  (dosync (alter world-state update-in [:cache] cache/miss cache-key value))
  value)

(defn- produce-value [node g label]
  (assert (contains? (t/outputs node) label) (str "There is no transform " label " on node " (:_id node)))
  (let [transform (some-> node t/transforms label)]
    (assert (not= ::abstract transform) )
    (metrics/node-value node label)
    (perform transform node g)))

(defn- get-node-value-internal
  [node label]
  (let [world-state (:world-ref node)]
    (if-let [cache-key (get-in @world-state [:cache-keys (:_id node) label])]
      (let [cache (:cache @world-state)]
        (if (cache/has? cache cache-key)
            (hit-cache  world-state cache-key (get cache cache-key))
            (miss-cache world-state cache-key (produce-value node (:graph @world-state) label))))
      (produce-value node (:graph @world-state) label))))

(defn get-node-value
  [node label]
  (e/result (get-node-value-internal node label)))

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextid (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn tempid [] (- (.getAndIncrement nextid)))

; ---------------------------------------------------------------------------
; Definition handling
; ---------------------------------------------------------------------------
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

(defn node-type-sexps [forms]
  (list* `-> {}
    (map node-type-form forms)))

(defn defaults
  [node-type]
  (map-vals t/default-property-value (t/properties' node-type)))

(defn classname-for [prefix]  (symbol (str prefix "__")))

(defn- state-vector
  [node-type]
  (vec (map (comp symbol name) (keys (t/properties' node-type)))))

(defn- message-processors
  [node-type-name node-type]
  (when (not-empty (t/events' node-type))
    `[t/MessageTarget
      (dynamo.types/process-one-event
       [~'self ~'event]
       (case (:type ~'event)
         ~@(mapcat (fn [e] [e `((get-in ~node-type-name [:event-handlers ~e]) ~'self ~'event)]) (t/events' node-type))
         nil))]))

(defn- generate-node-record-sexps
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

(defn- interpose-every
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









































; ---------------------------------------------------------------------------
; Dependency Injection
; ---------------------------------------------------------------------------

(defn- scoped-name
  [scope node]
  (:name node))

(defnk scope-dictionary
  [this nodes]
  (reduce
    (fn [m n]
      (assoc m
             (scoped-name this n)
             n))
    {}
    nodes))

(defn compatible?
  [[out-node out-label out-type in-node in-label in-type]]
  (cond
   (and (= out-label in-label) (t/compatible? out-type in-type false))
   [out-node out-label in-node in-label]

   (and (= (plural out-label) in-label) (t/compatible? out-type in-type true))
   [out-node out-label in-node in-label]))

(defn injection-candidates
  [targets nodes]
  (into #{}
     (keep compatible?
        (for [target  targets
              i       (t/injectable-inputs target)
              :let    [i-l (get (t/input-types target) i)]
              node    nodes
              [o o-l] (t/transform-types node)]
            [node o o-l target i i-l]))))


(ns internal.node
  (:require [camel-snake-kebab :refer [->kebab-case]]
            [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [clojure.core.match :refer [match]]
            [clojure.set :refer [rename-keys union]]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [dynamo.types :as t]
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
(defn node-inputs [v]            (into #{} (keys (-> v :descriptor :inputs))))
(defn node-input-types [v]       (-> v :descriptor :inputs))
(defn node-injectable-inputs [v] (-> v :descriptor :injectable-inputs))
(defn node-outputs [v]           (into #{} (keys (-> v :descriptor :transforms))))
(defn node-output-types [v]      (-> v :descriptor :transform-types))
(defn node-cached-outputs [v]    (-> v :descriptor :cached))

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
    (let [schema (get-in target-node [:descriptor :inputs target-label])
          output-transform (get-in target-node [:descriptor :transforms target-label])]
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
  (let [transform (get-in node [:descriptor :transforms label])]
    (assert transform (str "There is no transform " label " on node " (:_id node)))
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
(defn classname-for [prefix]  (symbol (str prefix "__")))

(defn fqsymbol
  [s]
  (if (symbol? s)
    (let [{:keys [ns name]} (meta (resolve s))]
      (symbol (str ns) (str name)))))

(defn classname-sym
  [cls]
  (when (class? cls)
    (symbol (.getName ^Class cls))))

(defn- property-symbols [behavior]
  (map (comp symbol name) (keys (:properties behavior))))

(defn state-vector [behavior]
  (into [] (list* 'inputs 'transforms '_id (property-symbols behavior))))

(defn defaults
  [{:keys [properties] :as descriptor}]
  (zipmap (keys properties)
          (map t/default-property-value (vals properties))))

(def ^:private property-flags #{:cached :on-update :abstract})
(def ^:private option-flags #{:substitute-value})

(defn parse-output-flags [args]
  (loop [properties #{}
         options {}
         args args]
    (if-let [[arg & remainder] (seq args)]
      (cond
        (contains? property-flags arg) (recur (conj properties arg) options remainder)
        (contains? option-flags arg)   (do (assert remainder (str "Expected value for option " arg))
                                         (recur properties (assoc options arg (first remainder)) (rest remainder)))
        :else {:properties properties :options options :remainder args})
      {:properties properties :options options :remainder args})))

(defn- emit-quote [form]
  (list `quote form))

(defn- compile-defnode-form
  [prefix form]
  (match [form]
     [(['inherits super] :seq)]
     (let [super-descriptor (resolve super)]
       (assert super-descriptor (str "Cannot resolve " super " to a node definition."))
       (deref super-descriptor))

     [(['property nm tp & options] :seq)]
     (let [property-desc (eval (ip/property-type-descriptor nm tp options))]
       {:properties      {(keyword nm) (emit-quote property-desc)}
        :transforms      {(keyword nm) {:production-fn (eval `(fnk [~nm] ~nm))}}
        :transform-types {(keyword nm) (emit-quote (:value-type property-desc))}})

     [(['input nm schema & flags] :seq)]
     (let [schema (if (coll? schema) (into [] schema) schema)
           label  (keyword nm)]
       (if (some #{:inject} flags)
         {:inputs {label schema} :injectable-inputs #{label}}
         {:inputs {label schema}}))

     [(['on evt & fn-body] :seq)]
     {:event-handlers {(keyword evt)
                       `(fn [~'self ~'event]
                          (dynamo.system/transactional ~@fn-body))}}

     [(['output nm output-type & remainder] :seq)]
     (let [oname (keyword nm)
           {:keys [properties options remainder]} (parse-output-flags remainder)
           [args-or-ref & remainder] remainder]
       (assert (not (keyword? output-type)) "The output type seems to be missing")
       (assert (or (:abstract properties) (and (vector? args-or-ref) (seq remainder))
                 (and (symbol? args-or-ref) (var? (resolve args-or-ref)) (empty? remainder)))
         (str "An output clause must have a name, optional flags, and type, before the fn-tail or function name."))
       (let [production-fn (cond
                             (:abstract properties) (abstract-function prefix nm output-type)
                             (vector? args-or-ref)  `(fn ~args-or-ref ~@remainder)
                             :else                  (resolve args-or-ref))
             tform (merge {:production-fn production-fn}
                     (when (contains? options :substitute-value) {:substitute-value-fn (:substitute-value options)}))]
         (reduce
           (fn [m f] (assoc m f #{oname}))
           {:transforms {oname tform}
            :transform-types {oname output-type}}
           properties)))

     [([nm [& args] & remainder] :seq)]
     {:methods        {nm `(fn ~args ~@remainder)}
      :record-methods #{[prefix nm args]}}

     [impl :guard symbol?]
     (if (class? (resolve impl))
       {:interfaces #{impl}}
       {:impl #{impl}})))

(defn resolve-or-else [sym]
  (assert (symbol? sym) (pr-str "Cannot resolve " sym))
  (if-let [val (resolve sym)]
    val
    (throw (ex-info (str "Unable to resolve symbol " sym " in this context") {:symbol sym}))))

(defn generate-message-processor
  [name descriptor]
  (let [event-types (keys (:event-handlers descriptor))]
    `(dynamo.types/process-one-event
       [~'self ~'event]
       (case (:type ~'event)
         ~@(mapcat (fn [e] [e `((get-in ~name [:event-handlers ~e]) ~'self ~'event)]) event-types)
         nil)
       :ok)))

(defn- is-schema? [val] (boolean (:schema (meta val))))

(defn- deep-merge
  [& vals]
  (cond
    (every? is-schema? vals) (last vals)
    (every? map? vals)       (apply merge vals)
    (every? set? vals)       (apply union vals)
    :else                    (last vals)))

(defn compile-specification
  [nm forms]
  (let [behavior-fragments (map #(compile-defnode-form nm %) forms)
        behavior           (apply merge-with deep-merge behavior-fragments)]
    (assoc behavior :name nm)))

(defn- generate-record-methods [descriptor]
  (for [[defined-in method-name args] (:record-methods descriptor)]
    `(~method-name ~args ((get-in ~defined-in [:methods ~(emit-quote method-name)]) ~@args))))

(defn- invert-map
  [m]
  (apply merge-with into
         (for [[k vs] m
               v vs]
           {v #{k}})))

(defn- inputs-for
  [transform]
  (let [production-fn (-> transform :production-fn t/var-get-recursive)]
    (if (pfnk? production-fn)
      (into #{} (keys (dissoc (pf/input-schema production-fn) s/Keyword :this :g)))
      #{})))

(defn- descriptor->output-dependencies
   [{:keys [transforms properties]}]
   (let [outs (dissoc transforms :self)
         outs (zipmap (keys outs) (map inputs-for (vals outs)))
         outs (assoc outs :properties (set (keys properties)))]
     (invert-map outs)))

(defn generate-defrecord [nm descriptor]
  (list* `defrecord (classname-for nm) (state-vector descriptor)
         `dynamo.types/Node
         `(properties [t#] (:properties ~nm))
         `(inputs [t#] (into #{} (keys (:inputs ~nm))))
         `(outputs [t#] (into #{} (keys (:transforms ~nm))))
         `(auto-update? [t# l#] (contains? (-> t# :descriptor :on-update) l#))
         `(cached-outputs [t#] (:cached ~nm))
         `(output-dependencies [t#] ~(descriptor->output-dependencies descriptor))
         `t/MessageTarget
         (generate-message-processor nm descriptor)
         (concat
          (map #(or (fqsymbol %) %) (:impl descriptor))
          (map #(or (classname-sym %) %) (:interfaces descriptor))
          (generate-record-methods descriptor))))

(defn- maps [f coll] (into #{} (map f coll)))

(defn quote-functions
  [descriptor]
  (-> descriptor
    (update-in [:name]           emit-quote)
    (update-in [:methods]        (fn [ms] (zipmap (map emit-quote (keys ms)) (vals ms))))
    (update-in [:record-methods] (partial maps emit-quote))
    (update-in [:impl]           (partial maps (comp emit-quote fqsymbol)))))

(defn generate-constructor [nm descriptor]
  (let [ctor             (symbol (str 'make- (->kebab-case (str nm))))
        record-ctor      (symbol (str 'map-> (classname-for nm)))]
    `(defn ~ctor
       ~(str "Constructor for " nm ", using default values for any property not in property-values.\nThe properties on " nm " are:\n" #_(describe-properties behavior))
       [& {:as ~'property-values}]
       (~record-ctor (merge {:_id (tempid)}
                            ~{:descriptor nm}
                            (defaults ~nm)
                            ~'property-values)))))

(defn generate-descriptor [nm descriptor]
  `(def ~nm ~(quote-functions descriptor)))

(defn generate-print-method [nm]
  (let [classname (classname-for nm)
        tagged-arg (vary-meta (gensym "v") assoc :tag (resolve classname))]
    `(defmethod print-method ~classname
       [~tagged-arg w#]
       (.write ^java.io.Writer w# (str "<" ~(str nm)
                                       (merge (select-keys ~tagged-arg [:_id])
                                              (select-keys ~tagged-arg (-> ~tagged-arg :descriptor :properties keys)))
                                       ">")))))

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
              i       (node-injectable-inputs target)
              :let    [i-l (get (node-input-types target) i)]
              node    nodes
              [o o-l] (node-output-types node)]
            [node o o-l target i i-l]))))

(defn validate-descriptor [nm descriptor]
  (doseq [[property-name property-type] (:properties descriptor)]
    (assert (satisfies? t/PropertyType property-type)
            (str "Node " nm ", property " (name property-name) " has type " property-type ". Expected instance of " `t/PropertyType))))

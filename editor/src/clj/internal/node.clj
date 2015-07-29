(ns internal.node
  (:require [clojure.core.match :refer [match]]
            [clojure.pprint :as pp]
            [clojure.set :as set]
            [dynamo.util :as util]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.property :as ip]
            [plumbing.core :as pc]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s])
  (:import [internal.graph.types IBasis]))

(defn warn [node-id node-type label input-schema error]
  (println "WARNING-SCHEMA-VALIDATION:" (format "<NODE|%s|%s|%s>"  node-id  (:name node-type) label))
  (doseq [[key val] (s/explain input-schema)]
    (println (format "========EXPECTED-%s========" key))
    (pp/pprint val)
    (println (format "========VALIDATION ERROR %s========" key))
    (pp/pprint (get error key))))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [^IBasis basis cache node-or-node-id label]
  (let [node               (ig/node-by-id-at basis (if (gt/node? node-or-node-id) (gt/node-id node-or-node-id) node-or-node-id))
        evaluation-context {:local    (atom {})
                            :snapshot (if cache (c/cache-snapshot cache) {})
                            :hits     (atom [])
                            :basis    basis
                            :in-production []}
        result             (and node (gt/produce-value node label evaluation-context))]
    (when (and node cache)
      (let [local             @(:local evaluation-context)
            local-for-encache (for [[node-id vmap] local
                                    [output val] vmap]
                                [[node-id output] val])]
        (c/cache-hit cache @(:hits evaluation-context))
        (c/cache-encache cache local-for-encache)))
    result))

(defn- all-properties [node-type]
  (merge (gt/properties node-type) (gt/internal-properties node-type)))

(defn- gather-property
  [self kwargs prop]
  (let [type              (-> self gt/node-type gt/properties prop)
        value             (get self prop)
        problems          (gt/property-validate type value)
        dynamics          (util/map-vals #(% kwargs) (gt/dynamic-attributes type))]
    (merge dynamics
           {:node-id             (gt/node-id self)
            :value               value
            :type                type
            :validation-problems problems})))

(defn- gather-properties
  "Production function that delivers the definition and value for all
  properties of this node. This is used to create the :_properties
  output on a node. You should not call it directly. Instead,
  call `(g/node-value _n_ :_properties)`"
  [kwargs]
  (let [self           (:_self kwargs)
        kwargs         (dissoc kwargs :_self)
        property-names (-> self gt/node-type gt/properties keys)]
    (zipmap property-names (map (partial gather-property self kwargs) property-names))))

;; ---------------------------------------------------------------------------
;; Definition handling
;; ---------------------------------------------------------------------------
(defrecord NodeTypeImpl
    [name supertypes interfaces protocols method-impls triggers transforms transform-types internal-properties properties inputs injectable-inputs cached-outputs event-handlers input-dependencies substitutes cardinalities property-types property-passthroughs]

  gt/NodeType
  (supertypes            [_] supertypes)
  (interfaces            [_] interfaces)
  (protocols             [_] protocols)
  (method-impls          [_] method-impls)
  (triggers              [_] triggers)
  (transforms            [_] transforms)
  (transform-types       [_] transform-types)
  (internal-properties   [_] internal-properties)
  (properties            [_] properties)
  (declared-inputs       [_] inputs)
  (injectable-inputs     [_] injectable-inputs)
  (declared-outputs      [_] (set (keys transforms)))
  (cached-outputs        [_] cached-outputs)
  (event-handlers        [_] event-handlers)
  (input-dependencies    [_] input-dependencies)
  (substitute-for        [_ input] (get substitutes input))
  (input-type            [_ input] (get inputs input))
  (input-cardinality     [_ input] (get cardinalities input))
  (output-type           [_ output] (get transform-types output))
  (property-type         [_ output] (get property-types output))
  (property-passthrough? [_ output] (contains? property-passthroughs output)))

(defmethod print-method NodeTypeImpl
  [^NodeTypeImpl v ^java.io.Writer w]
  (.write w (str "<NodeTypeImpl{:name " (:name v) ", :supertypes " (mapv :name (:supertypes v)) "}>")))

(defn- from-supertypes [local op]                (map op (:supertypes local)))
(defn- combine-with    [local op zero into-coll] (op (reduce op zero into-coll) local))

(declare attach-output)

(defn- attribute-fn-arguments
  [f]
  (keys (dissoc (pf/input-schema f) s/Keyword)))

(defn- property-dynamics-arguments
  [[property-name property-definition]]
  (mapcat attribute-fn-arguments (vals (gt/dynamic-attributes property-definition))))

(defn- properties-output-arguments
  [properties]
  (cons :_self (concat (set (keys properties))
                      (mapcat property-dynamics-arguments properties))))

(defn attach-properties-output
  [node-type-description]
  (let [properties      (:properties node-type-description)
        argument-names  (properties-output-arguments properties)
        argument-schema (zipmap argument-names (repeat s/Any))]
    (attach-output node-type-description :_properties s/Any #{} #{}
                   (s/schematize-fn (fn [args] (gather-properties args)) (s/=> s/Any argument-schema)))))

(defn keyset
  [m]
  (set (keys m)))

(defn verify-inputs-for-dynamics
  [node-type-description]
  (doseq [property (:properties node-type-description)]
    (let [args         (set (property-dynamics-arguments property))
          missing-args (reduce set/difference args [(keyset (:inputs node-type-description)) (keyset (:properties node-type-description))])]
      (assert (empty? missing-args) (str "Node " (:name node-type-description) " must have inputs or properties for the label(s) "
                                         missing-args ", because they are needed by its property '" (name (first property)) "'."))))
  node-type-description)

(defn invert-map
  [m]
  (apply merge-with into
         (for [[k vs] m
               v vs]
           {v #{k}})))

(defn inputs-for
  [production-fn]
  (if (gt/pfnk? production-fn)
    (into #{} (keys (dissoc (pf/input-schema production-fn) s/Keyword :this :g)))
    #{}))

(defn dependency-seq
  ([desc inputs]
   (dependency-seq desc #{} inputs))
  ([desc seen inputs]
   (reduce
    (fn [dependencies argument]
      (conj
       (if (not (seen argument))
         (if-let [recursive (get-in desc [:transforms argument])]
           (into dependencies (dependency-seq desc (conj seen argument) (inputs-for recursive)))
           dependencies)
         dependencies)
       argument))
    #{}
    inputs)))

(defn description->input-dependencies
  [{:keys [transforms properties] :as description}]
  (let [transforms (zipmap (keys transforms) (map #(dependency-seq description (inputs-for %)) (vals transforms)))
        transforms (assoc transforms :_self (keys properties))]
    (invert-map transforms)))

(defn attach-input-dependencies
  [description]
  (assoc description :input-dependencies (description->input-dependencies description)))

(defn input-dependencies-non-transitive
  "Return a map from input to affected outputs, but without including
  the transitive effects on other outputs within the same node
  type. This is a specialized case and if it's not apparent what it
  means, you should probably call input-dependencies instead."
  [node-type]
  (let [transforms (dissoc (gt/transforms node-type) :_self)]
    (invert-map
     (zipmap (keys transforms)
             (map #(inputs-for %) (vals transforms))))))

(def ^:private map-merge (partial merge-with merge))

(defn make-node-type
  "Create a node type object from a maplike description of the node.
  This is really meant to be used during macro expansion of `defnode`,
  not called directly."
  [description]
  (-> description
      (update-in [:inputs]                combine-with merge      {} (from-supertypes description gt/declared-inputs))
      (update-in [:injectable-inputs]     combine-with set/union #{} (from-supertypes description gt/injectable-inputs))
      (update-in [:internal-properties]   combine-with merge      {} (from-supertypes description gt/internal-properties))
      (update-in [:properties]            combine-with merge      {} (from-supertypes description gt/properties))
      (update-in [:transforms]            combine-with merge      {} (from-supertypes description gt/transforms))
      (update-in [:transform-types]       combine-with merge      {} (from-supertypes description gt/transform-types))
      (update-in [:cached-outputs]        combine-with set/union #{} (from-supertypes description gt/cached-outputs))
      (update-in [:event-handlers]        combine-with set/union #{} (from-supertypes description gt/event-handlers))
      (update-in [:interfaces]            combine-with set/union #{} (from-supertypes description gt/interfaces))
      (update-in [:protocols]             combine-with set/union #{} (from-supertypes description gt/protocols))
      (update-in [:method-impls]          combine-with merge      {} (from-supertypes description gt/method-impls))
      (update-in [:triggers]              combine-with map-merge  {} (from-supertypes description gt/triggers))
      (update-in [:substitutes]           combine-with merge      {} (from-supertypes description :substitutes))
      (update-in [:cardinalities]         combine-with merge      {} (from-supertypes description :cardinalities))
      (update-in [:property-types]        combine-with merge      {} (from-supertypes description :property-types))
      (update-in [:property-passthroughs] combine-with set/union #{} (from-supertypes description :property-passthroughs))
      attach-properties-output
      attach-input-dependencies
      verify-inputs-for-dynamics
      map->NodeTypeImpl))

(def ^:private inputs-properties (juxt :inputs :properties))

(defn- assert-form-kind [kind-label required-kind label form]
  (assert (required-kind form) (str "defnode " label " requires a " kind-label " not a " (class form) " of " form)))

(def assert-symbol (partial assert-form-kind "symbol" symbol?))
(def assert-schema (partial assert-form-kind "schema" util/schema?))
(defn assert-pfnk [label production-fn]
  (assert
   (gt/pfnk? production-fn)
   (format "Node output %s needs a production function that is a dynamo.graph/fnk" label)))

(defn- name-available
  [description label]
  (not (some #{label} (mapcat keys (inputs-properties description)))))

(defn attach-supertype
  "Update the node type description with the given supertype."
  [description supertype]
  (assoc description :supertypes (conj (:supertypes description []) supertype)))

(defn attach-input
  "Update the node type description with the given input."
  [description label schema flags options & [args]]
  (assert (name-available description label) (str "Cannot create input " label ". The id is already in use."))
  (assert (not (gt/protocol? schema))
          (format "Input %s on node type %s looks like its type is a protocol. Wrap it with (dynamo.graph/protocol) instead" label (:name description)))
  (assert-schema "input" schema)

  (let [property-schema (if (satisfies? gt/PropertyType schema) (gt/property-value-type schema) schema)]
    (cond->
        (assoc-in description [:inputs label] property-schema)

      (some #{:inject} flags)
      (update-in [:injectable-inputs] #(conj (or % #{}) label))

      (:substitute options)
      (update-in [:substitutes] assoc label (:substitute options))

      (not (some #{:array} flags))
      (update-in [:cardinalities] assoc label :one)

      (some #{:array} flags)
      (update-in [:cardinalities] assoc label :many))))

(defn- abstract-function
  [label type]
  (pc/fnk [this]
          (throw (AssertionError.
                  (format "Node %d does not supply a production function for the abstract '%s' output. Add (output %s %s your-function) to the definition of %s"
                          (gt/node-id this) label
                          label type this)))))

(defn attach-output
  "Update the node type description with the given output."
  [description label schema properties options & remainder]
  (assert-schema "output" schema)
  (let [production-fn (first remainder)]
    (when-not (:abstract properties)
      (assert-pfnk label production-fn))
    (assert
     (empty? (rest remainder))
     (format "Options and flags for output %s must go before the production function." label))
    (cond-> (update-in description [:transform-types] assoc label schema)

      (:cached properties)
      (update-in [:cached-outputs] #(conj (or % #{}) label))

      (:abstract properties)
      (update-in [:transforms] assoc-in [label] (abstract-function label schema))

      (not (:abstract properties))
      (update-in [:transforms] assoc-in [label] production-fn)

      true
      (update-in [:property-passthroughs] disj label))))

(def ^:private internal-keys #{:_id :_output-jammers})

(defn attach-property
  "Update the node type description with the given property."
  [description label property-type passthrough]
  (let [prop-key (if (contains? internal-keys label) :internal-properties :properties)]
   (cond-> (update-in description [prop-key] assoc label property-type)
     true
     (assoc-in [:property-types label] property-type)

     true
     (update-in [:transforms] assoc-in [label] passthrough)

     true
     (update-in [:transform-types] assoc label (:value-type property-type))

     true
     (update-in [:property-passthroughs] #(conj (or % #{}) label)))))

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

(defn- parse-flags-and-options
  [allowed-flags allowed-options args]
  (loop [flags   #{}
         options {}
         args    args]
    (if-let [[arg & remainder] (seq args)]
      (cond
        (allowed-flags   arg) (recur (conj flags arg) options remainder)
        (allowed-options arg) (do (assert remainder (str "Expected value for option " arg))
                                  (recur flags (assoc options arg (first remainder)) (rest remainder)))
        :else                 [flags options args])
      [flags options args])))

(defn classname
  [^Class c]
  (.getName c))

(defn fqsymbol
  [s]
  (assert (symbol? s))
  (let [{:keys [ns name]} (meta (resolve s))]
    (symbol (str ns) (str name))))

(def ^:private valid-trigger-kinds #{:added :deleted :property-touched :input-connections})
(def ^:private input-flags   #{:inject :array})
(def ^:private input-options #{:substitute})

(def ^:private output-flags   #{:cached :abstract})
(def ^:private output-options #{})

(defn- node-type-form
  "Translate the sugared `defnode` forms into function calls that
  build the node type description (map). These are emitted where you
  invoked `defnode` so that symbols and vars resolve correctly."
  [form]
  (match [form]
         [(['inherits supertype] :seq)]
         (do (assert-symbol "inherits" supertype)
             `(attach-supertype ~supertype))

         [(['input label schema & remainder] :seq)]
         (do (assert-symbol "input" label)
             (let [[properties options args] (parse-flags-and-options input-flags input-options remainder)]
               `(attach-input ~(keyword label) ~schema ~properties ~options ~@args)))

         [(['output label schema & remainder] :seq)]
         (do (assert-symbol "output" label)
             (let [[properties options args] (parse-flags-and-options output-flags output-options remainder)]
               (assert (or (:abstract properties) (not (empty? args)))
                       (format "The output %s is missing a production function. Either define the production function or mark it as :abstract." label))
               `(attach-output ~(keyword label) ~schema ~properties ~options ~@args)))

         [(['property label tp & options] :seq)]
         (do (assert-symbol "property" label)
             `(attach-property ~(keyword label) ~(ip/property-type-descriptor (str label) tp options) (pc/fnk [~label] ~label)))

         [(['on label & fn-body] :seq)]
         `(attach-event-handler ~(keyword label) (fn [~'self ~'event] ~@fn-body))

         [(['trigger label & rest] :seq)]
         (do
           (assert-symbol "trigger" label)
           (let [kinds (vec (take-while keyword? rest))
                action (drop-while keyword? rest)]
             (assert (every? valid-trigger-kinds kinds) (apply str "Invalid trigger kind. Valid trigger kinds are: " (interpose ", " valid-trigger-kinds)))
             (assert (not (empty? action)) "Trigger must have an action")
            `(attach-trigger ~(keyword label) ~kinds ~@action)))

         ;; Interface or protocol function
         [([nm [& argvec] & remainder] :seq)]
         `(attach-method-implementation '~nm '~argvec (fn ~argvec ~@remainder))

         [impl :guard symbol?]
         `(cond->
              (class? ~impl)
            (attach-interface (symbol (classname ~impl)))

            (not (class? ~impl))
            (attach-protocol (fqsymbol '~impl)))))

(defn node-type-forms
  "Given all the forms in a defnode macro, emit the forms that will build the node type description."
  [symb forms]
  (concat [`-> {:name (str (symbol (str *ns*) (str symb)))}]
          (map node-type-form forms)))

(defn defaults
  "Return a map of default values for the node type."
  [node-type]
  (util/map-vals gt/property-default-value (gt/properties node-type)))

(defn- has-multivalued-input?  [node-type input-label] (= :many (gt/input-cardinality node-type input-label)))
(defn- has-singlevalued-input? [node-type input-label] (= :one (gt/input-cardinality node-type input-label)))

(defn has-input?    [node-type argument] (gt/input-type node-type argument))
(defn has-property? [node-type argument] (gt/property-type node-type argument))
(defn has-output?   [node-type argument] (gt/output-type node-type argument))

(defn- property-overloads-output? [node-type argument output] (and (= output argument) (has-property? node-type argument)))
(defn- unoverloaded-output? [node-type argument output] (and (not= output argument) (has-output? node-type argument)))

(defn- property-schema
  [node-type property]
  (let [schema (gt/property-type node-type property)]
    (if (satisfies? gt/PropertyType schema) (gt/property-value-type schema) schema)))

(defn- dollar-name
  [node-type-name label]
  (symbol (str node-type-name "$" (name label))))

(defn- deduce-argument-type
  "Return the type of the node's input label (or property). Take care
  with :array inputs."
  [record-name node-type argument output]
  (cond
    (property-overloads-output? node-type argument output)
    (s/maybe (property-schema node-type argument))

    (unoverloaded-output? node-type argument output)
    (s/maybe (gt/output-type node-type argument))

    (has-multivalued-input? node-type argument)
    [(s/maybe (gt/input-type node-type argument))]

    (has-singlevalued-input? node-type argument)
    (s/maybe (gt/input-type node-type argument))

    (has-output? node-type argument)
    (s/maybe (gt/output-type node-type argument))

    (= :this argument)
    record-name

    (has-property? node-type argument)
    (s/maybe (property-schema node-type argument))))

(defn- collect-argument-schema
  "Return a schema with the production function's input names mapped to the node's corresponding input type."
  [transform argument-schema record-name node-type]
  (persistent!
   (reduce-kv
    (fn [arguments desired-argument-name _]
      (if (= s/Keyword desired-argument-name)
        arguments
        (let [argument-type (deduce-argument-type record-name node-type desired-argument-name transform)]
          (assoc! arguments desired-argument-name (or argument-type s/Any)))))
    (transient {})
    argument-schema)))

(defn- produce-value-form
  [node-sym output context-sym]
  `(gt/produce-value ~node-sym ~output ~context-sym))

(defn- input-value-forms
  [input]
  `(mapv (fn [[~'node-id ~'output-label]]
           (let [~'node (ig/node-by-id-at (:basis ~'evaluation-context) ~'node-id)]
             ~(produce-value-form 'node 'output-label 'evaluation-context)))
         (gt/sources (:basis ~'evaluation-context) (gt/node-id ~'this) ~input)))

(defn- lookup-multivalued-input
  [node-type-name node-type input]
  (if (gt/substitute-for node-type input)
    `(let [inputs#  ~(input-value-forms input)
           sub#     (gt/substitute-for ~node-type-name ~input)]
       (map #(if (gt/error? %) (util/apply-if-fn sub#) %) inputs#))
    (input-value-forms input)))

(defn- lookup-singlevalued-input
  [node-type-name node-type input]
  (if (gt/substitute-for node-type input)
    `(let [inputs#     ~(input-value-forms input)
           no-input?#  (empty? inputs#)
           input#      (first inputs#)
           sub#        (gt/substitute-for ~node-type-name ~input)]
       (if (or no-input?# (gt/error? input#))
         (util/apply-if-fn sub#)
         input#))
    `(first ~(input-value-forms input))))

(defn- input-lookup-forms
  [node-type-name node-type input]
  (cond
    (has-multivalued-input? node-type input)
    (lookup-multivalued-input node-type-name node-type input)

    (has-singlevalued-input? node-type input)
    (lookup-singlevalued-input node-type-name node-type input)))

(defn- node-input-forms
  [output node-type-name node-type [argument schema]]
  (cond
    (property-overloads-output? node-type argument output)
    `(get ~'this ~argument)

    (unoverloaded-output? node-type argument output)
    (produce-value-form 'this argument 'evaluation-context)

    (has-multivalued-input? node-type argument)
    (lookup-multivalued-input node-type-name node-type argument)

    (has-singlevalued-input? node-type argument)
    (lookup-singlevalued-input node-type-name node-type argument)

    (has-output? node-type argument)
    (produce-value-form 'this argument 'evaluation-context)

    (= :this argument)
    'this

    (has-property? node-type argument)
    `(get ~'this ~argument)))

(defn produce-value-forms [transform output-multi? node-type-name argument-forms argument-schema epilogue]
  `(let [pfn-input# ~argument-forms
         schema#    ~argument-schema]
     (if-let [~'error (some gt/error? (vals pfn-input#))]
       ~(if output-multi? `[~'error] 'error)
       (if-let [validation-error# (s/check schema# pfn-input#)]
         (do
           (warn (gt/node-id ~'this) ~node-type-name ~transform schema# validation-error#)
           (throw (ex-info "SCHEMA-VALIDATION"
                           {:node-id (gt/node-id ~'this) :type ~node-type-name :output ~transform
                            :expected schema# :actual pfn-input#
                            :validation-error validation-error#})))
         (let [~'result ((~transform (gt/transforms ~node-type-name)) pfn-input#)]
           ~epilogue
           ~'result)))))

(defn local-cache [evaluation-context transform]
  `(get-in @(:local ~evaluation-context) [(gt/node-id ~'this) ~transform]))

(defn global-cache [evaluation-context transform]
  `(if-some [cached# (get (:snapshot ~evaluation-context) [(gt/node-id ~'this) ~transform])]
     (do (swap! (:hits ~evaluation-context) conj [(gt/node-id ~'this) ~transform]) cached#)))

(defn node-output-value-function-forms
  [record-name node-type-name node-type]
  (for [[transform pfn]       (gt/transforms node-type)
        :let [funcname              (with-meta (dollar-name node-type-name transform) {:no-doc true})
              property-passthrough? (gt/property-passthrough? node-type transform)
              cached?               ((gt/cached-outputs node-type) transform)
              output-multi?         (seq? ((gt/transform-types node-type) transform))
              argument-schema       (collect-argument-schema transform (pf/input-schema pfn) record-name node-type)
              argument-forms        (zipmap (keys argument-schema)
                                            (map (partial node-input-forms transform node-type-name node-type) argument-schema))
              epilogue              (when cached?
                                      `(swap! (:local ~'evaluation-context) assoc-in [(gt/node-id ~'this) ~transform] ~'result))
              refresh               `(ig/node-by-id-at (:basis ~'evaluation-context) (gt/node-id ~'this))
              lookup                (if cached?
                                      `(or ~(local-cache 'evaluation-context transform)
                                           ~(global-cache 'evaluation-context transform)
                                           ~(produce-value-forms transform output-multi? node-type-name argument-forms argument-schema epilogue))
                                      (produce-value-forms transform output-multi? node-type-name argument-forms argument-schema epilogue))]]
    `(defn ~funcname [~'this ~'evaluation-context]
       (if-let [jammer# (get (:_output-jammers ~'this) ~transform)]
         (jammer#)
         ~(if property-passthrough?
            `(get ~'this ~transform)
            `(do
               (assert (every? #(not= % [(gt/node-id ~'this) ~transform]) (:in-production ~'evaluation-context))
                       (format "Cycle Detected on node type %s and output %s" (:name ~node-type-name) ~transform))
               (let [~'evaluation-context (update ~'evaluation-context :in-production conj [(gt/node-id ~'this) ~transform])]
                 ~(if (= transform :_self)
                    refresh
                    (if (= transform :this)
                      (gt/node-id ~'this)
                      lookup)))))))))


(defn node-input-value-function-forms
  [record-name node-type-name node-type]
  (for [[input input-schema] (gt/declared-inputs node-type)]
    (let [funcname (with-meta (dollar-name node-type-name input) {:no-doc true})]
     `(defn ~funcname [~'this ~'evaluation-context]
        ~(input-lookup-forms node-type-name node-type input)))))

(defn define-node-value-functions
  [record-name node-type-name node-type]
  (eval
   `(do
      ~@(node-input-value-function-forms record-name node-type-name node-type)
      ~@(node-output-value-function-forms record-name node-type-name node-type))))

(defn node-value-function-names
  [node-type-name node-type]
  (map (partial dollar-name node-type-name)
       (concat (keys (gt/transforms node-type))
               (keys (gt/declared-inputs node-type)))))

(defn declare-node-value-function-names
  [node-type-name node-type]
  (eval `(declare ~@(node-value-function-names node-type-name node-type))))

(defn classname-for [prefix] (symbol (str prefix "__")))

(defn- state-vector
  [node-type]
  (mapv (comp symbol name) (keys (all-properties node-type))))

(defn- subtract-keys
  [m1 m2]
  (set/difference (set (keys m1)) (set (keys m2))))

(defn- node-record-sexps
  [record-name node-type-name node-type]
  `(do
     (defrecord ~record-name ~(state-vector node-type)
       gt/Node
       (node-id        [this#]    (:_id this#))
       (node-type      [_]        ~node-type-name)
       (produce-value  [~'this label# ~'evaluation-context]
         (case label#
           ~@(mapcat (fn [an-output] [an-output (list (dollar-name node-type-name an-output) 'this 'evaluation-context)])
                     (keys (gt/transforms node-type)))
           ~@(mapcat (fn [an-input]  [an-input  (list (dollar-name node-type-name an-input) 'this 'evaluation-context)])
                     (subtract-keys (gt/declared-inputs node-type) (gt/transforms node-type)))
           (throw (ex-info (str "No such output, input, or property " label# " exists for node type " (:name ~node-type-name))
                           {:label label# :node-type ~node-type-name}))))
       ~@(gt/interfaces node-type)
       ~@(gt/protocols node-type)
       ~@(map (fn [[fname [argv _]]] `(~fname ~argv ((second (get (gt/method-impls ~node-type-name) '~fname)) ~@argv))) (gt/method-impls node-type)))
     (alter-meta! (var ~(symbol (str "map->" record-name))) assoc :no-doc true)
     (alter-meta! (var ~(symbol (str "->" record-name))) assoc :no-doc true)))

(defn define-node-record
  "Create a new class for the node type. This builds a defrecord with
  the node's properties as fields. The record will implement all the interfaces
  and protocols that the node type requires."
  [record-name node-type-name node-type]
  (eval (node-record-sexps record-name node-type-name node-type)))

(defn- interpose-every
  [n elt coll]
  (mapcat (fn [l r] (conj l r)) (partition-all n coll) (repeat elt)))

(defn- print-method-forms
  [record-name node-type-name node-type]
  (let [node (vary-meta 'node assoc :tag (resolve record-name))]
    `(defmethod print-method ~record-name
       [~node w#]
       (.write
        ^java.io.Writer w#
        (str "#" '~node-type-name "{" (:_id ~node)
             ~@(interpose-every 3 ", "
                                (mapcat (fn [prop] `[~prop " " (pr-str (get ~node ~prop))])
                                        (keys (all-properties node-type))))
             "}")))))

(defn define-print-method
  "Create a nice print method for a node type. This avoids infinitely recursive output in the REPL."
  [record-name node-type-name node-type]
  (eval (print-method-forms record-name node-type-name node-type)))

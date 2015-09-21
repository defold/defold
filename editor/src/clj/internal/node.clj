(ns internal.node
  (:require [clojure.core.match :refer [match]]
            [clojure.pprint :as pp]
            [clojure.set :as set]
            [dynamo.util :as util]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [internal.property :as ip]
            [plumbing.core :as pc]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s])
  (:import [internal.graph.types IBasis]
           [internal.graph.error_values ErrorValue]))

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
  [^IBasis node-or-node-id label {:keys [cache basis ignore-errors] :or {ignore-errors 0} :as options}]
  (let [caching?           (and (not (:no-cache options)) cache)
        cache              (when caching? cache)
        node               (ig/node-by-id-at basis (if (gt/node? node-or-node-id) (gt/node-id node-or-node-id) node-or-node-id))
        evaluation-context {:local         (atom {})
                            :snapshot      (if caching? (c/cache-snapshot cache) {})
                            :hits          (atom [])
                            :basis         basis
                            :in-production []
                            :ignore-errors ignore-errors}
        result             (and node (gt/produce-value node label evaluation-context))]
    (when (and node caching?)
      (let [local             @(:local evaluation-context)
            local-for-encache (for [[node-id vmap] local
                                    [output val] vmap]
                                [[node-id output] val])]
        (c/cache-hit cache @(:hits evaluation-context))
        (c/cache-encache cache local-for-encache)))
    result))


(defn- display-group?
  [label elem]
  (and (vector? elem) (= label (first elem))))

(defn- display-group
  [order label]
  (first (filter #(display-group? label %) order)))

(defn- join-display-groups
  [[label & _ :as elem] order2]
  (into elem (rest (display-group order2 label))))

(defn- expand-node-types
  [coll]
  (flatten
   (map #(if (gt/node-type? %) (gt/property-display-order %) %) coll)))

(defn merge-display-order
  ([order] order)
  ([order1 order2]
   (loop [result []
          left   order1
          right  order2]
     (if-let [elem (first left)]
       (cond
         (gt/node-type? elem)
         (recur result (concat (expand-node-types [elem]) (next left)) right)

         (keyword? elem)
         (recur (conj result elem) (next left) (remove #{elem} right))

         (sequential? elem)
         (if (some gt/node-type? elem)
           (recur result (cons (expand-node-types elem) (next left)) right)
           (let [group-label   (first elem)
                 group-member? (set (next elem))]
             (recur (conj result (join-display-groups elem right))
                    (next left)
                    (remove #(or (group-member? %) (display-group? group-label %)) right)))))
       (into result right))))
  ([order1 order2 & more]
   (if more
     (recur (merge-display-order order1 order2) (first more) (next more))
     (merge-display-order order1 order2))))

(defn- all-properties [node-type] (gt/declared-properties node-type))

(defn- gather-dynamics
  [{:keys [type] :as prop-snap} kwargs]
  (merge
   (util/map-vals #(% kwargs) (gt/dynamic-attributes type))
   prop-snap))

(defn- rename-key [m ok nk]
  (-> m
      (dissoc ok)
      (assoc nk (get m ok))))

(defn- property-value      [property kwargs] ((ip/getter-for property) kwargs))
(defn- property-validation [property kwargs] ((ip/validation property) kwargs))

(defn- gather-properties
  "Production function that delivers the definition and value for all
  properties of this node. This is used to create the :_properties
  output on a node. You should not call it directly. Instead,
  call `(g/node-value _n_ :_properties)`"
  [{:keys [_node-id basis] :as kwargs}]
  (let [kwargs (dissoc kwargs :_node-id)
        self   (ig/node-by-id-at basis _node-id)]
    {:properties (persistent!
                  (reduce-kv
                   (fn [m property property-type]
                     (let [current-value (property-value property-type kwargs)
                           pval          {:type    property-type
                                          :node-id _node-id}
                           dynamics      (util/map-vals
                                          #(% kwargs)
                                          (gt/dynamic-attributes property-type))
                           pval          (merge pval dynamics)
                           kwargs        (merge kwargs dynamics {:value current-value})
                           ;; TODO - collect all values first, then
                           ;; run all validations with the same kwargs.
                           problems      (when (ip/validation property-type)
                                           (property-validation property-type kwargs))
                           pval          (assoc pval :value
                                                (if problems problems current-value))]
                       (assoc! m property pval)))
                   (transient {})
                   (gt/property-types self)))
     :display-order (-> self gt/node-type gt/property-display-order)}))

(defn setter-for
  [node property]
  (ip/setter-for (-> node gt/node-type gt/declared-properties (get property))))

;; ---------------------------------------------------------------------------
;; Definition handling
;; ---------------------------------------------------------------------------
(defrecord NodeTypeImpl
    [name supertypes interfaces protocols method-impls transforms transform-types declared-properties inputs injectable-inputs cached-outputs input-dependencies substitutes cardinalities cascade-deletes  property-display-order]

  gt/NodeType
  (supertypes            [_] supertypes)
  (interfaces            [_] interfaces)
  (protocols             [_] protocols)
  (method-impls          [_] method-impls)
  (transforms            [_] transforms)
  (transform-types       [_] transform-types)
  (declared-properties   [_] declared-properties)
  (declared-inputs       [_] inputs)
  (injectable-inputs     [_] injectable-inputs)
  (declared-outputs      [_] (set (keys transforms)))
  (cached-outputs        [_] cached-outputs)
  (input-dependencies    [_] input-dependencies)
  (substitute-for        [_ input] (get substitutes input))
  (input-type            [_ input] (get inputs input))
  (input-cardinality     [_ input] (get cardinalities input))
  (cascade-deletes       [_]        cascade-deletes)
  (output-type           [_ output] (get transform-types output))
  (property-passthrough? [_ output] false)
  (property-display-order [this] property-display-order))

(defmethod print-method NodeTypeImpl
  [^NodeTypeImpl v ^java.io.Writer w]
  (.write w (str "<NodeTypeImpl{:name " (:name v) ", :supertypes " (mapv :name (:supertypes v)) "}>")))

(defn- from-supertypes [local op]                (map op (:supertypes local)))
(defn- combine-with    [local op zero into-coll] (op (reduce op zero into-coll) local))

(defn resolve-display-order
  [{:keys [display-order-decl property-order-decl supertypes] :as description}]
  (-> description
      (assoc :property-display-order (apply merge-display-order display-order-decl property-order-decl (map gt/property-display-order supertypes)))
      (dissoc :display-order-decl :property-order-decl)))

(declare attach-output)

(defn attach-properties-output
  [node-type-description]
  (let [properties      (util/filterm (comp not :internal? val) (:declared-properties node-type-description))
        argument-names  (mapcat (comp ip/property-dependencies val) properties)
        argument-schema (zipmap argument-names (repeat s/Any)) ]
    (attach-output
     node-type-description
     :_declared-properties s/Any #{} #{}
     (s/schematize-fn gather-properties (s/=> s/Any argument-schema)))))

(defn keyset
  [m]
  (set (keys m)))

(defn valset
  [m]
  (set (vals m)))

(defn inputs-for
  [pfn]
  (if (gt/pfnk? pfn)
    (disj (util/fnk-arguments pfn) :this :g)
    #{}))

(defn all-inputs-and-properties [node-type-description]
  (keyset
   (merge
    (:inputs node-type-description)
    (:declared-properties node-type-description))))

(defn warn-missing-arguments
  [node-type-description property-name dynamic-name missing-args]
  (str "Node " (:name node-type-description) " must have inputs or properties for the label(s) "
       missing-args ", because they are needed by its property '" (name property-name) "'."))

(defn assert-no-missing-args
  [node-type-description property dynamic missing-args]
  (assert (empty? missing-args)
          (warn-missing-arguments node-type-description (first property) (first dynamic) missing-args)))

(defn missing-inputs-for-dynamic
  [node-type-description dynamic]
  (let [fnk           (second dynamic)
        required-args (inputs-for fnk)]
    (set/difference required-args
                    (all-inputs-and-properties node-type-description))))

(defn verify-inputs-for-dynamics
  [node-type-description]
  (doseq [property (:declared-properties node-type-description)
          dynamic  (gt/dynamic-attributes (second property))
          :let     [missing-args (missing-inputs-for-dynamic node-type-description dynamic)]]
    (assert-no-missing-args node-type-description property dynamic missing-args))
  node-type-description)

(defn invert-map
  [m]
  (apply merge-with into
         (for [[k vs] m
               v vs]
           {v #{k}})))

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
  (let [transforms (zipmap (keys transforms) (map #(dependency-seq description (inputs-for %)) (vals transforms)))]
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
  (let [transforms (gt/transforms node-type)]
    (invert-map
     (zipmap (keys transforms)
             (map #(inputs-for %) (vals transforms))))))

(def ^:private map-merge (partial merge-with merge))
(defn- flip [f] (fn [x y] (f y x)))

(defn make-node-type
  "Create a node type object from a maplike description of the node.
  This is really meant to be used during macro expansion of `defnode`,
  not called directly."
  [description]
  (-> description
      (update-in [:inputs]                combine-with merge      {} (from-supertypes description gt/declared-inputs))
      (update-in [:injectable-inputs]     combine-with set/union #{} (from-supertypes description gt/injectable-inputs))
      (update-in [:declared-properties]   combine-with merge      {} (from-supertypes description gt/declared-properties))
      (update-in [:transforms]            combine-with merge      {} (from-supertypes description gt/transforms))
      (update-in [:transform-types]       combine-with merge      {} (from-supertypes description gt/transform-types))
      (update-in [:cached-outputs]        combine-with set/union #{} (from-supertypes description gt/cached-outputs))
      (update-in [:interfaces]            combine-with set/union #{} (from-supertypes description gt/interfaces))
      (update-in [:protocols]             combine-with set/union #{} (from-supertypes description gt/protocols))
      (update-in [:method-impls]          combine-with merge      {} (from-supertypes description gt/method-impls))
      (update-in [:substitutes]           combine-with merge      {} (from-supertypes description :substitutes))
      (update-in [:cardinalities]         combine-with merge      {} (from-supertypes description :cardinalities))
      (update-in [:cascade-deletes]       combine-with set/union #{} (from-supertypes description :cascade-deletes))
      resolve-display-order
      attach-properties-output
      attach-input-dependencies
      verify-inputs-for-dynamics
      map->NodeTypeImpl))

(def ^:private inputs-properties (juxt :inputs :declared-properties))

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

      (some #{:cascade-delete} flags)
      (update :cascade-deletes #(conj (or % #{}) label))

      (some #{:inject} flags)
      (update :injectable-inputs #(conj (or % #{}) label))

      (:substitute options)
      (update :substitutes assoc label (:substitute options))

      (not (some #{:array} flags))
      (update :cardinalities assoc label :one)

      (some #{:array} flags)
      (update :cardinalities assoc label :many))))

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
      (update-in [:transforms] assoc-in [label] production-fn))))

(def ^:private internal-keys #{:_node-id :_declared-properties :_properties :_output-jammers})

(defn attach-property
  "Update the node type description with the given property."
  [description label property-type]
  (let [property-type (if (contains? internal-keys label) (assoc property-type :internal? true) property-type)]
    (-> description
        (update    :declared-properties     assoc     label  property-type)
        (update-in [:transforms]            assoc-in [label] (ip/getter-for property-type))
        (update-in [:transform-types]       assoc     label  (:value-type property-type))
        (cond->
            (not (internal-keys label))
            (update-in [:property-order-decl] #(conj (or % []) label))))))

(defn attach-extern
  "Update the node type description with the given extern. It will be
  a property as well as an extern."
  [description label property-type]
  (attach-property description label (assoc property-type :unjammable? true)))

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
  (let [arglist     (mapv #(with-meta (gensym) (meta %)) argv)]
    (assoc-in description [:method-impls sym] [arglist fn-def])))

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

(def ^:private input-flags   #{:inject :array :cascade-delete})
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
             `(attach-property ~(keyword label) ~(ip/property-type-descriptor (keyword label) tp options)))

         [(['extern label tp & options] :seq)]
         (do (assert-symbol "extern" label)
             `(attach-extern ~(keyword label) ~(ip/property-type-descriptor (keyword label) tp options)))

         [(['display-order ordering] :seq)]
         `(assoc :display-order-decl ~ordering)

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
  (util/map-vals gt/property-default-value (gt/declared-properties node-type)))

(defn- has-multivalued-input?  [node-type input-label] (= :many (gt/input-cardinality node-type input-label)))
(defn- has-singlevalued-input? [node-type input-label] (= :one (gt/input-cardinality node-type input-label)))

(defn has-input?    [node-type argument] (gt/input-type node-type argument))
(defn has-property? [node-type argument] (gt/property-type node-type argument))
(defn has-output?   [node-type argument] (gt/output-type node-type argument))

(defn- property-overloads-output? [node-type argument output] (and (= output argument)    (has-property? node-type argument)))
(defn- unoverloaded-output?       [node-type argument output] (and (not= output argument) (has-output? node-type argument)))

(defn property-schema
  [node-type property]
  (let [schema (gt/property-type node-type property)]
    (if (satisfies? gt/PropertyType schema) (gt/property-value-type schema) schema)))

(defn- dollar-name
  [node-type-name label]
  (symbol (str node-type-name "$" (name label))))

(defn deduce-argument-type
  "Return the type of the node's input label (or property). Take care
  with :array inputs."
  [record-name node-type argument output]
  (cond
    (property-overloads-output? node-type argument output)
    (s/either (s/maybe (property-schema node-type argument)) ErrorValue)

    (unoverloaded-output? node-type argument output)
    (s/either (s/maybe (gt/output-type node-type argument)) ErrorValue)

    (has-multivalued-input? node-type argument)
    [(s/either (s/maybe (gt/input-type node-type argument)) ErrorValue)]

    (has-singlevalued-input? node-type argument)
    (s/either (s/maybe (gt/input-type node-type argument)) ErrorValue)

    (has-output? node-type argument)
    (s/either (s/maybe (gt/output-type node-type argument)) ErrorValue)

    (= :this argument)
    record-name

    (has-property? node-type argument)
    (s/either (s/maybe (property-schema node-type argument)) ErrorValue)))

(defn collect-argument-schema
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

(defn- first-input-value-form
  [self-name ctx-name input]
  `(let [[node-id# output-label#] (first (gt/sources (:basis ~ctx-name) (gt/node-id ~self-name) ~input))]
     (when-let [node# (and node-id# (ig/node-by-id-at (:basis ~ctx-name) node-id#))]
       (gt/produce-value node# output-label# ~ctx-name))))

(defn- input-value-forms
  [self-name ctx-name input]
  `(mapv (fn [[node-id# output-label#]]
           (let [node# (ig/node-by-id-at (:basis ~ctx-name) node-id#)]
             (gt/produce-value node# output-label# ~ctx-name)))
         (gt/sources (:basis ~ctx-name) (gt/node-id ~self-name) ~input)))

(defn maybe-use-substitute [node-type input forms]
  (if (gt/substitute-for node-type input)
    `(let [input# ~forms]
       (if (ie/error? input#)
         (util/apply-if-fn ~(gt/substitute-for node-type input) input#)
         input#))
    forms))

(defn- node-input-forms
  [self-name ctx-name output node-type [argument schema]]
  (cond
    (property-overloads-output? node-type argument output)
    `(get ~self-name ~argument)

    (unoverloaded-output? node-type argument output)
    `(gt/produce-value ~self-name ~argument ~ctx-name)

    (has-multivalued-input? node-type argument)
    (maybe-use-substitute
     node-type argument
     (input-value-forms self-name ctx-name argument))

    (has-singlevalued-input? node-type argument)
    (maybe-use-substitute
     node-type argument
     (first-input-value-form self-name ctx-name argument))

    (has-output? node-type argument)
    `(gt/produce-value ~self-name ~argument ~ctx-name)

    (= :this argument)
    'this))

(defn check-local-cache [ctx-name nodeid-sym transform]
  `(get-in @(:local ~ctx-name) [~nodeid-sym ~transform]))

(defn check-global-cache [ctx-name nodeid-sym transform]
  `(if-some [cached# (get (:snapshot ~ctx-name) [~nodeid-sym ~transform])]
     (do (swap! (:hits ~ctx-name) conj [~nodeid-sym ~transform]) cached#)))

(defn jam [self-name ctx-name nodeid-sym transform forms]
  `(if-let [jammer# (get (:_output-jammers ~self-name) ~transform)]
     (let [jam-value# (jammer#)]
       (if (ie/error? jam-value#)
         (assoc jam-value# :_label ~transform :_node-id ~nodeid-sym)
         jam-value#))
     ~forms))

(defn detect-cycles [ctx-name nodeid-sym transform node-type-name forms]
  `(do
     (assert (every? #(not= % [~nodeid-sym ~transform]) (:in-production ~ctx-name))
             (format "Cycle Detected on node type %s and output %s" (:name ~node-type-name) ~transform))
     ~forms))

(defn mark-in-production [ctx-name nodeid-sym transform forms]
  `(let [~ctx-name (update ~ctx-name :in-production conj [~nodeid-sym ~transform])]
     ~forms))

(defn check-caches [ctx-name nodeid-sym node-type transform forms]
  (if ((gt/cached-outputs node-type) transform)
    (list `or
          (check-local-cache ctx-name nodeid-sym transform)
          (check-global-cache ctx-name nodeid-sym transform)
          forms)
    forms))

(defn gather-inputs [input-sym schema-sym self-name ctx-name nodeid-sym record-name node-type transform production-function forms]
  (let [arg-names       (util/fnk-schema production-function)
        argument-schema (collect-argument-schema transform arg-names record-name node-type)
        argument-forms  (zipmap (keys argument-schema)
                                (map (partial node-input-forms self-name ctx-name transform node-type) argument-schema))]
    (list `let
          [input-sym argument-forms schema-sym argument-schema]
          forms)))

(defn input-error-check [self-name ctx-name node-type label input-sym tail]
  (let [internal?    (internal-keys label)
        multivalued? (seq? ((gt/transform-types node-type) label))]
    (if internal?
      tail
      `(let [bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals ~input-sym)))]
         (if (empty? bad-errors#)
           (let [~input-sym (util/map-vals ie/use-original-value ~input-sym)]
             ~tail)
           (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~label))))))

(defn schema-check [self-name ctx-name node-type node-type-name transform input-sym schema-sym nodeid-sym forms]
  `(if-let [validation-error# (s/check ~schema-sym ~input-sym)]
     (do
       (warn ~nodeid-sym ~node-type-name ~transform ~schema-sym validation-error#)
       (throw (ex-info "SCHEMA-VALIDATION"
                       {:node-id          ~nodeid-sym
                        :type             ~node-type-name
                        :output           ~transform
                        :expected         ~schema-sym
                        :actual           ~input-sym
                        :validation-error validation-error#})))
     ~forms))

(defn call-production-function [self-name ctx-name node-type node-type-name transform input-sym nodeid-sym]
  (let [result-sym (gensym "result")]
   `(let [production-function# (get (gt/transforms ~node-type-name) ~transform)
          ~result-sym          (production-function# (assoc ~input-sym :_node-id ~nodeid-sym :basis (:basis ~ctx-name)))]
      ~@(when ((gt/cached-outputs node-type) transform)
          `[(swap! (:local ~ctx-name) assoc-in [~nodeid-sym ~transform] ~result-sym)])
      ~result-sym)))

(defn node-output-value-function
  [self-name ctx-name record-name node-type-name node-type transform production-function]
  (let [nodeid-sym     (gensym "node-id")
        input-sym      (gensym "pfn-input")
        schema-sym     (gensym "schema")
        fn-name        (str (dollar-name node-type-name transform))]
    `(defn ~(with-meta (dollar-name node-type-name transform) {:no-doc true}) [~self-name ~ctx-name]
       (let [~nodeid-sym (gt/node-id ~self-name)]
         ~(if (= transform :this)
           nodeid-sym
           (jam self-name ctx-name nodeid-sym transform
             (detect-cycles ctx-name nodeid-sym transform node-type-name
               (mark-in-production ctx-name nodeid-sym transform
                 (check-caches ctx-name nodeid-sym node-type transform
                   (gather-inputs input-sym schema-sym self-name ctx-name nodeid-sym record-name node-type transform production-function
                     (input-error-check self-name ctx-name node-type transform input-sym
                       (schema-check self-name ctx-name node-type node-type-name transform input-sym schema-sym nodeid-sym
                         (call-production-function self-name ctx-name node-type node-type-name transform input-sym nodeid-sym)))))))))))))

(defn node-output-value-function-forms
  [self-name ctx-name record-name node-type-name node-type]
  (for [[transform production-function] (gt/transforms node-type)]
    (node-output-value-function self-name ctx-name record-name node-type-name node-type transform production-function)))

(defn node-input-value-function-forms
  [self-name ctx-name record-name node-type-name node-type]
  (for [[input input-schema] (gt/declared-inputs node-type)]
    (let [funcname (with-meta (dollar-name node-type-name input) {:no-doc true})]
      `(defn ~funcname [~self-name ~ctx-name]
         ~(maybe-use-substitute
           node-type input
           (cond
             (has-multivalued-input? node-type input)
             (input-value-forms self-name ctx-name input)

             (has-singlevalued-input? node-type input)
             (first-input-value-form self-name ctx-name input)))))))

(defn define-node-value-functions
  [record-name node-type-name node-type]
  (eval
   `(do
      ~@(node-input-value-function-forms 'this 'evaluation-context record-name node-type-name node-type)
      ~@(node-output-value-function-forms 'this 'evaluation-context record-name node-type-name node-type))))

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
  (mapv (comp symbol name) (keys (gt/declared-properties node-type))))

(defn- subtract-keys
  [m1 m2]
  (set/difference (set (keys m1)) (set (keys m2))))

(defn node-record-sexps
  [record-name node-type-name node-type]
  `(do
     (defrecord ~record-name ~(state-vector node-type)
       gt/Node
       (node-id        [this#]    (:_node-id this#))
       (node-type      [_]        ~node-type-name)
       (property-types [this]     (gt/public-properties ~node-type-name))
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
        (str "#" '~node-type-name "{" (:_node-id ~node)
             ~@(interpose-every 3 ", "
                                (mapcat (fn [prop] `[~prop " " (pr-str (get ~node ~prop))])
                                        (keys (gt/declared-properties node-type))))
             "}")))))

(defn define-print-method
  "Create a nice print method for a node type. This avoids infinitely recursive output in the REPL."
  [record-name node-type-name node-type]
  (eval (print-method-forms record-name node-type-name node-type)))

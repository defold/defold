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

(declare node-input-forms property-validation-exprs)

(def ^:dynamic *suppress-schema-warnings* false)

(defn warn-input-schema [node-id node-type label value input-schema error]
  (when-not *suppress-schema-warnings*
    (println "Schema validation failed for node " node-id "(" (:name node-type) " ) label " label)
    (println "There were " (count (vals error)) " problems")
    (let [explanation (s/explain input-schema)]
      (doseq [[key val] error]
        (println "Argument " key " which is")
        (pp/pprint value)
        (println "should match")
        (pp/pprint (get explanation key))
        (println "but it failed because ")
        (pp/pprint val)
        (println)))))

(defn warn-output-schema [node-id node-type label value output-schema error]
  (when-not *suppress-schema-warnings*
    (println "Schema validation failed for node " node-id "(" (:name node-type) " ) label " label)
    (println "Output:")
    (println value)
    (println "Should match:")
    (println (s/explain output-schema))))

(defn without [s exclusions] (reduce disj s exclusions))

(defn- all-properties
  [node-type]
  (gt/declared-properties node-type))

(defn- all-labels
  [node-type]
  (set/union (util/key-set (gt/transforms node-type)) (util/key-set (gt/declared-inputs node-type))))

(def ^:private special-labels #{:_declared-properties})

(defn- ordinary-output-labels
  [node-type]
  (without (util/key-set (gt/transforms node-type)) special-labels))

(defn- ordinary-input-labels
  [node-type]
  (without (util/key-set (gt/declared-inputs node-type)) (util/key-set (gt/transforms node-type))))

(defn- ordinary-property-labels
  [node-type]
  (without (util/key-set (gt/declared-properties node-type)) special-labels))

(defn- node-value*
  [node-or-node-id label evaluation-context]
  (let [cache              (:cache evaluation-context)
        basis              (:basis evaluation-context)
        node               (ig/node-by-id-at basis (if (gt/node? node-or-node-id) (gt/node-id node-or-node-id) node-or-node-id))
        result             (and node (gt/produce-value node label evaluation-context))]
    (when (and node cache)
      (let [local             @(:local evaluation-context)
            local-for-encache (for [[node-id vmap] local
                                    [output val] vmap]
                                [[node-id output] val])]
        (c/cache-hit cache @(:hits evaluation-context))
        (c/cache-encache cache local-for-encache)))
    result))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [node-or-node-id label {:keys [cache ^IBasis basis ignore-errors skip-validation] :or {ignore-errors 0} :as options}]
  (let [caching?           (and (not (:no-cache options)) cache)
        evaluation-context {:local           (atom {})
                            :cache           (when caching? cache)
                            :snapshot        (if caching? (c/cache-snapshot cache) {})
                            :hits            (atom [])
                            :basis           basis
                            :in-production   []
                            :ignore-errors   ignore-errors
                            :skip-validation skip-validation
                            :caching?        caching?}]
    (node-value* node-or-node-id label evaluation-context)))

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

(defn setter-for
  [basis node property]
  (ip/setter-for (-> node (gt/node-type basis) gt/declared-properties (get property))))

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

(defn- dummy-produce-declared-properties []
  (assert false "This is a dummy function. You're probably looking for declared-properties-function-forms."))

(defn attach-properties-output
  [node-type-description]
  (let [properties      (util/filterm (comp not #{:_node-id :_output-jammers} key) (:declared-properties node-type-description))
        argument-names  (into (util/key-set properties) (mapcat (comp ip/property-dependencies val) properties))
        argument-schema (zipmap argument-names (repeat s/Any)) ]
    (attach-output
     node-type-description
      :_declared-properties gt/Properties #{} #{}
      (s/schematize-fn dummy-produce-declared-properties (s/=> gt/Properties argument-schema)))))

(defn inputs-for
  [pfn]
  (if (gt/pfnk? pfn)
    (disj (util/fnk-arguments pfn) :this :g)
    #{}))

(defn- warn-missing-arguments
  [node-type-description property-name dynamic-name missing-args]
  (str "Node " (:name node-type-description) " must have inputs or properties for the label(s) "
       missing-args ", because they are needed by its property '" (name property-name) "'."))

(defn- assert-no-missing-args
  [node-type-description property dynamic missing-args]
  (assert (empty? missing-args)
          (warn-missing-arguments node-type-description (first property) (first dynamic) missing-args)))

(defn- all-inputs-and-properties
  [node-type-description]
  (util/key-set
   (merge
    (:inputs node-type-description)
    (:declared-properties node-type-description))))

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
      (update-in [:transforms] assoc-in [label] production-fn)

      true
      (update-in [:passthroughs] #(disj (or % #{}) label)))))

(def ^:private internal-keys #{:_node-id :_declared-properties :_properties :_output-jammers})

(defn attach-property
  "Update the node type description with the given property."
  [description label property-type]
  (let [property-type   (if (contains? internal-keys label) (assoc property-type :internal? true) property-type)
        getter          (if (ip/default-getter? property-type)
                          (eval (ip/default-getter label))
                          (ip/getter-for property-type))]
    (-> description
        (update    :declared-properties     assoc     label  property-type)
        (update-in [:transforms]            assoc-in [label] getter)
        (update-in [:transform-types]       assoc     label  (:value-type property-type))
        (update-in [:passthroughs]          #(conj (or % #{}) label))
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

(defn allow-nil [s]
  (s/maybe s))

(defn allow-error [s]
  (s/either s ErrorValue))

(defn relax-schema [s]
  (allow-error (allow-nil s)))

(defn deduce-argument-type
  "Return the type of the node's input label (or property). Take care
  with :array inputs."
  [record-name node-type argument output]
  (cond
    (property-overloads-output? node-type argument output)
    (relax-schema (property-schema node-type argument))

    (unoverloaded-output? node-type argument output)
    (relax-schema (gt/output-type node-type argument))

    (has-multivalued-input? node-type argument)
    [(relax-schema (gt/input-type node-type argument))]

    (has-singlevalued-input? node-type argument)
    (relax-schema (gt/input-type node-type argument))

    (has-output? node-type argument)
    (relax-schema (gt/output-type node-type argument))

    (= :this argument)
    record-name

    (has-property? node-type argument)
    (relax-schema (property-schema node-type argument))))

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

(defn call-with-error-checked-fnky-arguments
  [self-name ctx-name propmap-sym label node-type-name node-type compile-time-fnk runtime-fnk-expr & [supplied-arguments]]
  (let [arglist          (without (util/fnk-arguments compile-time-fnk) (keys supplied-arguments))
        argument-forms   (zipmap arglist (map #(node-input-forms self-name ctx-name propmap-sym label node-type-name node-type [% nil]) arglist))
        argument-forms   (merge argument-forms supplied-arguments)]
    `(let [arg-forms# ~argument-forms]
       (let [bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals arg-forms#)))]
         (if (empty? bad-errors#)
           (~runtime-fnk-expr arg-forms#)
           (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~label))))))

(defn collect-base-property-value
  [self-name ctx-name node-type-name node-type propmap-sym prop]
  (if (ip/default-getter? (gt/property-type node-type prop))
    `(gt/get-property ~self-name (:basis ~ctx-name) ~prop)
    (call-with-error-checked-fnky-arguments self-name ctx-name propmap-sym prop node-type-name node-type
                                            (ip/getter-for (gt/property-type node-type prop))
                                            `(get-in ~propmap-sym [~prop :internal.property/value]))))

(defn collect-property-value
  [self-name ctx-name node-type-name node-type propmap-sym prop]
  (let [property-definition (gt/property-type node-type prop)
        default?            (ip/default-getter? property-definition)
        validation          (ip/validation property-definition)
        get-expr            (if default?
                              `(get ~self-name ~prop)
                              (call-with-error-checked-fnky-arguments self-name ctx-name propmap-sym prop node-type-name node-type
                                                                      (ip/getter-for (gt/property-type node-type prop))
                                                                      `(get-in ~propmap-sym [~prop :internal.property/value])))
        validate-expr       (property-validation-exprs self-name ctx-name node-type-name node-type propmap-sym prop)]
    (if validation
      `(let [~'v ~get-expr]
         (if (:skip-validation ~ctx-name)
           ~'v
           (if (ie/error? ~'v)
             ~'v
             ~validate-expr)))
      get-expr)))

(defn- node-input-forms
  [self-name ctx-name propmap-sym output node-type-name node-type [argument schema]]
  (cond
    (property-overloads-output? node-type argument output)
    (collect-property-value self-name ctx-name node-type-name node-type propmap-sym argument)

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
    'this

    (has-property? node-type argument)
    (collect-property-value self-name ctx-name node-type-name node-type propmap-sym argument)))

(defn check-local-cache [ctx-name nodeid-sym transform]
  `(get-in @(:local ~ctx-name) [~nodeid-sym ~transform]))

(defn check-global-cache [ctx-name nodeid-sym transform]
  `(if-some [cached# (get (:snapshot ~ctx-name) [~nodeid-sym ~transform])]
     (do (swap! (:hits ~ctx-name) conj [~nodeid-sym ~transform]) cached#)))

(def ^:private jammable? (complement internal-keys))

(defn jam [self-name ctx-name nodeid-sym transform forms]
  (if (jammable? transform)
    `(if-let [jammer# (get (:_output-jammers ~self-name) ~transform)]
       (let [jam-value# (jammer#)]
         (if (ie/error? jam-value#)
           (assoc jam-value# :_label ~transform :_node-id ~nodeid-sym)
           jam-value#))
       ~forms)
    forms))

(defn- property-has-default-getter? [node-type transform] (get (gt/property-type node-type transform) :internal.property/default-getter))
(defn- property-has-no-overriding-output? [node-type transform] (get-in node-type [:passthroughs transform]))
(defn- has-validation? [node-type prop] (ip/validation (get (gt/declared-properties node-type) prop)))

(defn apply-default-property-shortcut [self-name ctx-name transform node-type forms]
  (if (and (property-has-default-getter? node-type transform)
           (property-has-no-overriding-output? node-type transform)
           (not (has-validation? node-type transform)))
    `(get ~self-name ~transform)
    forms))

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

(defn gather-inputs [input-sym schema-sym self-name ctx-name nodeid-sym propmap-sym record-name node-type-name node-type transform production-function forms]
  (let [arg-names       (util/fnk-schema production-function)
        argument-schema (collect-argument-schema transform arg-names record-name node-type)
        argument-forms  (zipmap (keys argument-schema)
                                (map (partial node-input-forms self-name ctx-name propmap-sym transform node-type-name node-type) argument-schema))]
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

(defn schema-check-input [self-name ctx-name node-type node-type-name transform input-sym schema-sym nodeid-sym forms]
  `(if-let [validation-error# (s/check ~schema-sym ~input-sym)]
     (do
       (warn-input-schema ~nodeid-sym ~node-type-name ~transform ~input-sym ~schema-sym validation-error#)
       (throw (ex-info "SCHEMA-VALIDATION"
                       {:node-id          ~nodeid-sym
                        :type             ~node-type-name
                        :output           ~transform
                        :expected         ~schema-sym
                        :actual           ~input-sym
                        :validation-error validation-error#})))
     ~forms))

(defn call-production-function [self-name ctx-name node-type node-type-name transform input-sym nodeid-sym output-sym forms]
  `(let [production-function# (get (gt/transforms ~node-type-name) ~transform)
         ~output-sym          (production-function# (assoc ~input-sym :_node-id ~nodeid-sym :basis (:basis ~ctx-name)))]
     ~forms))

(defn cache-output [ctx-name node-type transform nodeid-sym output-sym forms]
  `(do
     ~@(when ((gt/cached-outputs node-type) transform)
         `[(swap! (:local ~ctx-name) assoc-in [~nodeid-sym ~transform] ~output-sym)])
     ~forms))

(defn deduce-output-type
  [node-type output]
  (cond
    (property-has-no-overriding-output? node-type output)
    (relax-schema (property-schema node-type output))

    :else
    (do (assert (has-output? node-type output) "Unknown output type")
        (relax-schema (gt/output-type node-type output)))))

(defn schema-check-output [self-name ctx-name node-type node-type-name transform nodeid-sym output-sym forms]
  (let [output-schema (deduce-output-type node-type transform)]
    `(if-let [validation-error# (s/check ~output-schema ~output-sym)]
       (do
         (warn-output-schema ~nodeid-sym ~node-type-name ~transform ~output-sym ~output-schema validation-error#)
         (throw (ex-info "SCHEMA-VALIDATION"
                  {:node-id          ~nodeid-sym
                   :type             ~node-type-name
                   :output           ~transform
                   :expected         ~output-schema
                   :actual           ~output-sym
                   :validation-error validation-error#})))
       ~forms)))

(defn validate-output [self-name ctx-name node-type node-type-name transform nodeid-sym propmap-sym output-sym forms]
  (if (and (has-property? node-type transform)
        (property-has-no-overriding-output? node-type transform)
        (has-validation? node-type transform))
    (let [property-definition (gt/property-type node-type transform)
          validation (ip/validation property-definition)
          validate-expr (property-validation-exprs self-name ctx-name node-type-name node-type propmap-sym transform)]
      `(if (or (:skip-validation ~ctx-name) (ie/error? ~output-sym))
         ~forms
         (let [error# ~validate-expr
               bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (if error# [error#] []))]
           (if (empty? bad-errors#)
             ~forms
             (let [~output-sym (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~transform)]
               ~forms)))))
    forms))

(defn node-output-value-function
  [self-name ctx-name record-name node-type-name node-type transform]
  (let [production-function (get (gt/transforms node-type) transform)
        nodeid-sym          (gensym "node-id")
        input-sym           (gensym "pfn-input")
        schema-sym          (gensym "schema")
        propmap-sym         (gensym "propmap")
        output-sym          (gensym "output")
        ovf
    `(defn ~(with-meta (dollar-name node-type-name transform) {:no-doc true}) [~self-name ~ctx-name]
       (let [~nodeid-sym (gt/node-id ~self-name)
             ~propmap-sym (gt/declared-properties ~node-type-name)]
         ~(if (= transform :this)
            nodeid-sym
            (jam self-name ctx-name nodeid-sym transform
              (apply-default-property-shortcut self-name ctx-name transform node-type
                (detect-cycles ctx-name nodeid-sym transform node-type-name
                  (mark-in-production ctx-name nodeid-sym transform
                    (check-caches ctx-name nodeid-sym node-type transform
                      (gather-inputs input-sym schema-sym self-name ctx-name nodeid-sym propmap-sym record-name node-type-name node-type transform production-function
                        (input-error-check self-name ctx-name node-type transform input-sym
                          (schema-check-input self-name ctx-name node-type node-type-name transform input-sym schema-sym nodeid-sym
                            (call-production-function self-name ctx-name node-type node-type-name transform input-sym nodeid-sym output-sym
                              (schema-check-output self-name ctx-name node-type node-type-name transform nodeid-sym output-sym
                                (validate-output self-name ctx-name node-type node-type-name transform nodeid-sym propmap-sym output-sym
                                  (cache-output ctx-name node-type transform nodeid-sym output-sym
                                    output-sym)))))))))))))))]
    ovf))

(defn node-output-value-function-forms
  [self-name ctx-name record-name node-type-name node-type]
  (for [transform (ordinary-output-labels node-type)]
    (node-output-value-function self-name ctx-name record-name node-type-name node-type transform)))

(defn- has-dynamics? [ptype] (not (nil? (gt/dynamic-attributes ptype))))

(defn attach-property-dynamics
  [self-name ctx-name node-type-name node-type propmap-sym property-name property-type value-form]
  (let [dynsym (gensym "dynamics")]
    `(let [~dynsym (gt/dynamic-attributes (get ~propmap-sym ~property-name))]
       ~(reduce (fn [m [d dfnk]]
                  (assoc m d (call-with-error-checked-fnky-arguments self-name ctx-name propmap-sym d node-type-name node-type
                                                                     dfnk
                                                                     `(get ~dynsym ~d))))
                value-form
                (gt/dynamic-attributes property-type)))))

(defn collect-property-values
  [self-name ctx-name propmap-sym record-name node-type-name node-type nodeid-sym value-sym forms]
  (let [props        (gt/declared-properties node-type)
        fetch-exprs  (partial collect-base-property-value self-name ctx-name node-type-name node-type propmap-sym)
        add-dynamics (partial attach-property-dynamics self-name ctx-name node-type-name node-type propmap-sym)]
    `(let [~value-sym ~(apply hash-map
                              (mapcat identity
                                      (for [[p ptype] props
                                            :when (not (:internal? ptype))]
                                        (let [basic-val `{:type    (get ~propmap-sym ~p)
                                                          :value   ~(fetch-exprs p)
                                                          :node-id ~nodeid-sym}]
                                          (if (has-dynamics? ptype)
                                            [p (add-dynamics p ptype basic-val)]
                                            [p basic-val])))))]
       ~forms)))

(defn- create-validate-argument-form
  [self-name ctx-name propmap-sym node-type-name node-type argument]
  ;; This is  similar to node-input-forms, but simpler as we don't have to deal with the case where we're calling
  ;; an output function refering to an argument property with the same name.
  (cond
    (and (has-property? node-type argument) (property-has-no-overriding-output? node-type argument))
    (collect-base-property-value self-name ctx-name node-type-name node-type propmap-sym argument)

    (has-output? node-type argument)
    `(gt/produce-value ~self-name ~argument ~ctx-name)

    (has-multivalued-input? node-type argument)
    (maybe-use-substitute
     node-type argument
     (input-value-forms self-name ctx-name argument))

    (has-singlevalued-input? node-type argument)
    (maybe-use-substitute
     node-type argument
     (first-input-value-form self-name ctx-name argument))

    (= :this argument)
    'this

    :else (assert false (str "unknown argument " argument " in call to validate function"))))

(defn property-validation-exprs
  [self-name ctx-name node-type-name node-type propmap-sym prop & [supplied-arguments]]
  (when (has-validation? node-type prop)
    (let [compile-time-validation-fnk (ip/validation (gt/property-type node-type prop))
          arglist (without (util/fnk-arguments compile-time-validation-fnk) (keys supplied-arguments))
          argument-forms (zipmap arglist (map #(create-validate-argument-form self-name ctx-name propmap-sym node-type-name node-type % ) arglist))
          argument-forms (merge argument-forms supplied-arguments)]
      `(let [arg-forms# ~argument-forms
             bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals arg-forms#)))]
         (if (empty? bad-errors#)
           ((get-in ~propmap-sym [~prop :internal.property/validate]) arg-forms#)
           (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~prop)))))) ; TODO: decorate with :production :validate?

(defn collect-validation-problems
  [self-name ctx-name propmap-sym record-name node-type-name node-type value-map validation-map forms]
  (let [props-with-validation (util/map-vals ip/validation (gt/declared-properties node-type))
        validation-exprs      (partial property-validation-exprs self-name ctx-name node-type-name node-type propmap-sym)]
    `(let [~validation-map ~(apply hash-map
                                   (mapcat identity
                                           (for [[p validator] props-with-validation
                                                 :when validator]
                                             [p (validation-exprs p)])))]
       ~forms)))

(defn merge-problems
  [value-map validation-map]
  (let [validation-map (into {} (filter (comp not nil? second) validation-map))]
    (let [merger (fn [value problem]
                   (let [original-value (:value value)
                         problem (assoc problem :value original-value)]
                     (assoc value :validation-problems problem :value problem)))]
      (merge-with merger value-map validation-map))))

(defn merge-values-and-validation-problems
  [value-sym validation-sym forms]
  `(let [~value-sym (merge-problems ~value-sym ~validation-sym)]
     ~forms))

(defn collect-display-order
  [self-name ctx-name record-name node-type-name node-type display-order-sym forms]
  `(let [~display-order-sym (gt/property-display-order ~node-type-name)]
     ~forms))

(defn- assemble-properties-map
  [value-sym display-sym]
  `(hash-map :properties    ~value-sym
             :display-order ~display-sym))

(defn declared-properties-function-forms
  [self-name ctx-name record-name node-type-name node-type]
  (let [fn-name        (dollar-name node-type-name :_declared-properties)
        kwarg-sym      (gensym "kwargs")
        value-map      (gensym "values")
        validation-map (gensym "problems")
        nodeid-sym     (gensym "_node-id")
        display-order  (gensym "display-order")
        propmap-sym    (gensym "propmap")]
    `(defn ~(with-meta fn-name {:no-doc true}) [~self-name ~ctx-name]
       (let [~nodeid-sym (gt/node-id ~self-name)
             ~propmap-sym (gt/declared-properties ~node-type-name)]
         ~(collect-property-values self-name ctx-name propmap-sym record-name node-type-name node-type nodeid-sym value-map
            (collect-validation-problems self-name ctx-name propmap-sym record-name node-type-name node-type value-map validation-map
              (merge-values-and-validation-problems value-map validation-map
                (collect-display-order self-name ctx-name record-name node-type-name node-type display-order
                  (assemble-properties-map value-map display-order)))))))))

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
      ~(declared-properties-function-forms 'this 'evaluation-context record-name node-type-name node-type)
      ~@(node-input-value-function-forms 'this 'evaluation-context record-name node-type-name node-type)
      ~@(node-output-value-function-forms 'this 'evaluation-context record-name node-type-name node-type))))

(defn node-value-function-names
  [node-type-name node-type]
  (map (partial dollar-name node-type-name) (all-labels node-type)))

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

(defn- ordinary-output-case
  [self-name ctx-name node-type-name an-output]
  [an-output (list (dollar-name node-type-name an-output) self-name ctx-name)])

(defn- ordinary-input-case
  [self-name ctx-name node-type-name an-input]
  [an-input  (list (dollar-name node-type-name an-input) self-name ctx-name)])

(defn node-record-sexps
  [record-name node-type-name node-type]
  `(do
     (defrecord ~record-name ~(state-vector node-type)
       gt/Node
       (node-id        [this#]    (:_node-id this#))
       (node-type      [_ _]        ~node-type-name)
       (property-types [_ _]     (gt/public-properties ~node-type-name))
       (get-property   [~'this basis# property#]
         (get ~'this property#))
       (set-property   [~'this basis# property# value#]
         (let [type# (gt/node-type ~'this basis#)]
           (assert (contains? (gt/property-labels type#) property#)
                   (format "Attempting to use property %s from %s, but it does not exist" property# (:name type#))))
         (assoc ~'this property# value#))
       (clear-property [~'this basis# property#]
         (throw (ex-info (str "Not possible to clear property " property# " of node type " (:name ~node-type-name) " since the node is not an override")
                         {:label property# :node-type ~node-type-name})))
       (produce-value  [~'this label# ~'evaluation-context]
         (case label#
           :_declared-properties (~(dollar-name node-type-name :_declared-properties) ~'this ~'evaluation-context)
           ~@(mapcat (partial ordinary-output-case 'this 'evaluation-context node-type-name) (ordinary-output-labels node-type))
           ~@(mapcat (partial ordinary-input-case 'this 'evaluation-context node-type-name)  (ordinary-input-labels node-type))
           (throw (ex-info (str "No such output, input, or property " label# " exists for node type " (:name ~node-type-name))
                           {:label label# :node-type ~node-type-name}))))
       (original [this] nil)
       (override-id [this] nil)
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

(defn- output-fn [type output]
  (eval (dollar-name (:name type) output)))

(defrecord OverrideNode [override-id node-id original-id properties]
  gt/Node
  (node-id             [this] node-id)
  (node-type           [this basis] (gt/node-type (ig/node-by-id-at basis original-id) basis))
  (property-types      [this basis] (gt/public-properties (gt/node-type this basis)))
  (get-property        [this basis property] (get properties property))
  (set-property        [this basis property value] (if (= :_output-jammers property)
                                                     (throw (ex-info "Not possible to mark override nodes as defective" {}))
                                                     (assoc-in this [:properties property] value)))
  (clear-property      [this basis property] (update this :properties dissoc property))
  (produce-value       [this output evaluation-context]
    (let [basis (:basis evaluation-context)
          original (ig/node-by-id-at basis original-id)
          type (gt/node-type original basis)]
      (cond
        (= :_node-id output) node-id
        (or (= :_declared-properties output)
            (= :_properties output)) (let [props ((output-fn type output) this evaluation-context)
                                           original-props ((output-fn type output) original evaluation-context)]
                                       (let [res (reduce (fn [props [key p]]
                                                           (let [overridden? (if (contains? p :path)
                                                                               (not= nil (get-in properties (:path p)))
                                                                               (contains? properties key))]
                                                             (if overridden?
                                                               (-> props
                                                                 (update-in [:properties key] assoc :original-value (get-in props [:properties key :value]))
                                                                 (update-in [:properties key] assoc :value (:value p)))
                                                               props)))
                                                         original-props (:properties props))
                                             res (assoc res :properties (into {} (map (fn [[key value]] [key (assoc value :node-id node-id)]) (:properties res))))]
                                         res))
        ((gt/property-labels type) output) (if (ip/default-getter? (gt/property-type type output))
                                             (if (contains? properties output)
                                               (get properties output)
                                               (node-value* original output evaluation-context))
                                             ((output-fn type output) this evaluation-context))
        ((gt/output-labels type) output) ((output-fn type output) this evaluation-context)
        ((gt/input-labels type) output) ((output-fn type output) this evaluation-context)
        true (let [dyn-properties (node-value* original :_properties evaluation-context)]
               (if (contains? (:properties dyn-properties) output)
                 (get properties output)
                 (node-value* original output evaluation-context))))))
  (override-id [this] override-id)
  (original [this] original-id))

(defn make-override-node [override-id node-id original-id properties]
  (->OverrideNode override-id node-id original-id properties))

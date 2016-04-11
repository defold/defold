(ns internal.node
  (:require [clojure.pprint :as pp]
            [clojure.set :as set]
            [clojure.string :as str]
            [internal.util :as util]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [internal.property :as ip]
            [plumbing.core :as pc]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s])
  (:import [internal.graph.types IBasis]
           [internal.graph.error_values ErrorValue]
           [clojure.lang Named]))

(defmacro gensyms
  [[:as syms] & forms]
  (let [bindings (vec (interleave syms (map (fn [s] `(gensym ~(name s))) syms)))]
    `(let ~bindings
       ~@forms)))

(set! *warn-on-reflection* true)

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
  (set/union (util/key-set (:transforms node-type)) (util/key-set (:inputs node-type))))

(def ^:private special-labels #{:_declared-properties})

(defn ordinary-output-labels
  [description]
  (without (util/key-set (:transforms description)) special-labels))

(defn ordinary-input-labels
  [description]
  (without (util/key-set (:inputs description)) (util/key-set (:transforms description))))

(defn- ordinary-property-labels
  [node-type]
  (without (util/key-set (gt/declared-properties node-type)) special-labels))

(defn- node-value*
  [node-or-node-id label evaluation-context]
  (let [cache              (:cache evaluation-context)
        basis              (:basis evaluation-context)
        node               (if (gt/node-id? node-or-node-id) (ig/node-by-id-at basis node-or-node-id) node-or-node-id)
        result             (and node (gt/produce-value (gt/node-type node) node label evaluation-context))]
    (when (and node cache)
      (let [local             @(:local evaluation-context)
            local-for-encache (for [[node-id vmap] local
                                    [output val] vmap]
                                [[node-id output] val])]
        (c/cache-hit cache @(:hits evaluation-context))
        (c/cache-encache cache local-for-encache)))
    result))

(defn make-evaluation-context
  [cache basis ignore-errors skip-validation caching?]
  {:local           (atom {})
   :cache           (when caching? cache)
   :snapshot        (if caching? (c/cache-snapshot cache) {})
   :hits            (atom [])
   :basis           basis
   :in-production   []
   :ignore-errors   ignore-errors
   :skip-validation skip-validation
   :caching?        caching?})

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [node-or-node-id label {:keys [cache ^IBasis basis ignore-errors skip-validation] :or {ignore-errors 0} :as options}]
  (let [caching?           (and (not (:no-cache options)) cache)
        evaluation-context (make-evaluation-context cache basis ignore-errors skip-validation caching?)]
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
    [name supertypes outputs transforms transform-types declared-properties passthroughs inputs injectable-inputs cached-outputs input-dependencies substitutes cardinalities cascade-deletes property-display-order behaviors]

  gt/NodeType
  (supertypes            [_] supertypes)
  (transforms            [_] transforms)
  (transform-types       [_] transform-types)
  (declared-properties   [_] declared-properties)
  (declared-inputs       [_] inputs)
  (injectable-inputs     [_] injectable-inputs)
  (declared-outputs      [_] outputs)
  (cached-outputs        [_] cached-outputs)
  (input-dependencies    [_] input-dependencies)
  (substitute-for        [_ input] (get substitutes input))
  (input-type            [_ input] (get inputs input))
  (input-cardinality     [_ input] (get cardinalities input))
  (cascade-deletes       [_]        cascade-deletes)
  (output-type           [_ output] (get transform-types output))
  (passthroughs          [_] passthroughs)
  (property-display-order [this] property-display-order)

  (produce-value          [_ node label evaluation-context]
    (let [behavior (get behaviors label)]
      (assert behavior (str "No such output, input, or property " label " exists for node " (gt/node-id node) " of type " name))
      (behavior node evaluation-context)))

  Named
  (getNamespace [_] (str (:ns (meta (resolve (symbol name))))))
  (getName [_] (str (:name (meta (resolve (symbol name)))))))

(defmethod print-method NodeTypeImpl
  [^NodeTypeImpl v ^java.io.Writer w]
  (.write w (str "<NodeTypeImpl{:name " (:name v) ", :supertypes " (mapv :name (:supertypes v)) "}>")))

(defn- from-supertypes [local op]                (map (comp op util/vgr) (:supertypes local)))
(defn- combine-with    [local op zero into-coll] (op (reduce op zero into-coll) local))

(defn resolve-display-order
  [{:keys [display-order-decl property-order-decl] :as description} supertype-display-orders]
  (-> description
      (assoc :property-display-order (apply merge-display-order display-order-decl property-order-decl supertype-display-orders))
      (dissoc :display-order-decl :property-order-decl)))

(defn inputs-needed [x]
  (cond
    (gt/pfnk? x)       (util/fnk-arguments x)
    (util/property? x) (ip/property-dependencies x)
    (list? x)          (into #{} (map keyword (second x)))))

(declare attach-output)

(defn- dummy-produce-declared-properties []
  (assert false "This is a dummy function. You're probably looking for declared-properties-function-forms."))

;; TODO - fix this gross bit
(defn attach-properties-output
  [node-type-description]
  (let [properties      (util/filterm (comp not #{:_node-id :_output-jammers} key) (:declared-properties node-type-description))
        argument-names  (into (util/key-set properties) (mapcat inputs-needed properties))
        argument-schema (zipmap argument-names (repeat s/Any))]
    (println "attach-properties-output argument-schema " argument-schema)
    (attach-output
     node-type-description
     '_declared-properties 'internal.graph.types/Properties #{} #{}
     [`(g/fnk [] (assert false "This is a dummy function. You're probably looking for declared-properties-function-forms."))])))

(defn inputs-for
  [pfn]
  (if (gt/pfnk? pfn)
    (disj (util/fnk-arguments pfn) :this :basis)
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

(defn- all-inputs-and-properties-and-outputs
  [node-type-description]
  (set/union
    (all-inputs-and-properties node-type-description)
    (util/key-set
      (:outputs node-type-description))))

(defn missing-inputs-for-dynamic
  [node-type-description dynamic]
  (let [fnk           (second dynamic)
        required-args (inputs-for fnk)]
    (set/difference required-args
                    (all-inputs-and-properties node-type-description))))

(defn verify-inputs-for-dynamics
  [node-type-description]
  (doseq [property (:declared-properties node-type-description)
          dynamic  (ip/dynamic-attributes (second property))
          :let     [missing-args (missing-inputs-for-dynamic node-type-description dynamic)]]
    (assert-no-missing-args node-type-description property dynamic missing-args))
  node-type-description)

(defn verify-inputs-for-outputs
  [node-type-description]
  (doseq [[output fnk] #_(select-keys (:transforms node-type-description)
                                     (keys (:outputs node-type-description)))
          (:transforms node-type-description)
          :let [required-args (inputs-for fnk)
                missing-args (set/difference required-args
                                             (all-inputs-and-properties-and-outputs node-type-description))]]
    (assert (empty? missing-args)
            (str "Node " (:name node-type-description) " must have inputs, properties or outputs for the label(s) "
                 missing-args ", because they are needed by the output '" (name output) "'.")))
  node-type-description)

(defn verify-labels
  [node-type-description]
  (let [inputs     (set (keys (:inputs node-type-description)))
        properties (set (keys (:declared-properties node-type-description)))
        collisions (filter inputs properties)]
    (assert (empty? collisions) (str "inputs and properties can not be overloaded (problematic fields: " (str/join "," (map #(str "'" (name %) "'") collisions)) ")")))
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
  [{:keys [transforms] :as description}]
   (let [transforms (zipmap (keys transforms)
                              (map #(dependency-seq description (:input-schema %)) (vals transforms)))]
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
)

(declare node-output-value-function)
(declare declared-properties-function)
(declare node-input-value-function)

(defn transform-outputs-plumbing-map [description]
  (let [labels  (ordinary-output-labels description)]
    (zipmap labels
     (map (fn [label] (node-output-value-function description label)) labels))))

(defn attach-output-behaviors
  [description]
  (update description :behaviors merge (transform-outputs-plumbing-map description)))

(defn attach-declared-properties-behavior
  [description]
  (update description :behaviors merge
          {:_declared-properties
           (declared-properties-function description)}))

(defn transform-inputs-plumbing-map [description]
  (let [labels  (ordinary-input-labels description)]
    (zipmap labels
            (map (fn [input] (node-input-value-function description input)) labels))))

(defn attach-input-behaviors
  [description]
  (update description :behaviors merge (transform-inputs-plumbing-map description)))

(defn make-node-type-map
  "Create a node type object from a maplike description of the node.
  This is really meant to be used during macro expansion of `defnode`,
  not called directly."
  [description]
  (let [supertype-values (map util/vgr (:supertypes description))]
    (-> description
        (resolve-display-order (map gt/property-display-order supertype-values))
        attach-output-behaviors
        attach-declared-properties-behavior
        attach-input-behaviors
        attach-properties-output
        attach-input-dependencies
        verify-labels
        verify-inputs-for-dynamics
        verify-inputs-for-outputs
        )))

(defn lookup-from
  [node-sym node-type field]
  (reduce-kv
   (fn [m label v] (assoc m label {:fn `(get-in ~node-sym [~field ~label :fn])
                                  :output-type `(get-in ~node-sym [~field ~label :output-type])
                                  :input-schema `(get-in ~node-sym [~field ~label :input-schema])}))
   {}
   (get node-type field)))

(defn flatten-supertype
  [base supertype]
  (let [superval (util/vgr supertype)]
    (-> base
        (update :supertypes           util/conjv supertype)
        (update :transforms           merge      (lookup-from supertype superval :transforms))
        (update :inputs               merge      (:inputs     superval))
        (update :injectable-inputs    set/union  (:injectable-inputs   superval))
        (update :declared-properties  merge      (:declared-properties superval))
        (update :passthroughs         set/union  (:passthroughs        superval))
        (update :outputs              merge      (:declared-outputs    superval))
        (update :transform-types      merge      (:transform-types     superval))
        (update :cached-outputs       set/union  (:cached-outputs      superval))
        (update :substitutes          merge      (:substitutes         superval))
        (update :cardinalities        merge      (:cardinalities       superval))
        (update :cascade-deletes      set/union  (:cascade-deletes     superval)))))

(defn flatten-supertypes
  [supertypes]
  (reduce flatten-supertype {} supertypes))

(def ^:private inputs-properties (juxt :inputs :declared-properties))

(def assert-symbol (partial util/assert-form-kind "defnode" "symbol" symbol?))

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
  (assert-symbol "inherits" supertype)
  (assoc description :supertypes (conj (:supertypes description []) supertype)))

(defn attach-input
  "Update the node type description with the given input."
  [description label schema flags options & [args]]
  (assert-symbol "input" label)
  (ip/assert-schema "defnode" "input" schema)
  (let [label           (keyword label)
        resolved-schema (util/resolve-schema schema)]
    (assert (name-available description label) (str "Cannot create input " label ". The id is already in use."))
    (assert (not (gt/protocol? resolved-schema))
            (format "Input %s on node type %s looks like its type is a protocol. Wrap it with (dynamo.graph/protocol) instead" label (:name description)))
    (let [schema (if (util/property? resolved-schema) (ip/property-value-type resolved-schema) resolved-schema)]
      (cond->
          (assoc-in description [:inputs label] schema)

        (some #{:cascade-delete} flags)
        (update :cascade-deletes #(conj (or % #{}) label))

        (some #{:inject} flags)
        (update :injectable-inputs #(conj (or % #{}) label))

        (:substitute options)
        (update :substitutes assoc label (:substitute options))

        (not (some #{:array} flags))
        (update :cardinalities assoc label :one)

        (some #{:array} flags)
        (update :cardinalities assoc label :many)))))

(defn- abstract-function
  [label type]
  (pc/fnk [this]
          (throw (AssertionError.
                  (format "Node %d does not supply a production function for the abstract '%s' output. Add (output %s %s your-function) to the definition of %s"
                          (gt/node-id this) label
                          label type this)))))

(defn attach-output
  "Update the node type description with the given output."
  [description label schema properties options remainder]
  (assert-symbol "output" label)
  (ip/assert-schema "defnode" "output" schema)
  (assert (or (:abstract properties) (not (empty? remainder)))
          (format "The output %s is missing a production function. Either define the production function or mark it as :abstract." label))
  (let [label           (keyword label)
        production-fn   (if (:abstract properties)
                          (abstract-function label schema)
                          (first remainder))
        input-schema    (inputs-needed production-fn)
        resolved-schema (util/resolve-schema schema)
        schema          (if (util/property? resolved-schema) (ip/property-value-type resolved-schema) schema)]
    #_(when-not (:abstract properties)
      (assert-pfnk label production-fn))
    (assert
      (empty? (rest remainder))
      (format "Options and flags for output %s must go before the production function." label))
    (-> description
      (update-in [:transform-types] assoc label schema)
      (update-in [:passthroughs] #(disj (or % #{}) label))
      (update-in [:transforms label] assoc-in [:fn] production-fn)
      (update-in [:transforms label] assoc-in [:output-type] schema)
      (update-in [:transforms label] assoc-in [:input-schema] input-schema)
      (update-in [:outputs] assoc-in [label] schema)
      (cond->

        (:cached properties)
        (update-in [:cached-outputs] #(conj (or % #{}) label))))))

(def ^:private internal-keys #{:_node-id :_declared-properties :_properties :_output-jammers})

(defn attach-property
  "Update the node type description with the given property."
  [description label property-type]
  (assert-symbol "property" label)
  (let [sym-label     label
        label         (keyword label)
        prop-label    (keyword (str "_prop_" sym-label))
        property-type (if (contains? internal-keys label) (assoc property-type :internal? true) property-type)
        getter        (or (ip/getter-for property-type)
                          `(pc/fnk [~'this ~sym-label] (get ~'this ~label)))
        input-schema  (inputs-needed getter)]
    (-> description
        (update    :declared-properties     assoc     label  property-type)
        (update-in [:transforms label] assoc-in [:fn] getter)
        (update-in [:transforms label] assoc-in [:output-type] property-type)
        (update-in [:transforms label] assoc-in [:input-schema] input-schema)
        (update-in [:transform-types]  assoc    label  (:value-type property-type))
        (cond->
            (not (internal-keys label))
            (update-in [:property-order-decl] #(conj (or % []) label))

            (and (nil? (ip/validation property-type))
                 (nil? getter))
            (update-in [:passthroughs] #(conj (or % #{}) label))))))

(defn attach-extern
  "Update the node type description with the given extern. It will be
  a property as well as an extern."
  [description label property-type]
  (assert-symbol "extern" label)
  (attach-property description label (assoc property-type :unjammable? true)))

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
  [description form]
  (case (first form)
    inherits
    (let [[_ supertype] form]
      (attach-supertype description supertype))

    input
    (let [[_ label schema & remainder] form
          [properties options args]    (parse-flags-and-options input-flags input-options remainder)]
      (attach-input description label schema properties options args))

    output
    (let [[_ label schema & remainder] form
          [properties options args]    (parse-flags-and-options output-flags output-options remainder)]
      (attach-output description label schema properties options args))

    property
    (let [[_ label tp & options] form]
      (attach-property description label (ip/property-type-descriptor (keyword label) tp options)))

    extern
    (let [[_ label tp & options] form]
      (attach-extern description label (ip/property-type-descriptor (keyword label) tp options)))

    display-order
    (assoc description :display-order-decl (second form))))

(defn node-type-forms
  "Given all the forms in a defnode macro, emit the forms that will build the node type description."
  [symb forms]
  (let [inherits-clauses (filter #(=    'inherits (first %)) forms)
        other-clauses    (filter #(not= 'inherits (first %)) forms)
        base             (assoc (flatten-supertypes (map second inherits-clauses)) :name (str symb))]
    (reduce node-type-form base other-clauses)))

(defn defaults
  "Return a map of default values for the node type."
  [node-type]
  (util/map-vals ip/property-default-value (gt/declared-properties node-type)))

(defn has-multivalued-input?  [node-type input-label] (= :many (get-in node-type [:cardinalities input-label])))
(defn has-singlevalued-input? [node-type input-label] (= :one (get-in node-type [:cardinalities input-label])))

(defn has-input?     [node-type argument] (contains? (:inputs node-type) argument))
(defn has-property?  [node-type argument] (contains? (:declared-properties node-type) argument))
(defn has-output?    [node-type argument] (contains? (:transforms node-type) argument))
(defn has-declared-output? [node-type argument] (contains? (:outputs node-type) argument))

(defn property-overloads-output? [node-type argument output] (and (= output argument) (has-property? node-type argument) (has-declared-output? node-type argument)))
(defn unoverloaded-output?       [node-type argument output] (and (not= output argument) (has-declared-output? node-type argument)))

(defn property-type [description property]
  (get-in description [:declared-properties property]))

(defn property-schema
  [node-type property]
  (let [schema (property-type node-type property)]
    (if (util/property? schema) (ip/property-value-type schema) schema)))


(defn allow-nil [s]
  `(s/maybe ~s))

(defn allow-error [s]
  `(s/either ~s ErrorValue))

(defn relax-schema [s]
  (allow-error (allow-nil s)))

(defn deduce-argument-type
  "Return the type of the node's input label (or property). Take care
  with :array inputs."
  [node-type argument output]
  (cond
    (= :this argument)
    s/Any

    (property-overloads-output? node-type argument output)
    (relax-schema (property-schema node-type argument))

    (unoverloaded-output? node-type argument output)
    (relax-schema (get (:transform-types node-type) argument))

    (has-property? node-type argument)
    (relax-schema (property-schema node-type argument))

    (has-multivalued-input? node-type argument)
    [(relax-schema (get (:inputs node-type) argument))]

    (has-singlevalued-input? node-type argument)
    (do
      (println "deduce-argument-type " argument " has " (get (:inputs node-type) argument))
      (relax-schema (get (:inputs node-type) argument)))

    (has-declared-output? node-type argument)
    (relax-schema (get (:transform-types node-type) argument))))

(defn collect-argument-schema
  "Return a schema with the production function's input names mapped to the node's corresponding input type."
  [transform argument-schema node-type]
  (persistent!
   (reduce
    (fn [arguments desired-argument-name]
      (if (= s/Keyword desired-argument-name)
        arguments
        (let [argument-type (deduce-argument-type node-type desired-argument-name transform)]
          (assoc! arguments desired-argument-name (or argument-type s/Any)))))
    (transient {})
    argument-schema)))

(defn first-input-value-form
  [self-name ctx-name nodeid-sym input]
  `(let [[upstream-node-id# output-label#] (first (gt/sources (:basis ~ctx-name) ~nodeid-sym ~input))]
     (when-let [upstream-node# (and upstream-node-id# (ig/node-by-id-at (:basis ~ctx-name) upstream-node-id#))]
       (gt/produce-value (gt/node-type upstream-node# (:basis ~ctx-name)) upstream-node# output-label# ~ctx-name))))

(defn input-value-forms
  [self-name ctx-name nodeid-sym input]
  `(mapv (fn [[upstream-node-id# output-label#]]
           (let [upstream-node# (ig/node-by-id-at (:basis ~ctx-name) upstream-node-id#)]
             (gt/produce-value (gt/node-type upstream-node# (:basis ~ctx-name)) upstream-node# output-label# ~ctx-name)))
         (gt/sources (:basis ~ctx-name) (gt/node-id ~self-name) ~input)))

(defn maybe-use-substitute [node-type input forms]
  (if (and (:substitutes node-type) (get-in node-type [:substitutes input]))
   (do
    `(let [input# ~forms]
       (if (ie/error? input#)
         (util/apply-if-fn ~(get-in node-type [:substitues input]) input#)
         input#)))
    forms))

(defn call-with-error-checked-fnky-arguments
  [self-name ctx-name nodeid-sym propmap-sym label node-type compile-time-fnk runtime-fnk-expr & [supplied-arguments]]
  (let [base-args      {:_node-id `(gt/node-id ~self-name) :basis `(:basis ~ctx-name)}
        arglist        (without (inputs-needed compile-time-fnk) (keys supplied-arguments))
        argument-forms (zipmap arglist (map #(get base-args % (if (= label %)
                                                                `(gt/get-property ~self-name (:basis ~ctx-name) ~label)
                                                                (node-input-forms self-name ctx-name nodeid-sym propmap-sym label node-type [% nil])))
                                            arglist))
        argument-forms (merge argument-forms supplied-arguments)]
    `(let [arg-forms# ~argument-forms
           bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals arg-forms#)))]
       (if (empty? bad-errors#)
         (~runtime-fnk-expr arg-forms#)
         (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~label)))))

(defn collect-base-property-value
  [self-name ctx-name nodeid-sym node-type propmap-sym prop]
  (println "collect-base-property-value " prop " getter " (ip/getter-for (property-type node-type prop)))
  (if (ip/default-getter? (property-type node-type prop))
    `(gt/get-property ~self-name (:basis ~ctx-name) ~prop)
    (call-with-error-checked-fnky-arguments self-name ctx-name nodeid-sym propmap-sym prop node-type
                                            (ip/getter-for (property-type node-type prop))
                                            `(get-in ~propmap-sym [~prop :internal.property/value]))))

(defn collect-property-value
  [self-name ctx-name nodeid-sym node-type propmap-sym prop]
  (let [property-definition (property-type node-type prop)
        default?            (ip/default-getter? property-definition)
        validation          (ip/validation property-definition)
        get-expr            (if default?
                              (gt/get-property self-name (:basis ctx-name) prop)
                              (call-with-error-checked-fnky-arguments self-name ctx-name nodeid-sym propmap-sym prop node-type
                                                                      (ip/getter-for (property-type node-type prop))
                                                                      (get-in propmap-sym [prop :internal.property/value])))
        validate-expr       (property-validation-exprs self-name ctx-name nodeid-sym node-type propmap-sym prop)]
    (if validation
      (let [v get-expr]
         (if (:skip-validation ~ctx-name)
           v
           (if (ie/error? v)
             v
             (let [valid-v validate-expr]
               (if (nil? valid-v) v valid-v)))))
      get-expr)))

(defn node-input-forms
  [self-name ctx-name nodeid-sym propmap-sym output node-type [argument _]]
  (cond
    (= :this argument)
    self-name

    (= :basis argument)
    `(:basis ~ctx-name)

    (property-overloads-output? node-type argument output)
    (collect-property-value self-name ctx-name nodeid-sym node-type propmap-sym argument)

    (unoverloaded-output? node-type argument output)
    (gt/produce-value (gt/node-type self-name (:basis ctx-name)) self-name argument ctx-name)

    (has-property? node-type argument)
    (if (= output argument)
      (gt/get-property ~self-name (:basis ctx-name) argument)
      (collect-property-value self-name ctx-name nodeid-sym node-type propmap-sym argument))

    (has-multivalued-input? node-type argument)
    (maybe-use-substitute
      node-type argument
      (input-value-forms self-name ctx-name nodeid-sym argument))

    (has-singlevalued-input? node-type argument)
    (maybe-use-substitute
      node-type argument
      (first-input-value-form self-name ctx-name nodeid-sym argument))

    (has-declared-output? node-type argument)
    (gt/produce-value (gt/node-type self-name (:basis ctx-name)) self-name argument ctx-name)))

(defn check-local-cache [ctx-name nodeid-sym transform]
  `(get-in @(:local ~ctx-name) [~nodeid-sym ~transform]))

(defn check-global-cache [ctx-name nodeid-sym transform]
  `(if-some [cached# (get (:snapshot ~ctx-name) [~nodeid-sym ~transform])]
     (do (swap! (:hits ~ctx-name) conj [~nodeid-sym ~transform]) cached#)))

(def ^:private jammable? (complement internal-keys))

(defn original-root [basis node-id]
  (let [node (ig/node-by-id-at basis node-id)
        orig-id (:original node)]
    (if orig-id
      (recur basis orig-id)
      node-id)))

(defn jam [self-name ctx-name nodeid-sym transform forms]
  (if (jammable? transform)
    `(let [basis# (:basis ~ctx-name)
           original# (if (:original ~self-name)
                       (ig/node-by-id-at basis# (original-root basis# ~nodeid-sym))
                       ~self-name)]
       (if-let [jammer# (get (:_output-jammers original#) ~transform)]
        (let [jam-value# (jammer#)]
          (if (ie/error? jam-value#)
            (assoc jam-value# :_label ~transform :_node-id ~nodeid-sym)
            jam-value#))
        ~forms))
    forms))

(defn property-has-default-getter? [node-type transform] (ip/default-getter? (property-type node-type transform)))
(defn property-has-no-overriding-output? [node-type transform] (not (contains? (:outputs node-type) transform)))
(defn has-validation? [node-type prop] (ip/validation (get (:declared-properties node-type) prop)))

(defn apply-default-property-shortcut [self-name ctx-name transform node-type forms]
  (let [property-name transform]
    (if (and (has-property? node-type property-name)
            (property-has-default-getter? node-type property-name)
            (property-has-no-overriding-output? node-type property-name)
            (not (has-validation? node-type property-name)))
     `(gt/get-property ~self-name (:basis ~ctx-name) ~transform)
     forms)))

(defn detect-cycles [ctx-name nodeid-sym transform node-type forms]
  `(do
     (assert (every? #(not= % [~nodeid-sym ~transform]) (:in-production ~ctx-name))
             (format "Cycle Detected on node type %s and output %s" ~(:name node-type) ~transform))
     ~forms))

(defn mark-in-production [ctx-name nodeid-sym transform forms]
  `(let [~ctx-name (update ~ctx-name :in-production conj [~nodeid-sym ~transform])]
     ~forms))

(defn check-caches [ctx-name nodeid-sym node-type transform forms]
  (if (get (:cached-outputs node-type) transform)
    (list `or
          (check-local-cache ctx-name nodeid-sym transform)
          (check-global-cache ctx-name nodeid-sym transform)
          forms)
    forms))

(defn gather-inputs [input-sym schema-sym self-name ctx-name nodeid-sym propmap-sym node-type transform production-function forms]
  (let [
        arg-names       (get-in node-type [:transforms transform :input-schema])
        argument-schema (collect-argument-schema transform arg-names node-type)
        argument-forms  (zipmap (keys argument-schema)
                                (map  #(node-input-forms self-name ctx-name nodeid-sym propmap-sym transform node-type %) argument-schema))]
    (list `let
          [input-sym argument-forms schema-sym argument-schema]
          forms)))

(defn input-error-check [self-name ctx-name node-type label input-sym tail]
  (let [internal?    (internal-keys label)]
    (if internal?
      tail
      `(let [bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals ~input-sym)))]
         (if (empty? bad-errors#)
           (let [~input-sym (util/map-vals ie/use-original-value ~input-sym)]
             ~tail)
           (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~label))))))

(defn call-production-function [self-name ctx-name node-type transform input-sym nodeid-sym output-sym forms]
  `(let [production-function# (get-in ~self-name [:node-type :transforms ~transform :fn])
         ~input-sym           (assoc ~input-sym :_node-id ~nodeid-sym :basis (:basis ~ctx-name))
         ~output-sym          (production-function# ~input-sym)]
     ~forms))

(defn cache-output [ctx-name node-type transform nodeid-sym output-sym forms]
  `(do
     ~@(when (get (:cached-outputs node-type) transform)
         `[(swap! (:local ~ctx-name) assoc-in [~nodeid-sym ~transform] ~output-sym)])
     ~forms))

(defn deduce-output-type
  [node-type output]
  (cond
    (property-has-no-overriding-output? node-type output)
    (relax-schema (get-in node-type [:transforms output :output-type]))

    :else
    (do (assert (has-output? node-type output) "Unknown output type")
        (relax-schema (get-in node-type [:transforms output :output-type])))))

(defn schema-check-output [self-name ctx-name node-type transform nodeid-sym output-sym forms]
  `(let [output-schema# (get-in ~self-name [:node-type :transforms ~output-sym :output-type])]
     (if-let [validation-error# (s/check output-schema# ~output-sym)]
       (do
         (warn-output-schema ~nodeid-sym ~transform ~output-sym output-schema# validation-error#)
         (throw (ex-info "SCHEMA-VALIDATION"
                         {:node-id          ~nodeid-sym
                          :type             ~(:name node-type)
                          :output           ~transform
                          :expected         output-schema#
                          :actual           ~output-sym
                          :validation-error validation-error#})))
       ~forms)))

(defn validate-output [self-name ctx-name node-type transform nodeid-sym propmap-sym output-sym forms]
  (if (and (has-property? node-type transform)
           (property-has-no-overriding-output? node-type transform)
           (has-validation? node-type transform))
    (let [property-definition (property-type node-type transform)
          validation (ip/validation property-definition)
          validate-expr (property-validation-exprs self-name ctx-name nodeid-sym node-type propmap-sym transform)]
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
  [node-type transform]
  (let [production-function (get-in node-type [:transforms transform :fn])]
    (gensyms [self-name ctx-name nodeid-sym input-sym schema-sym propmap-sym output-sym]
      `(fn [~self-name ~ctx-name]
         (let [~nodeid-sym (gt/node-id ~self-name)
               ~propmap-sym ~(:declared-properties node-type)]
           ~(if (= transform :this)
              nodeid-sym
              (jam self-name ctx-name nodeid-sym transform
                (apply-default-property-shortcut self-name ctx-name transform node-type
                  (detect-cycles ctx-name nodeid-sym transform node-type
                    (mark-in-production ctx-name nodeid-sym transform
                      (check-caches ctx-name nodeid-sym node-type transform
                        (gather-inputs input-sym schema-sym self-name ctx-name nodeid-sym propmap-sym node-type transform production-function
                          (input-error-check self-name ctx-name node-type transform input-sym
                            (call-production-function self-name ctx-name node-type transform input-sym nodeid-sym output-sym
                              (schema-check-output self-name ctx-name node-type transform nodeid-sym output-sym
                                (validate-output self-name ctx-name node-type transform nodeid-sym propmap-sym output-sym
                                  (cache-output ctx-name node-type transform nodeid-sym output-sym
                                    output-sym)))))))))))))))))

(defn- has-dynamics? [ptype] (not (nil? (ip/dynamic-attributes ptype))))

(defn attach-property-dynamics
  [self-name ctx-name nodeid-sym node-type propmap-sym property-name property-type value-form]
  (gensyms [dynsym]
    `(let [~dynsym (gt/dynamic-attributes (get ~propmap-sym ~property-name))]
       ~(reduce (fn [m [d dfnk]]
                  (assoc m d (call-with-error-checked-fnky-arguments self-name ctx-name nodeid-sym propmap-sym d node-type
                                                                     dfnk
                                                                     `(get ~dynsym ~d))))
                value-form
                (ip/dynamic-attributes property-type)))))

(defn collect-property-values
  [self-name ctx-name propmap-sym node-type nodeid-sym value-sym forms]
  (let [props        (:declared-properties node-type)
        fetch-exprs  (partial collect-base-property-value self-name ctx-name nodeid-sym node-type propmap-sym)
        add-dynamics (partial attach-property-dynamics self-name ctx-name nodeid-sym node-type propmap-sym)]
    `(let [~value-sym ~(apply hash-map
                              (mapcat identity
                                      (for [[p ptype] props
                                            :when (not (:internal? ptype))]
                                        (let [basic-val `{:type    ~ptype
                                                          :value   ~(fetch-exprs p)
                                                          :node-id ~nodeid-sym}]
                                          (if (has-dynamics? ptype)
                                            [p (add-dynamics p ptype basic-val)]
                                            [p basic-val])))))]
       ~forms)))

;; TODO fix produce-value here
(defn- create-validate-argument-form
  [self-name ctx-name nodeid-sym propmap-sym node-type argument]
  ;; This is  similar to node-input-forms, but simpler as we don't have to deal with the case where we're calling
  ;; an output function refering to an argument property with the same name.
  (cond
    (and (has-property? node-type argument) (property-has-no-overriding-output? node-type argument))
    (collect-base-property-value self-name ctx-name nodeid-sym node-type propmap-sym argument)

    (has-declared-output? node-type argument)
    '(gt/produce-value node-type (:basis ctx-name) self-name argument ctx-name)

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
  [self-name ctx-name node-type nodeid-sym propmap-sym prop & [supplied-arguments]]
  (when (has-validation? node-type prop)
    (let [compile-time-validation-fnk (ip/validation (property-type node-type prop))
          arglist (without (util/fnk-arguments compile-time-validation-fnk) (keys supplied-arguments))
          argument-forms (zipmap arglist (map #(create-validate-argument-form self-name ctx-name nodeid-sym propmap-sym node-type % ) arglist))
          argument-forms (merge argument-forms supplied-arguments)]
      `(let [arg-forms# ~argument-forms
             bad-errors# (ie/worse-than (:ignore-errors ~ctx-name) (flatten (vals arg-forms#)))]
         (if (empty? bad-errors#)
           ((get-in ~propmap-sym [~prop :internal.property/validate]) arg-forms#)
           (assoc (ie/error-aggregate bad-errors#) :_node-id (gt/node-id ~self-name) :_label ~prop)))))) ; TODO: decorate with :production :validate?

(defn collect-validation-problems
  [self-name ctx-name nodeid-sym propmap-sym node-type value-map validation-map forms]
  (let [props-with-validation (util/map-vals ip/validation (:declared-properties node-type))
        validation-exprs (partial property-validation-exprs self-name ctx-name nodeid-sym node-type propmap-sym)]
    `(let [~validation-map ~(apply hash-map
                                   (mapcat identity
                                           (for [[p validator] props-with-validation
                                                 :when validator]
                                             [p (validation-exprs p)])))]
       ~forms)
    forms))

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
  [self-name ctx-name node-type display-order-sym forms]
  `(let [~display-order-sym ~(:property-display-order node-type)]
     ~forms))

(defn- assemble-properties-map
  [value-sym display-sym]
  `(hash-map :properties    ~value-sym
             :display-order ~display-sym))

(defn declared-properties-function
  [node-type]
  (let [validations? (not (empty? (keep ip/validation (vals (:declared-properties node-type)))))]
    (println "validations? " validations? (keep ip/validation (vals (:declared-properties node-type))))
    (gensyms [self-name ctx-name kwarg-sym value-map validation-map nodeid-sym display-order propmap-sym]
       (if validations?
           `(fn [~self-name ~ctx-name]
              (let [~nodeid-sym (gt/node-id ~self-name)
                    ~propmap-sym ~(:declared-properties node-type)]
                ~(collect-property-values self-name ctx-name propmap-sym node-type nodeid-sym value-map
                   (collect-validation-problems self-name ctx-name nodeid-sym propmap-sym node-type value-map validation-map
                     (merge-values-and-validation-problems value-map validation-map
                       (collect-display-order self-name ctx-name node-type display-order
                         (assemble-properties-map value-map display-order)))))))
           `(fn [~self-name ~ctx-name]
              (let [~nodeid-sym (gt/node-id ~self-name)
                    ~propmap-sym ~(:declared-properties node-type)]
                ~(collect-property-values self-name ctx-name propmap-sym node-type nodeid-sym value-map
                   (collect-display-order self-name ctx-name node-type display-order
                     (assemble-properties-map value-map display-order)))))))))

(defn node-input-value-function
  [node-type input]
  (gensyms [self-name ctx-name]
   `(fn [~self-name ~ctx-name]
      ~(maybe-use-substitute
        node-type input
        (cond
           (has-multivalued-input? node-type input)
           `(input-value-forms ~self-name ~ctx-name ~input)

           (has-singlevalued-input? node-type input)
           `(first-input-value-form ~self-name ~ctx-name ~input))))))


;; todo delete

(defn- dollar-name
  [node-type-name label]
  (symbol (str node-type-name "$" (name label))))

(defn- output-fn [type output]
  (eval (dollar-name (:name type) output)))

(defrecord NodeImpl [node-type]
       gt/Node
       (node-id        [this]    (:_node-id this))
       (node-type      [_ _]  node-type)
       (property-types [_ _]     (gt/public-properties node-type))
       (get-property   [this basis property] (get this property))
       (set-property   [this basis property value]
         (assert (contains? (gt/property-labels node-type) property)
                 (format "Attempting to use property %s from %s, but it does not exist" property (:name node-type)))
         (assoc this property value))
       (clear-property [this basis property]
         (throw (ex-info (str "Not possible to clear property " property " of node type " (:name node-type) " since the node is not an override")
                         {:label property :node-type node-type})))
       (original [this] nil)
       (set-original [this original-id] (throw (ex-info "Originals can't be changed for original nodes")))
       (override-id [this] nil))

(defrecord OverrideNode [override-id node-id original-id properties]
  gt/Node
  (node-id             [this] node-id)
  (node-type           [this basis] (gt/node-type (ig/node-by-id-at basis original-id) basis))
  (property-types      [this basis] (gt/public-properties (gt/node-type this basis)))
  (get-property        [this basis property] (get properties property (gt/get-property (ig/node-by-id-at basis original-id) basis property)))
  (set-property        [this basis property value] (if (= :_output-jammers property)
                                                     (throw (ex-info "Not possible to mark override nodes as defective" {}))
                                                     (assoc-in this [:properties property] value)))
  (clear-property      [this basis property] (update this :properties dissoc property))
  #_(produce-value       [this output evaluation-context]
    (let [basis (:basis evaluation-context)
          original (ig/node-by-id-at basis original-id)
          type (gt/node-type original basis)]
      (cond
        (= :_node-id output) node-id
        (or (= :_declared-properties output)
            (= :_properties output)) (let [f (output-fn type output)
                                           props (f this evaluation-context)
                                           orig-props (:properties (f original evaluation-context))
                                           dynamic-props (without (set (keys properties)) (set (keys (property-types this basis))))]
                                       (reduce (fn [props [k v]]
                                                 (cond-> props
                                                   (and (= :_properties output)
                                                        (dynamic-props k))
                                                   (assoc-in [:properties k :value] v)

                                                   (contains? orig-props k)
                                                   (assoc-in [:properties k :original-value]
                                                             (get-in orig-props [k :value]))))
                                               props properties))
        (or ((gt/passthroughs type) output)
            ((gt/transforms type) output)
            ((gt/input-labels type) output)) ((output-fn type output) this evaluation-context)
        true (let [dyn-properties (node-value* original :_properties evaluation-context)]
               (if (contains? (:properties dyn-properties) output)
                 (get properties output)
                 (node-value* original output evaluation-context))))))
  (override-id [this] override-id)
  (original [this] original-id)
  (set-original [this original-id] (assoc this :original-id original-id)))

(defn make-override-node [override-id node-id original-id properties]
  (->OverrideNode override-id node-id original-id properties))

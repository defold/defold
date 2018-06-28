(ns internal.node
  (:require [clojure.pprint :as pp]
            [clojure.set :as set]
            [clojure.string :as str]
            [internal.util :as util]
            [internal.cache :as c]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [plumbing.core :as pc]
            [schema.core :as s]
            [clojure.walk :as walk]
            [clojure.zip :as zip])
  (:import [internal.graph.error_values ErrorValue]
           [schema.core Maybe Either]
           [java.lang.ref WeakReference]))

;; TODO - replace use of 'transform' as a variable name with 'label'

(set! *warn-on-reflection* true)

(def ^:dynamic *check-schemas* (get *compiler-options* :defold/check-schemas true))

(def ^:dynamic *node-value-debug* nil)

(defn trace-expr [self ctx output output-type deferred-expr]
  (if-let [tracer (:tracer ctx)]
    (let [node-id (gt/node-id self)]
      (tracer :begin node-id output-type output)
      (let [[result e] (try [(deferred-expr) nil] (catch Exception e [nil e]))]
        (if e
          (do (tracer :fail node-id output-type output)
              (throw e))
          (do (tracer :end node-id output-type output)
              result))))
    (deferred-expr)))

(defn- with-tracer-calls-form [self-name ctx-name output-name output-type expr]
  `(trace-expr ~self-name ~ctx-name ~output-name ~output-type
               (fn [] ~expr)))

(defn- check-dry-run-form
  ([ctx-name expr]
   `(when-not (:dry-run ~ctx-name) ~expr))
  ([ctx-name expr dry-expr]
   `(if-not (:dry-run ~ctx-name) ~expr ~dry-expr)))

(defn nodevalstr [this node-type label & t]
  (apply str "\t" (:_node-id this) "\t"  (:name @node-type) label "\t" t))

(prefer-method pp/code-dispatch clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method pp/simple-dispatch clojure.lang.IPersistentMap clojure.lang.IDeref)

(defprotocol Ref
  (ref-key [this]))

(defn ref? [x] (and x (extends? Ref (class x))))

(defprotocol Type
  (describe* [type])
  (ref*      [type]))

(defn type? [x] (and (extends? Type (class x)) x))
(defn- named? [x] (instance? clojure.lang.Named x))

;;; ----------------------------------------
;;; Node type definition
(declare node-type-resolve value-type-resolve)

(defn- has-flag? [flag entry]
  (contains? (:flags entry) flag))

(def ^:private cached?             (partial has-flag? :cached))
(def ^:private cascade-deletes?    (partial has-flag? :cascade-delete))
(def ^:private extern?             (partial has-flag? :unjammable))
(def ^:private internal?           (partial has-flag? :internal))
(def ^:private explicit?           (partial has-flag? :explicit))
(def ^:private external-property?  (complement internal?))

(def ^:private internal-keys #{:_node-id :_declared-properties :_properties :_output-jammers})

(defn- filterm [pred m]
  (into {} (filter pred m)))

(defprotocol NodeType)

(defn node-type? [x] (satisfies? NodeType x))

(defrecord NodeTypeRef [k]
  Ref
  (ref-key [this] k)

  clojure.lang.IDeref
  (deref [this]
    (node-type-resolve k)))

(defn isa-node-type? [t]
  (instance? NodeTypeRef t))

(defrecord NodeTypeImpl [name supertypes output input property input-dependencies property-display-order]
  NodeType
  Type
  (describe*              [this] (pr-str (select-keys this [:input :output :property])))
  (ref*                   [this] (->NodeTypeRef key)))

;;; accessors for node type information

(defn type-name                [nt]        (some-> nt deref :name))
(defn supertypes               [nt]        (some-> nt deref :supertypes))
(defn property-display-order   [nt]        (some-> nt deref :property-display-order))
(defn all-properties           [nt]        (some-> nt deref :property))
(defn declared-property-labels [nt]        (some-> nt deref :declared-property))
(defn internal-property-labels [nt]        (some-> nt deref :internal-property))
(defn declared-inputs          [nt]        (some-> nt deref :input))
(defn declared-outputs         [nt]        (some-> nt deref :output))
(defn cached-outputs           [nt]        (some-> nt deref :output (->> (filterm #(cached? (val %))) util/key-set)))
(defn input-dependencies       [nt]        (some-> nt deref :input-dependencies))
(defn substitute-for           [nt label]  (some-> nt deref (get-in [:input label :options :substitute])))
(defn input-type               [nt label]  (some-> nt deref (get-in [:input label :value-type])))
(defn input-cardinality        [nt label]  (if (has-flag? :array (get-in (deref nt) [:input label])) :many :one))
(defn behavior                 [nt label]  (some-> nt deref (get-in [:behavior label])))
(defn property-behavior        [nt label]  (some-> nt deref (get-in [:property-behavior label])))
(defn cascade-deletes          [nt]        (some-> nt deref :cascade-deletes))
(defn output-type              [nt label]  (some-> nt deref (get-in [:output label :value-type])))
(defn output-arguments         [nt label]  (some-> nt deref (get-in [:output label :arguments])))
(defn externs                  [nt]        (some-> nt deref :property (->> (filterm #(extern? (val %))) util/key-set)))
(defn property-setter          [nt label]  (some-> nt deref (get-in [:property label :setter :fn]) util/var-get-recursive))
(defn property-type            [nt label]  (some-> nt deref (get-in [:property label :value-type])))
(defn has-input?               [nt label]  (some-> nt deref (get :input) (contains? label)))
(defn has-output?              [nt label]  (some-> nt deref (get :output) (contains? label)))
(defn has-property?            [nt label]  (some-> nt deref (get :property) (contains? label)))
(defn input-labels             [nt]        (util/key-set (declared-inputs nt)))
(defn output-labels            [nt]        (util/key-set (declared-outputs nt)))
(defn declared-properties
  "Beware, more expensive than you might think."
  [nt]
  (into {} (filter (comp (declared-property-labels nt) key)) (all-properties nt)))

(defn jammable-output-labels [nt]
  (into #{}
        (keep (fn [[label {:keys [flags]}]]
                (when-not (contains? flags :unjammable)
                  label)))
        (declared-outputs nt)))

;;; ----------------------------------------
;;; Registry of node types

(defn- type-resolve
  [reg k]
  (loop [type-name k]
    (cond
      (ref? type-name)           (recur (get reg (ref-key type-name)))
      (named? type-name)         (recur (get reg type-name))
      (symbol? type-name)        (recur (util/vgr type-name))
      (type? type-name)          type-name
      (util/protocol? type-name) type-name
      (class? type-name)         type-name
      :else                      nil)))

(defn register-type
  [reg-ref k type]
  (assert (named? k))
  (assert (or (type? type) (named? type) (util/schema? type) (util/protocol? type) (class? type)) (pr-str k type))
  (swap! reg-ref assoc k type)
  k)

(defn unregister-type
  [reg-ref k]
  (swap! reg-ref dissoc k))

(defonce ^:private node-type-registry-ref (atom {}))

(defn node-type-registry [] @node-type-registry-ref)

(defn node-type-resolve  [k] (type-resolve (node-type-registry) k))

(defn node-type [x]
  (cond
    (type? x)  x
    (named? x) (or (node-type-resolve x) (throw (Exception. (str "Unable to resolve node type: " (pr-str x)))))
    :else      (throw (Exception. (str (pr-str x) " is not a node type")))))

(defn register-node-type
  [k node-type]
  (assert (node-type? node-type))
  (->NodeTypeRef (register-type node-type-registry-ref k node-type)))

;;; ----------------------------------------
;;; Value type definition

(defprotocol ValueType
  (dispatch-value [this])
  (schema [this]      "Returns a schema.core/Schema that can conform values of this type"))

(defrecord SchemaType [dispatch-value schema]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema         [_] schema)

  Type
  (describe* [this] (when schema (s/explain schema))))

(defrecord ClassType [dispatch-value ^Class class]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema         [_] class)

  Type
  (describe* [this] (.getName class)))

(defrecord ProtocolType [dispatch-value schema]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema         [_] schema)

  Type
  (describe* [this] dispatch-value))

(defn value-type? [x] (satisfies? ValueType x))

;;; ----------------------------------------
;;; Registry of value types

(defonce ^:private value-type-registry-ref (atom {}))

(defn value-type-registry [] @value-type-registry-ref)

(defrecord ValueTypeRef [k]
  Ref
  (ref-key [this] k)

  clojure.lang.IDeref
  (deref [this]
    (value-type-resolve k)))

(defn register-value-type
  [k value-type]
  (assert value-type)
  (->ValueTypeRef (register-type value-type-registry-ref k value-type)))

(defn- make-protocol-value-type
  [dispatch-value name]
  (->ProtocolType dispatch-value (eval (list `s/protocol name))))

(defn make-value-type
  [name key body]
  (let [dispatch-value (->ValueTypeRef key)]
    (cond
      (class? body)         (->ClassType dispatch-value body)
      (util/protocol? body) (make-protocol-value-type dispatch-value name)
      (util/schema? body)   (->SchemaType dispatch-value body))))

(defn unregister-value-type
  [k]
  (unregister-type value-type-registry-ref k))

(defn value-type-resolve [k]
  (type-resolve (value-type-registry) k))

(defn value-type-schema         [vtr] (when (ref? vtr) (some-> vtr deref schema)))
(defn value-type-dispatch-value [vtr] (when (ref? vtr) (some-> vtr deref dispatch-value)))

;;; ----------------------------------------
;;; Construction support

(defrecord NodeImpl [node-type]
  gt/Node
  (node-id [this]
    (:_node-id this))

  (node-type [_ _]
    node-type)

  (get-property [this basis property]
    (get this property))

  (set-property [this basis property value]
    (assert (contains? (-> node-type all-properties) property)
            (format "Attempting to use property %s from %s, but it does not exist"
                    property (:name @node-type)))
    (assoc this property value))

  (overridden-properties [this basis] {})
  (property-overridden?  [this property] false)

  gt/Evaluation
  (produce-value [this label evaluation-context]
    (let [beh (behavior node-type label)]
      (assert beh (str "No such output, input, or property " label
                       " exists for node " (gt/node-id this)
                       " of type " (:name @node-type)
                       "\nIn production: " (get evaluation-context :in-production)))
      (when *node-value-debug*
        (println (nodevalstr this node-type label)))
      ((:fn beh) this evaluation-context)))

  gt/OverrideNode
  (clear-property [this basis property]
    (throw (ex-info (str "Not possible to clear property " property
                         " of node type " (:name node-type)
                         " since the node is not an override")
                    {:label property :node-type node-type})))

  (original [this]
    nil)

  (set-original [this original-id]
    (throw (ex-info "Originals can't be changed for original nodes")))

  (override-id [this]
    nil))

(defn- defaults
  "Return a map of default values for the node type."
  [node-type-ref]
  (util/map-vals #(some-> % :default :fn util/var-get-recursive (util/apply-if-fn {}))
                 (declared-properties node-type-ref)))

(defn- assert-no-extra-args
  [node-type-ref args]
  (let [args-without-properties (set/difference
                                 (util/key-set args)
                                 (util/key-set (:property (deref node-type-ref))))]
    (assert (empty? args-without-properties) (str "You have given values for properties " args-without-properties ", but those don't exist on nodes of type " (:name node-type-ref)))))

(defn construct
  [node-type-ref args]
  (assert (and node-type-ref (deref node-type-ref)))
  (assert-no-extra-args node-type-ref args)
  (-> (new internal.node.NodeImpl node-type-ref)
      (merge (defaults node-type-ref))
      (merge args)))

;;; ----------------------------------------
;;; Evaluating outputs

(defn without [s exclusions] (reduce disj s exclusions))

(defn- all-labels
  [node-type]
  (set/union (util/key-set (:output node-type)) (util/key-set (:input node-type))))

(def ^:private special-labels #{:_declared-properties})

(defn- ordinary-output-labels
  [description]
  (without (util/key-set (:output description)) special-labels))

(defn- ordinary-input-labels
  [description]
  (without (util/key-set (:input description)) (util/key-set (:output description))))

(defn- ordinary-property-labels
  [node-type]
  (util/key-set (:property node-type)))

(defn- node-value*
  [node label evaluation-context]
  (and node (gt/produce-value node label evaluation-context)))

(defn- validate-evaluation-context-options [options]
  ;; :dry-run means no production functions will be called, useful speedup when tracing dependencies
  ;; :no-local-temp disables the non deterministic local caching of non :cached outputs, useful for stable results when debugging dependencies
  (assert (every? #{:basis :cache :initial-invalidate-counters :tracer :dry-run :no-local-temp} (keys options)) (str (keys options)))
  (assert (not (and (some? (:cache options)) (nil? (:basis options))))))

(defn default-evaluation-context
  [basis cache initial-invalidate-counters]
  (assert (gt/basis? basis))
  (assert (c/cache? cache))
  (assert (map? initial-invalidate-counters))
  {:basis basis
   :cache cache ; cache from the system
   :initial-invalidate-counters initial-invalidate-counters
   :local (atom {}) ; local cache for :cached outputs produced during node-value, will likely populate system cache later on
   :local-temp (atom {}) ; local (weak) cache for non-:cached outputs produced during node-value, never used to populate system cache
   :hits (atom [])
   :in-production #{}
   :tx-data-context (atom {})})

(defn custom-evaluation-context
  [options]
  (validate-evaluation-context-options options)
  (cond-> (assoc options
                 :local           (atom {})
                 :hits            (atom [])
                 :in-production   #{}
                 :tx-data-context (atom {}))
    (not (:no-local-temp options))
    (assoc :local-temp      (atom {}))))

(defn- validate-evaluation-context [evaluation-context]
  (assert (some? (:basis evaluation-context)))
  (assert (some? (:in-production evaluation-context)))
  (assert (some? (:local evaluation-context)))
  (assert (some? (:hits evaluation-context))))

(defn- apply-dry-run-cache [evaluation-context]
  (cond-> evaluation-context
    (:dry-run evaluation-context)
    (assoc :local (atom @(:local evaluation-context))
           :local-temp (some-> (:local-temp evaluation-context) deref atom)
           :hits (atom @(:hits evaluation-context)))))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  return the result, meanwhile collecting stats on cache hits and
  misses (for later cache update) in the evaluation-context."
  [node-or-node-id label evaluation-context]
  (validate-evaluation-context evaluation-context)
  (let [basis (:basis evaluation-context)
        node (if (gt/node-id? node-or-node-id) (gt/node-by-id-at basis node-or-node-id) node-or-node-id)]
    (node-value* node label (apply-dry-run-cache evaluation-context))))

(defn- node-property-value* [node label evaluation-context]
  (let [basis              (:basis evaluation-context)
        node-type          (gt/node-type node basis)]
    (when-let [behavior (property-behavior node-type label)]
      ((:fn behavior) node evaluation-context))))

(defn node-property-value [node-or-node-id label evaluation-context]
  (validate-evaluation-context evaluation-context)
  (let [basis (:basis evaluation-context)
        node (if (gt/node-id? node-or-node-id) (gt/node-by-id-at basis node-or-node-id) node-or-node-id)]
    (node-property-value* node label evaluation-context)))

(def ^:dynamic *suppress-schema-warnings* false)

(defn warn-output-schema [node-id node-type label value output-schema error]
  (when-not *suppress-schema-warnings*
    (println "Schema validation failed for node " node-id "(" node-type " ) label " label)
    (println "Output value:" value)
    (println "Should match:" (s/explain output-schema))
    (println "But:" error)))

;;; ----------------------------------------
;; Type checking

(def ^:private nothing-schema (s/pred (constantly false)))

(defn- prop-type->schema [prop-type]
  (cond
    (util/schema? prop-type)
    prop-type

    (ref? prop-type)
    (value-type-schema prop-type)

    :else nothing-schema))

(defn- check-single-type
  [out in]
  (or
   (= s/Any in)
   (= out in)
   (and (class? in) (class? out) (.isAssignableFrom ^Class in out))))

(defn type-compatible?
  [output-typeref input-typeref]
  (or (not *check-schemas*)
    (let [output-schema (value-type-schema output-typeref)
          input-schema  (value-type-schema input-typeref)
          out-t-pl? (coll? output-schema)
          in-t-pl?  (coll? input-schema)]
      (or
        (= s/Any input-schema)
        (and out-t-pl? (= [s/Any] input-schema))
        (and (= out-t-pl? in-t-pl? true) (check-single-type (first output-schema) (first input-schema)))
        (and (= out-t-pl? in-t-pl? false) (check-single-type output-schema input-schema))
        (and (instance? Maybe input-schema) (type-compatible? output-schema (:schema input-schema)))
        (and (instance? Either input-schema) (some #(type-compatible? output-schema %) (:schemas input-schema)))))))

;;; ----------------------------------------
;;; Node type implementation

(defn- alias-of [ns s]
  (get (ns-aliases ns) s))

(defn localize
  ([ctor s]   (ctor (str *ns*) s))
  ([ctor n s] (ctor n s)))

(defn canonicalize [x]
  (cond
    (and (symbol? x) (namespace x))
    (do (assert (resolve x) (str "Unable to resolve symbol: " (pr-str x) " in this context"))
        (if-let [n (alias-of *ns* (symbol (namespace x)))]
          (symbol (str n) (name x))
          x))

    (util/class-symbol? x)
    x

    (and (symbol? x) (not (namespace x)))
    (do
      (assert (resolve x) (str "Unable to resolve symbol: " (pr-str x) " in this context"))
      (symbol (str *ns*) (name x)))

    (and (keyword? x) (namespace x))
    (if-let [n (alias-of *ns* (symbol (namespace x)))]
      (keyword (str n) (name x))
      x)

    :else
    x))

(defn- display-group?
  "Return true if the coll is a display group.
   A display group is a vector with a string label in the first position."
  [label coll]
  (and (vector? coll) (= label (first coll))))

(defn- display-group
  "Find a display group with the given label in the order."
  [order label]
  (first (filter #(display-group? label %) order)))

(defn join-display-groups
  "Given a display group and an 'order' in the rhs, see if there is a
  display group with the same label in the rhs. If so, attach its
  members to the original display group."
  [[label & _ :as lhs] rhs]
  (let [group-in-rhs (display-group rhs label)]
    (vec (into lhs (rest group-in-rhs)))))

(defn- expand-node-types
  "Replace every symbol that refers to a node type with the display
  order of that node type. E.g., given node BaseNode with display
  order [:a :b :c], then the input [:x :y BaseNode :z]
  becomes [:x :y :a :b :c :z]"
  [coll]
  (flatten
   (map #(if (ref? %) (property-display-order %) %) coll)))

(defn node-type-name? [x]
  (node-type-resolve (keyword (and (named? x) (canonicalize x)))))

(defn merge-display-order
  "Premise: intelligently merge the right-hand display order into the left hand one.
   Rules for merging are as follows:

   - A keyword on the left is left alone.
   - Any keywords on the right that do not appear on the left are appended to the end.
   - A vector with a string in the first position is a display group.
   - A display group on the left remains in the same relative position.
   - A display group on the left is recursively merged with a display group on the right that has the same label.

  When more than two display orders are given, each one is merged into the left successively."
  ([order] order)
  ([order1 order2]
   (loop [result []
          left   order1
          right  order2]
     (if-let [elem (first left)]
       (let [elem (if (node-type-name? elem) (canonicalize elem) elem)]
         (cond
           (ref? elem)
           (recur result (concat (property-display-order elem) (next left)) right)

           (keyword? elem)
           (recur (conj result elem) (next left) (remove #{elem} right))

           (sequential? elem)
           (let [header (first elem)
                 group  (next elem)]
             (if (some ref? elem)
               (recur result (cons (expand-node-types elem) (next left)) right)
               (let [group-label   header
                     group-member? (set group)]
                 (recur (conj result (join-display-groups elem right))
                        (next left)
                        (remove #(or (group-member? %) (display-group? group-label %)) right)))))))
       (into result right))))
  ([order1 order2 & more]
   (reduce merge-display-order (merge-display-order order1 order2) more)))

(def assert-symbol (partial util/assert-form-kind "defnode" "symbol" symbol?))

;;; ----------------------------------------
;;; Parsing defnode forms

(defn- all-available-arguments
  [description]
  (set/union #{:this :_basis}
             (util/key-set (:input description))
             (util/key-set (:property description))
             (util/key-set (:output description))))

(defn- verify-inputs-for-dynamics
  [description]
  (let [available-arguments (all-available-arguments description)]
    (doseq [[property-name property-type]       (:property description)
            [dynamic-name {:keys [arguments]}]  (:dynamics property-type)
            :let     [missing-args (set/difference arguments available-arguments)]]
      (assert (empty? missing-args)
              (str "Node " (:name description) " must have inputs or properties for the label(s) "
                   missing-args ", because they are needed by its property '" (name property-name) "'."))))
  description)

(defn- verify-inputs-for-outputs
  [description]
  (let [available-arguments (all-available-arguments description)]
   (doseq [[output {:keys [arguments]}] (:output description)
           :let [missing-args (set/difference arguments available-arguments)]]
     (assert (empty? missing-args)
             (str "Node " (:name description) " must have inputs, properties or outputs for the label(s) "
                  missing-args ", because they are needed by the output '" (name output) "'."))))
  description)

(defn- verify-labels
  [description]
  (let [inputs     (util/key-set (:input description))
        properties (util/key-set (:property description))
        collisions (set/intersection inputs properties)]
    (assert (empty? collisions) (str "inputs and properties can not be overloaded (problematic fields: " (str/join "," (map #(str "'" (name %) "'") collisions)) ")")))
  description)

(defn- invert-map
  [m]
  (apply merge-with into
         (for [[k vs] m
               v vs]
           {v #{k}})))

(defn- dependency-seq
  ([desc inputs]
   (dependency-seq desc #{} inputs))
  ([desc seen inputs]
   (disj
    (reduce
     (fn [dependencies argument]
       (conj
        (if (not (seen argument))
          (if-let [recursive (get-in desc [:output argument :dependencies])]
            (into dependencies (dependency-seq desc (conj seen argument) recursive))
            dependencies)
          dependencies)
        argument))
     #{}
     inputs)
    :this)))

(defn- description->input-dependencies
  [{:keys [output] :as description}]
  (let [outputs (zipmap (keys output)
                        (map #(dependency-seq description (:dependencies %)) (vals output)))]
    (invert-map outputs)))

(defn- attach-input-dependencies
  [description]
  (assoc description :input-dependencies (description->input-dependencies description)))

(declare node-property-value-function-form)
(declare node-output-value-function-form)
(declare declared-properties-function-form)
(declare node-input-value-function-form)

(defn- transform-properties-plumbing-map [description]
  (let [labels  (ordinary-property-labels description)]
    (zipmap labels
      (map (fn [label]
             {:fn (node-property-value-function-form description label)}) labels))))

(defn- attach-property-behaviors
  [description]
  (update description :property-behavior merge (transform-properties-plumbing-map description)))

(defn- transform-outputs-plumbing-map [description]
  (let [labels  (ordinary-output-labels description)]
    (zipmap labels
            (map (fn [label]
                   {:fn (node-output-value-function-form description label)}) labels))))

(defn- attach-output-behaviors
  [description]
  (update description :behavior merge (transform-outputs-plumbing-map description)))

(defn- transform-inputs-plumbing-map [description]
  (let [labels  (ordinary-input-labels description)]
    (zipmap labels
            (map (fn [input] {:fn (node-input-value-function-form description input)}) labels))))

(defn- attach-input-behaviors
  [description]
  (update description :behavior merge (transform-inputs-plumbing-map description)))

(defn- abstract-function-form
  [label type]
  (let [format-string (str "Node %d does not supply a production function for the abstract '" label "' output. Add (output " label " " type " your-function) to the definition")]
    `(pc/fnk [~'this]
             (throw (AssertionError.
                     (format ~format-string
                      (gt/node-id ~'this)))))))

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

(def ^:dynamic *autotypes* (atom {}))

(defn- named->vtr
  [symbol-or-keyword]
  (->ValueTypeRef (keyword (canonicalize symbol-or-keyword))))

;; A lot happens in parse-type-form. It creates information that is
;; used during the rest of node compilation. If the type form was
;; previously defined with deftype, then everything is easy.
;;
;; However, you can also use a Java class name or Clojure protocol
;; name here.
;;
;; if you define a node type with a Java class (or Clojure record),
;; then you use that class or record for multimethod dispatch.
;;
;; otoh, if you define a node type with a deftype'd type, then you
;; use the typeref as the multimethod dispatch value.
(defn parse-type-form
  [where original-form]
  (let [multivalued? (vector? original-form)
        form         (if multivalued? (first original-form) original-form)
        autotype     (cond
                       (util/protocol-symbol? form) `(->ProtocolType ~form (s/protocol ~form))
                       (util/class-symbol? form)    `(->ClassType ~form ~form))
        typeref      (cond
                       (ref? form)                  form
                       (util/protocol-symbol? form) (named->vtr form)
                       (util/class-symbol? form)    (named->vtr (.getName ^Class (resolve form)))
                       (named? form)                (named->vtr form))]
    (assert (not (nil? typeref))
            (str "defnode " where " requires a value type but was supplied with '"
                 original-form "' which cannot be used as a type"))
    (when (and (ref? typeref) (nil? autotype))
      (assert (not (nil? (deref typeref)))
              (str "defnode " where " requires a value type but was supplied with '"
                   original-form "' which cannot be used as a type")))
    (util/assert-form-kind "defnode" "registered value type"
                           (some-fn ref? value-type?) where typeref)
    (when autotype
      ;; the next two steps look redundant but are not. when we build
      ;; the release bundle, macroexpansion happens during compilation.
      ;; we need type information for compilation
      (register-type value-type-registry-ref (:k typeref)
                     (cond
                       (util/protocol-symbol? form)
                       (let [pval (util/vgr form)]
                         (make-protocol-value-type pval form))

                       ;; note: this occurs in type position of a defnode clause. we use
                       ;; the class itself as the dispatch value so multimethods can be
                       ;; expressed most naturally.
                       (util/class-symbol? form)
                       (let [cls (resolve form)]
                         (->ClassType cls cls))))

      ;; when we run the bundle, compilation is long past, we we
      ;; need to re-register the automatic types at runtime. defnode
      ;; emits code to do that, based on the types we smuggle out via
      ;; this (hacky) atom
      (swap! *autotypes* assoc (:k typeref) autotype))
    {:value-type typeref
     :flags (if multivalued? #{:collection} #{})}))

(defn- macro-expression?
  [f]
  (when-let [term (and (seq? f) (symbol? (first f)) (first f))]
    (let [v (resolve term)]
      (and (var? v) (.isMacro ^clojure.lang.Var v)))))

(defn- fnk-expression?
  [f]
  (when-let [term (and (seq? f) (symbol? (first f)) (first f))]
    (or (= "#'dynamo.graph/fnk" (str (resolve term))))))

(defn- maybe-macroexpand
  [f]
  (if (and (macro-expression? f) (not (fnk-expression? f)))
    (macroexpand-1 f)
    f))

(def ^:private node-intrinsics
  [(list 'extern '_node-id :dynamo.graph/NodeID)
   (list 'output '_properties :dynamo.graph/Properties `(dynamo.graph/fnk [~'_declared-properties] ~'_declared-properties))
   (list 'extern '_output-jammers :dynamo.graph/KeywordMap)
   (list 'output '_overridden-properties :dynamo.graph/KeywordMap `(dynamo.graph/fnk [~'this ~'_basis] (gt/overridden-properties ~'this ~'_basis)))])

(defn- maybe-inject-intrinsics
  [forms]
  (if (some #(= 'inherits %) (map first forms))
    forms
    (concat node-intrinsics forms)))

(defmulti process-property-form first)

(defmethod process-property-form 'dynamic [[_ label forms]]
  (assert-symbol "dynamic" label)
  {:dynamics {(keyword label) {:fn (maybe-macroexpand forms)}}})

(defmethod process-property-form 'value [[_ form]]
  {:value {:fn (maybe-macroexpand form)}})

(defmethod process-property-form 'set [[_ form]]
  {:setter {:fn (maybe-macroexpand form)}})

(defmethod process-property-form 'default [[_ form]]
  {:default {:fn (maybe-macroexpand form)}})

(defn- process-property-forms
  [[type-form & body-forms]]
  (apply merge-with merge
         (parse-type-form "property" type-form)
         (for [b body-forms]
           (process-property-form b))))

(defmulti process-as first)

(defn- mark-unjammable [flags]
  (conj (or flags #{}) :unjammable))

(defmethod process-as 'extern [[_ label & forms]]
  (assert-symbol "extern" label)
  (-> (process-as (list* 'property label forms))
      (update-in [:property (keyword label) :flags] mark-unjammable)
      (update-in [:output (keyword label) :flags] mark-unjammable)))

(defmethod process-as 'property [[_ label & forms]]
  (assert-symbol "property" label)
  (let [klabel  (keyword label)
        propdef (cond-> (process-property-forms forms)
                  (contains? internal-keys klabel)
                  (update :flags #(conj (or % #{}) :internal)))
        outdef  (-> propdef
                    (dissoc :setter :dynamics :value)
                    (assoc :fn
                           (if-let [evaluator (-> propdef :value :fn)]
                             evaluator
                             `(dynamo.graph/fnk [~'this ~label] (get ~'this ~klabel)))))
        desc    {:property            {klabel propdef}
                 :property-order-decl (if (contains? internal-keys klabel) [] [klabel])
                 :output              {klabel outdef}}]
    desc))

(def ^:private output-flags   #{:cached :abstract :unjammable})
(def ^:private output-options #{})

(defmethod process-as 'output [[_ label & forms]]
  (assert-symbol "output" label)
  (let [type-form                (first forms)
        base                     (parse-type-form "output" type-form)
        [flags options fn-forms] (parse-flags-and-options output-flags output-options (rest forms))
        abstract?                (contains? flags :abstract)]
    (assert (or abstract? (first fn-forms))
            (format "Output %s has no production function and is not abstract" label))
    (assert (not (next fn-forms))
            (format "Output %s seems to have something after the production function: " label (next fn-forms)))
    {:output
     {(keyword label)
      (merge-with into base {:flags   (into #{} (conj flags :explicit))
                             :options options
                             :fn      (if abstract?
                                        (abstract-function-form label type-form)
                                        (maybe-macroexpand (first fn-forms)))})}}))

(def ^:private input-flags   #{:array :cascade-delete})
(def ^:private input-options #{:substitute})

(defmethod process-as 'input [[_ label & forms]]
  (assert-symbol "input" label)
  (let [type-form         (first forms)
        base              (parse-type-form "input" type-form)
        [flags options _] (parse-flags-and-options input-flags input-options (rest forms))]
    {:input
     {(keyword label)
      (merge-with into base {:flags flags :options options})}}))

(defmethod process-as 'inherits [[_ & forms]]
  {:supertypes
   (for [f forms]
     (do
       (assert-symbol "inherits" f)
       (let [typeref (util/vgr f)]
         (assert (node-type-resolve typeref)
                 (str "Cannot inherit from " f " it cannot be resolved in this context (from namespace " *ns* ".)"))
         typeref)))})

(defmethod process-as 'display-order [[_ decl]]
  {:display-order-decl decl})

(defn- group-node-type-forms
  [forms]
  (let [parse (for [f forms :when (seq? f)]
                (process-as f))]
    (apply merge-with into parse)))

(defn- node-type-merge
  ([] {})
  ([l] l)
  ([l r]
   {:property            (merge-with merge (:property l) (:property r))
    :input               (merge-with merge (:input l)    (:input r))
    :output              (merge-with merge (:output l)   (:output r))
    :supertypes          (into (:supertypes l)           (:supertypes r))
    ;; Display order gets resolved at runtime
    :property-order-decl (:property-order-decl r)
    :display-order-decl  (:display-order-decl r)})
  ([l r & more]
   (reduce node-type-merge (node-type-merge l r) more)))

(defn- merge-supertypes
  [tree]
  (let [supertypes (map deref (get tree :supertypes []))]
    (node-type-merge (apply node-type-merge supertypes) tree)))

(defn- defer-display-order-resolution
  [tree]
  (assoc tree :property-display-order
         `(merge-display-order ~(:display-order-decl tree) ~(:property-order-decl tree)
                                 ~@(map property-display-order (:supertypes tree)))))

(defn- wrap-when
  [tree key-pred val-pred xf]
  (walk/postwalk
   (fn [f]
     (if (vector? f)
       (let [[k v] f]
         (if (and (key-pred k) (val-pred v))
           [k (xf v)]
           f))
       f))
   tree))

(defn- wrap-constant-fns
  [tree]
  (wrap-when tree #(= :fn %)
             (fn [v] (not (or (seq? v) (util/pfnksymbol? v) (util/pfnkvar? v))))
             (fn [v] `(dynamo.graph/fnk [] ~(if (symbol? v) (resolve v) v)))))

(defn- into-set [x v] (into (or x #{}) v))

(defn- extract-fn-arguments
  [tree]
  (walk/postwalk
   (fn [f]
     (if (and (map? f) (contains? f :fn))
       (let [args (util/inputs-needed (:fn f))]
         (-> f
             (update :arguments #(into-set % args))
             (update :dependencies #(into-set % args))))
       f))
   tree))

(defn- prop+args [[pname pdef]]
  (into #{pname} (:arguments pdef)))

(defn- attach-declared-properties
  [description]
  (let [publics (apply disj (reduce into #{} (map prop+args (:property description))) internal-keys)]
    (assoc-in description [:output :_declared-properties]
              {:value-type   (->ValueTypeRef :dynamo.graph/Properties)
               :flags        #{:cached}
               :arguments    publics
               :dependencies publics})))

(defn- attach-declared-properties-behavior
  [description]
  (assoc-in description [:behavior :_declared-properties :fn]
            (declared-properties-function-form description)))

(defn- attach-cascade-deletes
  [{:keys [input] :as description}]
  (assoc description :cascade-deletes (into #{} (comp (filter (comp cascade-deletes? val)) (map key)) input)))

(defn- attach-declared-internal-property
  [{:keys [property] :as description}]
  (assoc description
    :declared-property (into #{} (comp (remove (comp internal? val)) (map key)) property)
    :internal-property (into #{} (comp (filter (comp internal? val)) (map key)) property)))

(defn- recursive-filter
  [m k]
  (filter #(and (map? %) (contains? % k)) (tree-seq map? vals m)))

(defn- all-subtree-dependencies
  [tree]
  (apply set/union (map :dependencies (recursive-filter tree :dependencies))))

(defn- merge-property-dependencies
  [tree]
  (update tree :property
          (fn [properties]
            (reduce-kv
             (fn [props pname pdef]
               (update-in props [pname :dependencies] #(disj (into-set % (all-subtree-dependencies pdef)) pname)))
             properties
             properties))))

(defn- apply-property-dependencies-to-outputs
  [tree]
  (reduce-kv
   (fn [tree pname pdef]
     (update-in tree [:output pname :dependencies] #(into-set % (:dependencies pdef))))
   tree
   (:property tree)))

(defn process-node-type-forms
  [fqs forms]
  (-> (maybe-inject-intrinsics forms)
      group-node-type-forms
      merge-supertypes
      (assoc :name (str fqs))
      (assoc :key (keyword fqs))
      wrap-constant-fns
      defer-display-order-resolution
      extract-fn-arguments
      merge-property-dependencies
      apply-property-dependencies-to-outputs
      attach-declared-properties
      attach-input-dependencies
      attach-property-behaviors
      attach-output-behaviors
      attach-input-behaviors
      attach-declared-properties-behavior
      attach-cascade-deletes
      attach-declared-internal-property
      verify-inputs-for-dynamics
      verify-inputs-for-outputs
      verify-labels))

(defn- map-zipper [m]
  (zip/zipper
   (fn [x] (or (map? x) (map? (nth x 1))))
   (fn [x] (seq (if (map? x) x (nth x 1))))
   (fn [x children]
     (if (map? x)
       (into {} children)
       (assoc x 1 (into {} children))))
   m))

(defn- key-path
  [z]
  (map first (rest (zip/path z))))

(defn extract-functions
  [tree]
  (loop [where (map-zipper tree)
         what  []]
    (if (zip/end? where)
      what
      (recur (zip/next where)
             (if (= :fn (first (zip/node where)))
               (conj what [(key-path where) (second (zip/node where))])
               what)))))

(defn dollar-name [prefix path]
    (->> path
         (map name)
         (interpose "$")
         (apply str prefix "$")
         symbol))

;;; ----------------------------------------
;;; Code generation

(defmacro gensyms
  [[:as syms] & forms]
  (let [bindings (vec (interleave syms (map (fn [s] `(gensym ~(name s))) syms)))]
    `(let ~bindings
       ~@forms)))

(declare fnk-argument-form)

(defn- desc-has-input?     [description argument] (contains? (:input description) argument))
(defn- desc-has-property?  [description argument] (contains? (:property description) argument))
(defn- desc-has-output?    [description argument] (contains? (:output description) argument))
(defn- desc-has-explicit-output?    [description argument]
  (contains? (get-in description [:output argument :flags]) :explicit))

(defn- has-multivalued-input?  [description input-label]
  (contains? (get-in description [:input input-label :flags]) :array))

(defn- has-singlevalued-input? [description input-label]
  (and (desc-has-input? description input-label)
       (not (has-multivalued-input? description input-label))))

(defn- has-substitute? [description input-label]
  (contains? (get-in description [:input input-label :options]) :substitute))

(defn- property-overloads-output? [description argument output]
  (and (= output argument)
       (desc-has-property? description argument)
       (desc-has-explicit-output? description argument)))

(defn- unoverloaded-output? [description argument output]
  (and (not= output argument)
       (desc-has-output? description argument)))

(defn pull-first-input-value
  [input node evaluation-context]
  (let [basis                      (:basis evaluation-context)
        [upstream-id output-label] (first (gt/sources basis (gt/node-id node) input))]
     (when-let [upstream-node (and upstream-id (gt/node-by-id-at basis upstream-id))]
       (gt/produce-value upstream-node output-label evaluation-context))))

(defn pull-first-input-with-substitute
  [input sub node evaluation-context]
  ;; todo - invoke substitute
  (pull-first-input-value input node evaluation-context))

(defn- first-input-value-form
  [self-name ctx-name input]
  `(pull-first-input-value ~input ~self-name ~ctx-name))

(defn pull-input-values
  [input node evaluation-context]
  (let [basis (:basis evaluation-context)]
    (mapv (fn [[upstream-id output-label]]
            (let [upstream-node (gt/node-by-id-at basis upstream-id)]
              (gt/produce-value upstream-node output-label evaluation-context)))
          (gt/sources basis (gt/node-id node) input))))

(defn pull-input-values-with-substitute
  [input sub node evaluation-context]
  ;; todo - invoke substitute
  (pull-input-values input node evaluation-context))

(defn- input-value-form
  [self-name ctx-name input]
  `(pull-input-values ~input ~self-name ~ctx-name))

(defn- maybe-substitute-error-in-value-form
  [nodeid-sym description input forms]
  (let [sub       (get-in description [:input input :options :substitute] ::no-sub) ; nil is a valid substitute literal
        input-val (gensym)]
    `(let [~input-val ~forms]
       (if (instance? ErrorValue ~input-val)
         (if (ie/worse-than :info ~input-val)
           ~(if (not= ::no-sub sub)
              `(util/apply-if-fn ~sub ~input-val)
              `(ie/error-aggregate [~input-val] :_node-id ~nodeid-sym :_label ~input))
           (:value ~input-val))
         ~input-val))))

(defn- maybe-substitute-error-in-array-form
  [nodeid-sym description input forms]
  (let [sub            (get-in description [:input input :options :substitute] ::no-sub) ; nil is a valid substitute literal
        input-array    (gensym)
        serious-errors (gensym)]
    `(let [~input-array ~forms]
       (if (some #(instance? ErrorValue %) ~input-array)
         (let [~serious-errors (filter #(ie/worse-than :info %) ~input-array)]
           (if (empty? ~serious-errors)
             (mapv #(if (instance? ErrorValue %) (:value %) %) ~input-array)
             ~(if (not= ::no-sub sub)
                `(util/apply-if-fn ~sub ~input-array)
                `(ie/error-aggregate ~serious-errors :_node-id ~nodeid-sym :_label ~input))))
         ~input-array))))

(defn- call-with-error-checked-fnky-arguments-form
  [self-name ctx-name nodeid-sym label description arguments runtime-fnk-expr & [supplied-arguments]]
  (let [base-args      {:_node-id `(gt/node-id ~self-name) :_basis `(:basis ~ctx-name)}
        arglist        (without arguments (keys supplied-arguments))
        argument-forms (zipmap arglist (map #(get base-args % (if (= label %)
                                                                `(gt/get-property ~self-name (:basis ~ctx-name) ~label)
                                                                (fnk-argument-form self-name ctx-name nodeid-sym label description %)))
                                            arglist))
        argument-forms (merge argument-forms supplied-arguments)]
    (if (empty? argument-forms)
      `(~runtime-fnk-expr {})
      `(let [arg-forms# ~argument-forms
             argument-errors# (filter #(instance? ErrorValue %) (vals arg-forms#))]
         (if (empty? argument-errors#)
           (~runtime-fnk-expr arg-forms#)
           (ie/error-aggregate argument-errors# :_node-id ~nodeid-sym :_label ~label))))))

(defn- collect-base-property-value-form
  [self-name ctx-name nodeid-sym description prop-name]
  (let [property-definition (get-in description [:property prop-name])
        default?            (not (:value property-definition))]
    (if default?
      (with-tracer-calls-form self-name ctx-name prop-name :raw-property
        (check-dry-run-form ctx-name `(gt/get-property ~self-name (:basis ~ctx-name) ~prop-name)))
      (with-tracer-calls-form self-name ctx-name prop-name :property
        (call-with-error-checked-fnky-arguments-form self-name ctx-name nodeid-sym prop-name description
                                                     (get-in property-definition [:value :arguments])
                                                     (check-dry-run-form ctx-name
                                                                         `(var ~(symbol (dollar-name (:name description) [:property prop-name :value])))
                                                                         `(constantly nil)))))))
(defn- collect-property-value-form
  [self-name ctx-name nodeid-sym description prop]
  (let [property-definition (get-in description [:property prop])
        default?            (not (:value property-definition))
        get-expr            (if default?
                              (with-tracer-calls-form self-name ctx-name prop :raw-property
                                (check-dry-run-form ctx-name `(gt/get-property ~self-name (:basis ~ctx-name) ~prop)))
                              (with-tracer-calls-form self-name ctx-name prop :property
                                (call-with-error-checked-fnky-arguments-form self-name ctx-name nodeid-sym prop description
                                                                             (get-in property-definition [:value :arguments])
                                                                             (check-dry-run-form ctx-name
                                                                                                 `(var ~(dollar-name (:name description) [:property prop :value]))
                                                                                                 `(constantly nil)))))]
    get-expr))

(defn- fnk-argument-form
  [self-name ctx-name nodeid-sym output description argument]
  (cond
    (= :this argument)
    self-name

    (= :_basis argument)
    `(:basis ~ctx-name)

    (property-overloads-output? description argument output)
    (collect-property-value-form self-name ctx-name nodeid-sym description argument)

    (unoverloaded-output? description argument output)
    `(gt/produce-value ~self-name ~argument ~ctx-name)

    (desc-has-property? description argument)
    (if (= output argument)
      (with-tracer-calls-form self-name ctx-name argument :raw-property
        (check-dry-run-form ctx-name `(gt/get-property ~self-name (:basis ~ctx-name) ~argument)))
      (collect-property-value-form self-name ctx-name nodeid-sym description argument))

    (has-multivalued-input? description argument)
    (maybe-substitute-error-in-array-form
      nodeid-sym description argument
      (input-value-form self-name ctx-name argument))

    (has-singlevalued-input? description argument)
    (maybe-substitute-error-in-value-form
      nodeid-sym description argument
      (first-input-value-form self-name ctx-name argument))

    (desc-has-output? description argument)
    `(gt/produce-value  ~self-name ~argument ~ctx-name)

    :else
    (assert false (str "A function needs an argument this node can't supply. There is no input, output, or property called " (pr-str argument)))))

(def ^:private jammable? (complement internal-keys))

(defn original-root [basis node-id]
  (let [node (gt/node-by-id-at basis node-id)
        orig-id (:original-id node)]
    (if orig-id
      (recur basis orig-id)
      node-id)))

(defn- check-jammed-form [self-name ctx-name nodeid-sym transform forms]
  (if (jammable? transform)
    `(let [basis# (:basis ~ctx-name)
           original# (if (:original-id ~self-name)
                       (gt/node-by-id-at basis# (original-root basis# ~nodeid-sym))
                       ~self-name)]
       (if-let [jammer# (get (:_output-jammers original#) ~transform)]
         (let [jam-value# (jammer#)]
           (if (ie/error? jam-value#)
             (assoc jam-value# :_label ~transform :_node-id ~nodeid-sym)
             jam-value#))
         ~forms))
    forms))

(defn- property-has-default-getter?       [description label] (not (get-in description [:property label :value])))
(defn- property-has-no-overriding-output? [description label] (not (desc-has-explicit-output? description label)))

(defn- apply-default-property-shortcut-form [self-name ctx-name property-name description forms]
  (let [property? (and (desc-has-property? description property-name) (property-has-no-overriding-output? description property-name))
        default?  (and (property-has-default-getter? description property-name)
                       (property-has-no-overriding-output? description property-name))]
    (if default?
      (with-tracer-calls-form self-name ctx-name property-name :raw-property
        (check-dry-run-form ctx-name `(gt/get-property ~self-name (:basis ~ctx-name) ~property-name)))
      forms)))

(defn mark-in-production [ctx node-type-name node-id label]
  (assert (not (contains? (:in-production ctx) [node-id label]))
          (format "Cycle detected on node type %s and output %s" node-type-name label))
  (update ctx :in-production conj [node-id label]))

(defn- mark-in-production-form [ctx-name nodeid-sym transform description forms]
  `(let [~ctx-name (mark-in-production ~ctx-name ~(:name description) ~nodeid-sym ~transform)]
     ~forms))

(defn- check-caches-form [self-name ctx-name nodeid-sym description transform local-cache-sym forms]
  (gensyms [local global local-temp local-temp-res key]
           (if (get-in description [:output transform :flags :cached])
             `(let [~local-cache-sym (:local ~ctx-name)
                    ~local  (deref ~local-cache-sym)
                    ~global (:cache ~ctx-name)
                    ~key [~nodeid-sym ~transform]]
                (cond
                  (contains? ~local ~key)
                  ~(with-tracer-calls-form self-name ctx-name transform :cache
                     `(get ~local ~key))

                  (contains? ~global ~key)
                  ~(with-tracer-calls-form self-name ctx-name transform :cache
                     `(if-some [cached# (get ~global ~key)]
                        (do (swap! (:hits ~ctx-name) conj ~key)
                            cached#)))

                  true
                  ~forms))
             `(let [~local-temp (some-> (:local-temp ~ctx-name) deref)
                    ~key [~nodeid-sym ~transform]
                    weak# ^WeakReference (get ~local-temp ~key)]
                (if-some [~local-temp-res (and weak# (.get weak#))]
                  ~(with-tracer-calls-form self-name ctx-name transform :cache
                     `(if (= ~local-temp-res ::nil) nil ~local-temp-res))
                  ~forms)))))

(defn- gather-inputs-form [input-sym schema-sym self-name ctx-name nodeid-sym description transform production-function forms]
  (let [arg-names       (get-in description [:output transform :arguments])
        argument-forms  (zipmap arg-names (map #(fnk-argument-form self-name ctx-name nodeid-sym transform description %) arg-names))
        argument-forms  (assoc argument-forms :_node-id nodeid-sym :_basis `(:basis ~ctx-name))]
    (list `let
          [input-sym argument-forms]
          forms)))

(defn- input-error-check-form [self-name ctx-name description label nodeid-sym input-sym tail]
  (if (contains? internal-keys label)
    tail
    `(let [input-errors# (filter #(instance? ErrorValue %) (vals ~input-sym))]
       (if (empty? input-errors#)
         ~tail
         (ie/error-aggregate input-errors# :_node-id ~nodeid-sym :_label ~label)))))

(defn- call-production-function-form [self-name ctx-name description transform input-sym nodeid-sym output-sym forms]
  `(let [~output-sym ~(input-error-check-form self-name ctx-name description transform nodeid-sym input-sym
                                              (check-dry-run-form ctx-name `((var ~(symbol (dollar-name (:name description) [:output transform]))) ~input-sym)))]
     ~forms))

(defn- cache-output-form [ctx-name description transform nodeid-sym output-sym local-cache-sym forms]
  (if (contains? (get-in description [:output transform :flags]) :cached)
    `(do
       (swap! ~local-cache-sym assoc [~nodeid-sym ~transform] ~output-sym)
       ~forms)
    `(do
       (when-let [local-temp# (:local-temp ~ctx-name)]
         (swap! local-temp# assoc [~nodeid-sym ~transform] (WeakReference. (if (= ~output-sym nil) ::nil ~output-sym))))
       ~forms)))

(defn deduce-output-type-form
  [self-name description transform]
  (let [schema (some-> (get-in description [:output transform :value-type]) value-type-schema)
        schema (if (get-in description [:output transform :flags :collection])
                 (vector schema)
                 schema)]
    `(s/either (s/maybe ~schema) ErrorValue))) ; allow nil's and errors

(defn report-schema-error
  [node-type-name transform nodeid-sym output-sym output-schema validation-error]
  (warn-output-schema nodeid-sym node-type-name transform output-sym output-schema validation-error)
  (throw (ex-info "SCHEMA-VALIDATION"
                  {:node-id          nodeid-sym
                   :type             node-type-name
                   :output           transform
                   :expected         output-schema
                   :actual           output-sym
                   :validation-error validation-error})))

(defn- schema-check-output-form [self-name ctx-name description transform nodeid-sym output-sym forms]
  (if *check-schemas*
    `(let [output-schema# ~(deduce-output-type-form self-name description transform)]
       (try
         (if-let [validation-error# (when-not (:dry-run ~ctx-name) (s/check output-schema# ~output-sym))]
           (report-schema-error ~(:name description) ~transform ~nodeid-sym ~output-sym output-schema# validation-error#)
           ~forms)
         (catch IllegalArgumentException iae#
           (throw (ex-info "MALFORMED-SCHEMA"
                           {:transform ~transform :node-type ~(:name description)}
                           iae#)))))
    forms))

(defn- node-property-value-function-form [description property]
  (gensyms [self-name ctx-name nodeid-sym]
    `(fn [~self-name ~ctx-name]
       (let [~nodeid-sym (gt/node-id ~self-name)]
         ~(collect-property-value-form self-name ctx-name nodeid-sym description property)))))

(defn- node-output-value-function-form
  [description transform]
  (let [production-function (get-in description [:output transform :fn])
        tracer-output-type (if (desc-has-explicit-output? description transform) :output :property)]
    (gensyms [self-name ctx-name nodeid-sym input-sym schema-sym output-sym local-cache-sym]
      `(fn [~self-name ~ctx-name]
         (let [~nodeid-sym (gt/node-id ~self-name)]
           ~(if (= transform :this)
              nodeid-sym
              (check-jammed-form self-name ctx-name nodeid-sym transform
                (apply-default-property-shortcut-form self-name ctx-name transform description
                  (mark-in-production-form ctx-name nodeid-sym transform description
                    (check-caches-form self-name ctx-name nodeid-sym description transform local-cache-sym
                      (with-tracer-calls-form self-name ctx-name transform tracer-output-type
                        (gather-inputs-form input-sym schema-sym self-name ctx-name nodeid-sym description transform production-function
                          (call-production-function-form self-name ctx-name description transform input-sym nodeid-sym output-sym
                            (schema-check-output-form self-name ctx-name description transform nodeid-sym output-sym
                              (cache-output-form ctx-name description transform nodeid-sym output-sym local-cache-sym
                                output-sym)))))))))))))))

(defn- assemble-properties-map-form
  [nodeid-sym value-sym display-order]
  `(hash-map :properties    ~value-sym
             :display-order ~display-order
             :node-id       ~nodeid-sym))

(defn- property-dynamics
  [self-name ctx-name nodeid-sym description property-name property-type value-form]
  (apply merge
         (for [[dynamic-label {:keys [arguments] :as dynamic}] (get property-type :dynamics)]
           {dynamic-label
            (with-tracer-calls-form self-name ctx-name [property-name dynamic-label] :dynamic
              (call-with-error-checked-fnky-arguments-form self-name ctx-name nodeid-sym dynamic-label description arguments
                                                           (check-dry-run-form ctx-name
                                                                               `(var ~(dollar-name (:name description) [:property property-name :dynamics dynamic-label]))
                                                                               `(constantly nil))))})))

(defn- property-value-exprs
  [self-name ctx-name nodeid-sym description prop-name prop-type]
  (let [basic-val `{:type    ~(:value-type prop-type)
                    :value   ~(collect-base-property-value-form self-name ctx-name nodeid-sym description prop-name)
                    :node-id ~nodeid-sym}]
    (if (not (empty? (:dynamics prop-type)))
      (let [dyn-exprs (property-dynamics self-name ctx-name nodeid-sym description prop-name prop-type basic-val)]
        (merge basic-val dyn-exprs))
      basic-val)))

(defn- declared-properties-function-form
  [description]
  (let [props (:property description)]
    (gensyms [self-name ctx-name value-map nodeid-sym display-order]
      `(fn [~self-name ~ctx-name]
         (let [~nodeid-sym    (gt/node-id ~self-name)
               ~display-order (-> (gt/node-type ~self-name (:basis ~ctx-name))
                                  (property-display-order))
               ~value-map     ~(apply merge {}
                                      (for [[p _] (filter (comp external-property? val) props)]
                                        {p (property-value-exprs self-name ctx-name nodeid-sym description p (get props p))}))]
           ~(check-dry-run-form ctx-name (assemble-properties-map-form nodeid-sym value-map display-order)))))))

(defn- node-input-value-function-form
  [description input]
  (let [sub?   (has-substitute?        description input)
        multi? (has-multivalued-input? description input)]
    (cond
      (and (not sub?) (not multi?))
      `(partial pull-first-input-value ~input)

      (and (not sub?) multi?)
      `(partial pull-input-values ~input)

      (not multi?)
      `(partial pull-first-input-with-substitute ~input ~sub?)

      :else
      `(partial pull-input-values-with-substitute ~input ~sub?))))

;;; ----------------------------------------
;;; Overrides

(defrecord OverrideNode [override-id node-id original-id properties]
  gt/Node
  (node-id               [this]                      node-id)
  (node-type             [this basis]                (gt/node-type (gt/node-by-id-at basis original-id) basis))
  (get-property          [this basis property]
    (get properties property (gt/get-property (gt/node-by-id-at basis original-id) basis property)))
  (set-property          [this basis property value]
    (if (= :_output-jammers property)
      (throw (ex-info "Not possible to mark override nodes as defective" {}))
      (assoc-in this     [:properties property] value)))
  (overridden-properties [this basis] properties)
  (property-overridden?  [this property] (contains? properties property))

  gt/Evaluation
  (produce-value       [this output evaluation-context]
    (let [basis    (:basis evaluation-context)
          type     (gt/node-type this basis)]
      (when *node-value-debug*
        (println (nodevalstr this type output " (override node)")))
      (cond
        (= :_node-id output)
        node-id

        (or (= :_declared-properties output)
            (= :_properties output))
        (let [beh           (behavior type output)
              props         ((:fn beh) this evaluation-context)
              original      (gt/node-by-id-at basis original-id)
              orig-props    (:properties (gt/produce-value original output evaluation-context))]
          (when-not (:dry-run evaluation-context)
            (let [static-props  (all-properties type)
                  props         (reduce-kv (fn [p k v]
                                             (if (and (not (contains? static-props k))
                                                      (= original-id (:node-id v)))
                                               (assoc-in p [:properties k :value] (:value v))
                                               p))
                                           props orig-props)]
              (reduce (fn [props [k v]]
                        (let [prop-type (get-in props [:properties k :type])]
                          (if (nil? (s/check (prop-type->schema prop-type) v))
                            (cond-> props
                              (and (= :_properties output)
                                   (not (contains? static-props k)))
                              (assoc-in [:properties k :value] v)

                              (contains? orig-props k)
                              (assoc-in [:properties k :original-value]
                                        (get-in orig-props [k :value])))
                            props)))
                      props properties))))

        (or (has-output? type output)
            (has-input? type output))
        (let [beh (behavior type output)]
          ((:fn beh) this evaluation-context))

        true
        (if (contains? (all-properties type) output)
          (get properties output)
          (node-value* (gt/node-by-id-at basis original-id) output evaluation-context)))))

  gt/OverrideNode
  (clear-property [this basis property] (update this :properties dissoc property))
  (override-id    [this]                override-id)
  (original       [this]                original-id)
  (set-original   [this original-id]    (assoc this :original-id original-id)))

(defn make-override-node [override-id node-id original-id properties]
  (->OverrideNode override-id node-id original-id properties))

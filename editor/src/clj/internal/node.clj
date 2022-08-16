;; Copyright 2020-2022 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.node
  (:require [clojure.pprint :as pp]
            [clojure.set :as set]
            [clojure.string :as str]
            [internal.util :as util]
            [internal.cache :as c]
            [internal.graph.types :as gt]
            [internal.graph.error-values :as ie]
            [plumbing.core :as pc]
            [schema.core :as s])
  (:import [internal.graph.error_values ErrorValue]
           [schema.core Maybe ConditionalSchema]
           [java.lang.ref WeakReference]))

(set! *warn-on-reflection* true)

(def ^:dynamic *check-schemas* (get *compiler-options* :defold/check-schemas true))

(defn trace-expr [node-id label evaluation-context label-type deferred-expr]
  (if-let [tracer (:tracer evaluation-context)]
    (do
      (tracer :begin node-id label-type label)
      (let [[result e] (try [(deferred-expr) nil] (catch Exception e [nil e]))]
        (if e
          (do (tracer :fail node-id label-type label)
              (throw e))
          (do (tracer :end node-id label-type label)
              result))))
    (deferred-expr)))

(defn- with-tracer-calls-form [node-id-sym label-sym evaluation-context-sym label-type expr]
  `(trace-expr ~node-id-sym ~label-sym ~evaluation-context-sym ~label-type
               (fn [] ~expr)))

(defn- check-dry-run-form
  ([evaluation-context-sym expr]
   `(when-not (:dry-run ~evaluation-context-sym) ~expr))
  ([evaluation-context-sym expr dry-expr]
   `(if-not (:dry-run ~evaluation-context-sym) ~expr ~dry-expr)))

(prefer-method pp/code-dispatch clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method pp/simple-dispatch clojure.lang.IPersistentMap clojure.lang.IDeref)

(defprotocol Ref
  (ref-key [this]))

(defn ref? [x] (and x (extends? Ref (class x))))

(defprotocol Type)

(defn- type? [x] (and (extends? Type (class x)) x))
(defn- named? [x] (instance? clojure.lang.Named x))

;;; ----------------------------------------
;;; Node type definition
(declare node-type-resolve value-type-resolve)

(defn- has-flag? [flag entry]
  (contains? (:flags entry) flag))

(def ^:private cached?             (partial has-flag? :cached))
(def ^:private cascade-deletes?    (partial has-flag? :cascade-delete))
(def ^:private unjammable?         (partial has-flag? :unjammable))
(def ^:private explicit?           (partial has-flag? :explicit))

(defn- filterm [pred m]
  (into {} (filter pred) m))

(defprotocol NodeType)

(defn- node-type? [x] (satisfies? NodeType x))

(defrecord NodeTypeRef [k]
  Ref
  (ref-key [this] k)

  clojure.lang.IDeref
  (deref [this]
    (node-type-resolve k)))

(defn isa-node-type? [t]
  (instance? NodeTypeRef t))

(defrecord NodeTypeImpl [name supertypes output input property input-dependencies property-display-order cascade-deletes behavior property-behavior declared-property]
  NodeType
  Type)

;;; accessors for node type information
(defn type-name                [nt]        (some-> nt deref :name))
(defn supertypes               [nt]        (some-> nt deref :supertypes))
(defn declared-outputs         [nt]        (some-> nt deref :output))
(defn declared-inputs          [nt]        (some-> nt deref :input))
(defn all-properties           [nt]        (some-> nt deref :property))
(defn input-dependencies       [nt]        (some-> nt deref :input-dependencies))
(defn property-display-order   [nt]        (some-> nt deref :property-display-order))
(defn cascade-deletes          [nt]        (some-> nt deref :cascade-deletes))
(defn behavior                 [nt label]  (some-> nt deref (get-in [:behavior label])))
(defn property-behavior        [nt label]  (some-> nt deref (get-in [:property-behavior label])))
(defn declared-property-labels [nt]        (some-> nt deref :declared-property))

(defn cached-outputs           [nt]        (some-> nt deref :output (->> (filterm #(cached? (val %))) util/key-set)))
(defn substitute-for           [nt label]  (some-> nt deref (get-in [:input label :options :substitute])))
(defn input-type               [nt label]  (some-> nt deref (get-in [:input label :value-type])))
(defn input-cardinality        [nt label]  (if (has-flag? :array (get-in (deref nt) [:input label])) :many :one))
(defn output-type              [nt label]  (some-> nt deref (get-in [:output label :value-type])))
(defn output-arguments         [nt label]  (some-> nt deref (get-in [:output label :arguments])))
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
        (keep (fn [[label outdef]]
                (when-not (unjammable? outdef)
                  label)))
        (declared-outputs nt)))

;;; ----------------------------------------
;;; Registry of node types

(defn- type-resolve
  [reg k]
  (loop [type-name k]
    (cond
      (named? type-name) (recur (get reg type-name))
      (type? type-name) type-name
      (ref? type-name) (recur (get reg (ref-key type-name)))
      (symbol? type-name) (recur (util/vgr type-name))
      (util/protocol? type-name) type-name
      (class? type-name) type-name
      :else nil)))

(defn- register-type
  [reg-ref k type]
  (assert (named? k))
  (assert (or (type? type) (named? type) (util/schema? type) (util/protocol? type) (class? type)) (pr-str k type))
  (swap! reg-ref assoc k type)
  k)

(defn- unregister-type
  [reg-ref k]
  (swap! reg-ref dissoc k))

(defonce ^:private node-type-registry-ref (atom {}))

(defn- node-type-registry [] @node-type-registry-ref)

(defn- node-type-resolve [k] (type-resolve (node-type-registry) k))

(defn- node-type [x]
  (cond
    (type? x) x
    (named? x) (or (node-type-resolve x) (throw (Exception. (str "Unable to resolve node type: " (pr-str x)))))
    :else (throw (Exception. (str (pr-str x) " is not a node type")))))

(defn register-node-type
  [k node-type]
  (assert (node-type? node-type))
  (->NodeTypeRef (register-type node-type-registry-ref k node-type)))

;;; ----------------------------------------
;;; Value type definition

(defprotocol ValueType
  (dispatch-value [this])
  (schema [this] "Returns a schema.core/Schema that can conform values of this type"))

(defrecord SchemaType [dispatch-value schema]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema [_] schema)

  Type)

(defrecord ClassType [dispatch-value ^Class class]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema [_] class)

  Type)

(defrecord ProtocolType [dispatch-value schema]
  ValueType
  (dispatch-value [_] dispatch-value)
  (schema [_] schema)

  Type)

(defn value-type? [x] (satisfies? ValueType x))

;;; ----------------------------------------
;;; Registry of value types

(defonce ^:private value-type-registry-ref (atom {}))

(defn- value-type-registry [] @value-type-registry-ref)

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
      (class? body) (->ClassType dispatch-value body)
      (util/protocol? body) (make-protocol-value-type dispatch-value name)
      (util/schema? body) (->SchemaType dispatch-value body))))

(defn unregister-value-type
  [k]
  (unregister-type value-type-registry-ref k))

(defn value-type-resolve [k]
  (type-resolve (value-type-registry) k))

(defn value-type-schema [value-type-ref] (when (ref? value-type-ref) (some-> value-type-ref deref schema)))
(defn value-type-dispatch-value [value-type-ref] (when (ref? value-type-ref) (some-> value-type-ref deref dispatch-value)))

;;; ----------------------------------------
;;; Construction support

(defrecord NodeImpl [node-type]
  gt/Node
  (node-id [this]
    (:_node-id this))

  (node-type [_]
    node-type)

  (get-property [this basis property]
    (get this property))

  (set-property [this basis property value]
    (assert (contains? (-> node-type all-properties) property)
            (format "Attempting to use property %s from %s, but it does not exist"
                    property (:name @node-type)))
    (assoc this property value))

  (overridden-properties [this] {})
  (property-overridden?  [this property] false)

  gt/Evaluation
  (produce-value [this label evaluation-context]
    (let [beh (behavior node-type label)]
      (assert beh (str "No such output, input, or property " label
                       " exists for node " (gt/node-id this)
                       " of type " (:name @node-type)
                       "\nIn production: " (get evaluation-context :in-production)))
      ((:fn beh) this label evaluation-context)))

  gt/OverrideNode
  (clear-property [this basis property]
    (throw (ex-info (str "Not possible to clear property " property
                         " of node type " (:name node-type)
                         " since the node is not an override")
                    {:label property :node-type node-type})))

  (original [this]
    nil)

  (set-original [this original-id]
    (throw (ex-info "Originals can't be changed for original nodes" {})))

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

(defn- without [s exclusions] (reduce disj s exclusions))

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

(defn- validate-evaluation-context-options [options]
  ;; :dry-run means no production functions will be called, useful speedup when tracing dependencies
  ;; :no-local-temp disables the non deterministic local caching of non :cached outputs, useful for stable results when debugging dependencies
  (assert (every? #{:basis :cache :dry-run :initial-invalidate-counters :no-local-temp :tracer :tx-data-context} (keys options)) (str (keys options)))
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
                 :local (atom {})
                 :hits (atom [])
                 :in-production #{})

          (not (:no-local-temp options))
          (assoc :local-temp (atom {}))

          (not (contains? options :tx-data-context))
          (assoc :tx-data-context (atom {}))))

(defn pruned-evaluation-context
  "Selectively filters out cache entries from the supplied evaluation context.
  Returns a new evaluation context with only the cache entries that passed the
  cache-entry-pred predicate. The predicate function will be called with
  node-id, output-label, evaluation-context and should return true if the
  cache entry for the output-label should remain in the cache."
  [evaluation-context cache-entry-pred]
  (let [unfiltered-hits @(:hits evaluation-context)
        unfiltered-local @(:local evaluation-context)
        filtered-hits (into []
                            (filter (fn [endpoint]
                                      (cache-entry-pred (gt/endpoint-node-id endpoint)
                                                        (gt/endpoint-label endpoint)
                                                        evaluation-context)))
                            unfiltered-hits)
        filtered-local (into {}
                             (filter (fn [e]
                                       (let [endpoint (key e)]
                                         (cache-entry-pred (gt/endpoint-node-id endpoint)
                                                           (gt/endpoint-label endpoint)
                                                           evaluation-context))))
                             unfiltered-local)]
    (assoc evaluation-context
      :hits (atom filtered-hits)
      :local (atom filtered-local))))

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
  [node-id label evaluation-context]
  (validate-evaluation-context evaluation-context)
  (when (some? node-id)
    (let [basis (:basis evaluation-context)
          node (gt/node-by-id-at basis node-id)]
      (gt/produce-value node label (apply-dry-run-cache evaluation-context)))))

(defn node-property-value* [node label evaluation-context]
  (validate-evaluation-context evaluation-context)
  (let [node-type (gt/node-type node)]
    (when-let [behavior (property-behavior node-type label)]
      ((:fn behavior) node label evaluation-context))))

(def ^:dynamic *suppress-schema-warnings* false)

(defn warn-output-schema [node-id label node-type-name value output-schema error]
  (when-not *suppress-schema-warnings*
    (println "Schema validation failed for node " node-id "(" node-type-name " ) label " label)
    (println "Output value:" value)
    (println "Should match:" (s/explain output-schema))
    (println "But:" error)))

;;; ----------------------------------------
;; Type checking

(def ^:private nothing-schema (s/pred (constantly false)))

(defn- prop-type->schema [prop-type]
  (cond
    (util/protocol? prop-type)
    (s/protocol prop-type)

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
            input-schema (value-type-schema input-typeref)
            out-t-pl? (coll? output-schema)
            in-t-pl?  (coll? input-schema)]
        (or
          (= s/Any input-schema)
          (and out-t-pl? (= [s/Any] input-schema))
          (and (= out-t-pl? in-t-pl? true) (check-single-type (first output-schema) (first input-schema)))
          (and (= out-t-pl? in-t-pl? false) (check-single-type output-schema input-schema))
          (and (instance? Maybe input-schema) (type-compatible? output-schema (:schema input-schema)))
          (and (instance? ConditionalSchema input-schema) (some #(type-compatible? output-schema %) (map second (:preds-and-schemas input-schema))))))))

;;; ----------------------------------------
;;; Node type implementation

(defn- alias-of [ns s]
  (get (ns-aliases ns) s))

(defn- localize
  ([ctor s] (ctor (str *ns*) s))
  ([ctor n s] (ctor n s)))

(defn- canonicalize [x]
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

(defn- join-display-groups
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

(defn- node-type-name? [x]
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
          left order1
          right order2]
     (if-let [elem (first left)]
       (let [elem (if (node-type-name? elem) (canonicalize elem) elem)]
         (cond
           (ref? elem)
           (recur result (concat (property-display-order elem) (next left)) right)

           (keyword? elem)
           (recur (conj result elem) (next left) (remove #{elem} right))

           (sequential? elem)
           (let [header (first elem)
                 group (next elem)]
             (if (some ref? elem)
               (recur result (cons (expand-node-types elem) (next left)) right)
               (let [group-label header
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
  (set/union #{:_this}
             (util/key-set (:input description))
             (util/key-set (:property description))
             (util/key-set (:output description))))

(defn- verify-inputs-for-dynamics
  [description]
  (let [available-arguments (all-available-arguments description)]
    (doseq [[property-name property-type] (:property description)
            [dynamic-name {:keys [arguments]}] (:dynamics property-type)
            :let [missing-args (set/difference arguments available-arguments)]]
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
  (let [inputs (util/key-set (:input description))
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
     :_this)))

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
  (let [labels (ordinary-property-labels description)]
    (zipmap labels
            (map (fn [label]
                   {:fn (node-property-value-function-form description label)}) labels))))

(defn- attach-property-behaviors
  [description]
  (update description :property-behavior merge (transform-properties-plumbing-map description)))

(defn- transform-outputs-plumbing-map [description]
  (let [labels (ordinary-output-labels description)]
    (zipmap labels
            (map (fn [label]
                   {:fn (node-output-value-function-form description label)}) labels))))

(defn- attach-output-behaviors
  [description]
  (update description :behavior merge (transform-outputs-plumbing-map description)))

(defn- transform-inputs-plumbing-map [description]
  (let [labels (ordinary-input-labels description)]
    (zipmap labels
            (map (fn [input] {:fn (node-input-value-function-form description input)}) labels))))

(defn- attach-input-behaviors
  [description]
  (update description :behavior merge (transform-inputs-plumbing-map description)))

(defn- abstract-function-form
  [label type]
  (let [format-string (str "Node %d does not supply a production function for the abstract '" label "' output. Add (output " label " " type " your-function) to the definition")]
    `(pc/fnk [~'_this]
             (throw (AssertionError.
                      (format ~format-string
                              (gt/node-id ~'_this)))))))

(defn- parse-flags-and-options
  [allowed-flags allowed-options args]
  (loop [flags #{}
         options {}
         args args]
    (if-let [[arg & remainder] (seq args)]
      (cond
        (allowed-flags arg) (recur (conj flags arg) options remainder)
        (allowed-options arg) (do (assert remainder (str "Expected value for option " arg))
                                  (recur flags (assoc options arg (first remainder)) (rest remainder)))
        :else [flags options args])
      [flags options args])))

(defn- named->value-type-ref
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
        form (if multivalued? (first original-form) original-form)
        autotype-form (cond
                        (util/protocol-symbol? form) `(->ProtocolType ~form (s/protocol ~form))
                        (util/class-symbol? form) `(->ClassType ~form ~form))
        typeref (cond
                  (ref? form) form
                  (util/protocol-symbol? form) (named->value-type-ref form)
                  (util/class-symbol? form) (named->value-type-ref (.getName ^Class (resolve form)))
                  (named? form) (named->value-type-ref form))]
    (assert (not (nil? typeref))
            (str "defnode " where " requires a value type but was supplied with '"
                 original-form "' which cannot be used as a type"))
    (when (and (ref? typeref) (nil? autotype-form))
      (assert (not (nil? (deref typeref)))
              (str "defnode " where " requires a value type but was supplied with '"
                   original-form "' which cannot be used as a type")))
    (util/assert-form-kind "defnode" "registered value type"
                           (some-fn ref? value-type?) where typeref)
    (when autotype-form
      ;; When we build the release bundle, macroexpansion happens
      ;; during compilation. we need type information for compilation
      (register-type value-type-registry-ref (ref-key typeref)
                     (cond
                       (util/protocol-symbol? form)
                       (let [pval (util/vgr form)]
                         (make-protocol-value-type pval form))

                       ;; note: this occurs in type position of a defnode clause. we use
                       ;; the class itself as the dispatch value so multimethods can be
                       ;; expressed most naturally.
                       (util/class-symbol? form)
                       (let [cls (resolve form)]
                         (->ClassType cls cls)))))
    (cond-> {:value-type typeref :flags (if multivalued? #{:collection} #{})}
            ;; When we run the bundle, compilation is long past, we we
            ;; need to re-register the automatic types at runtime. defnode
            ;; emits code to do that, based on the types we collect here
            (some? autotype-form)
            (assoc :register-type-info {(ref-key typeref) autotype-form}))))

(defn- macro-expression?
  [form]
  (when-let [term (and (seq? form) (symbol? (first form)) (first form))]
    (let [v (resolve term)]
      (and (var? v) (.isMacro ^clojure.lang.Var v)))))

(defn- fnk-expression?
  [form]
  (when-let [term (and (seq? form) (symbol? (first form)) (first form))]
    (or (= "#'dynamo.graph/fnk" (str (resolve term))))))

(defn- maybe-macroexpand
  [form]
  (if (and (macro-expression? form) (not (fnk-expression? form)))
    (macroexpand-1 form)
    form))

(def ^:private node-intrinsics
  [(list 'property '_node-id :dynamo.graph/NodeID :unjammable)
   (list 'property '_output-jammers :dynamo.graph/KeywordMap :unjammable)
   (list 'output '_properties :dynamo.graph/Properties `(dynamo.graph/fnk [~'_declared-properties] ~'_declared-properties))
   (list 'output '_overridden-properties :dynamo.graph/KeywordMap `(dynamo.graph/fnk [~'_this] (gt/overridden-properties ~'_this)))])

(def ^:private intrinsic-properties #{:_node-id :_output-jammers})

(defn- maybe-inject-intrinsics
  [forms]
  (if (some #(= 'inherits %) (map first forms))
    forms
    (concat node-intrinsics forms)))

(defmulti process-property-form first)

(defmethod process-property-form 'dynamic [[_ label forms]]
  (assert-symbol "dynamic" label) ; "dynamic" argument is for debug printing
  {:dynamics {(keyword label) {:fn (maybe-macroexpand forms)}}})

(defmethod process-property-form 'value [[_ form]]
  {:value {:fn (maybe-macroexpand form)}})

(defmethod process-property-form 'set [[_ form]]
  {:setter {:fn (maybe-macroexpand form)}})

(defmethod process-property-form 'default [[_ form]]
  {:default {:fn (maybe-macroexpand form)}})

(def ^:private allowed-property-flags #{:unjammable})
(def ^:private allowed-property-options #{})

(defn- process-property-forms
  [[type-form & forms]]
  (let [type-info (parse-type-form "property" type-form)
        [flags options body-forms] (parse-flags-and-options allowed-property-flags allowed-property-options forms)]
    (apply merge-with into
           type-info
           {:flags flags :options options}
           (for [b body-forms]
             (process-property-form b)))))

(defmulti process-as first)

(defmethod process-as 'property [[_ label & forms]]
  (assert-symbol "property" label)
  (let [klabel (keyword label)
        propdef (process-property-forms forms)
        register-type-info (:register-type-info propdef)
        propdef (dissoc propdef :register-type-info)
        prop-value-fn (-> propdef :value :fn)
        outdef (cond-> (dissoc propdef :setter :dynamics :value :default)

                       (some? prop-value-fn)
                       (assoc :fn prop-value-fn)

                       (nil? prop-value-fn)
                       (assoc :fn ::default-fn :default-fn-label klabel))
        desc {:register-type-info register-type-info
              :property {klabel propdef}
              :property-order-decl (if (contains? intrinsic-properties klabel) [] [klabel])
              :output {klabel outdef}}]
    desc))

(def ^:private allowed-output-flags #{:cached :abstract :unjammable})
(def ^:private allowed-output-options #{})

(defmethod process-as 'output [[_ label & forms]]
  (assert-symbol "output" label)
  (let [type-form (first forms)
        base (parse-type-form "output" type-form)
        register-type-info (:register-type-info base)
        base (dissoc base :register-type-info)
        [flags options fn-forms] (parse-flags-and-options allowed-output-flags allowed-output-options (rest forms))
        abstract?                (contains? flags :abstract)]
    (assert (or abstract? (first fn-forms))
            (format "Output %s has no production function and is not abstract" label))
    (assert (not (next fn-forms))
            (format "Output %s seems to have something after the production function: " label (next fn-forms)))
    {:register-type-info register-type-info
     :output
     {(keyword label)
      (merge-with into base {:flags (into #{} (conj flags :explicit))
                             :options options
                             :fn (if abstract?
                                   (abstract-function-form label type-form)
                                   (maybe-macroexpand (first fn-forms)))})}}))

(def ^:private input-flags #{:array :cascade-delete})
(def ^:private input-options #{:substitute})

(defmethod process-as 'input [[_ label & forms]]
  (assert-symbol "input" label)
  (let [type-form (first forms)
        base (parse-type-form "input" type-form)
        register-type-info (:register-type-info base)
        base (dissoc base :register-type-info)
        [flags options _] (parse-flags-and-options input-flags input-options (rest forms))]
    {:register-type-info register-type-info
     :input
     {(keyword label)
      (merge-with into base {:flags flags :options options})}}))

(defmethod process-as 'inherits [[_ & forms]]
  {:supertypes
   (for [form forms]
     (do
       (assert-symbol "inherits" form)
       (let [typeref (util/vgr form)]
         (assert (node-type-resolve typeref)
                 (str "Cannot inherit from " form " it cannot be resolved in this context (from namespace " *ns* ")."))
         typeref)))})

(defmethod process-as 'display-order [[_ decl]]
  {:display-order-decl decl})

(defn- group-node-type-forms
  [forms]
  (let [parse (for [form forms :when (seq? form)]
                (process-as form))]
    (apply merge-with into parse)))

(defn- node-type-merge
  ([] {})
  ([l] l)
  ([l r]
   {:property (merge-with merge (:property l) (:property r))
    :input (merge-with merge (:input l) (:input r))
    :output (merge-with merge (:output l) (:output r))
    :supertypes (into (:supertypes l) (:supertypes r))
    :register-type-info (into (or (:register-type-info l) {}) (:register-type-info r))
    ;; Display order gets resolved at runtime
    :property-order-decl (:property-order-decl r)
    :display-order-decl (:display-order-decl r)})
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

(defn- update-fn-maps [tree f]
  (-> tree
      (update :output (partial util/map-vals f))
      (update :property (fn [properties]
                          (into {}
                                (map (fn [[property-label propdef]]
                                       [property-label (cond-> propdef
                                                               (some? (-> propdef :value :fn))
                                                               (update :value f)

                                                               (some? (-> propdef :default :fn))
                                                               (update :default f)

                                                               (some? (-> propdef :dynamics))
                                                               (update :dynamics (partial util/map-vals f)))]))
                                properties)))))

(defn- wrap-constant-fn? [fn]
  (not (or (seq? fn) (util/pfnksymbol? fn) (util/pfnkvar? fn) (= fn ::default-fn))))

(defn- wrap-constant-fn [v]
  `(dynamo.graph/fnk [] ~(if (symbol? v) (resolve v) v)))

(defn- maybe-wrap-constant-fn [fn]
  (if (wrap-constant-fn? fn)
    (wrap-constant-fn fn)
    fn))

(defn- wrap-constant-fns [tree]
  (update-fn-maps tree #(update % :fn maybe-wrap-constant-fn)))

(defn- into-set [x v] (into (or x #{}) v))

(defn- extract-fn-arguments [tree]
  (update-fn-maps tree
                  (fn [fn-map]
                    (let [fnf (:fn fn-map)
                          default-label (:default-fn-label fn-map)
                          args (if (= fnf ::default-fn)
                                 #{default-label}
                                 (util/inputs-needed fnf))]
                      (-> fn-map
                          (update :arguments #(into-set % args))
                          (update :dependencies #(into-set % args)))))))

(defn- prop+args [[pname pdef]]
  (into #{pname} (:arguments pdef)))

(defn- attach-declared-properties-output
  [description]
  (let [publics (apply disj (reduce into #{} (map prop+args (:property description))) intrinsic-properties)]
    (assoc-in description [:output :_declared-properties]
              {:value-type (->ValueTypeRef :dynamo.graph/Properties)
               :flags #{:cached}
               :arguments publics
               :dependencies publics})))

(defn- attach-declared-properties-behavior
  [description]
  (assoc-in description [:behavior :_declared-properties :fn]
            (declared-properties-function-form description)))

(defn- attach-cascade-deletes
  [{:keys [input] :as description}]
  (assoc description :cascade-deletes (into #{} (comp (filter (comp cascade-deletes? val)) (map key)) input)))

(defn- attach-declared-property
  [{:keys [property] :as description}]
  (assoc description
         :declared-property (into #{}
                                  (comp (map key)
                                        (remove intrinsic-properties))
                                  property)))

(defn- all-subtree-dependencies
  [tree]
  (into #{}
        (mapcat :dependencies)
        (tree-seq map? vals tree)))

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
  [fully-qualified-node-type-symbol forms]
  (-> (maybe-inject-intrinsics forms)
      group-node-type-forms
      merge-supertypes
      (assoc :name (str fully-qualified-node-type-symbol))
      (assoc :key (keyword fully-qualified-node-type-symbol))
      wrap-constant-fns
      defer-display-order-resolution
      extract-fn-arguments
      merge-property-dependencies
      apply-property-dependencies-to-outputs
      attach-declared-properties-output
      attach-input-dependencies
      attach-property-behaviors
      attach-output-behaviors
      attach-input-behaviors
      attach-declared-properties-behavior
      attach-cascade-deletes
      attach-declared-property
      verify-inputs-for-dynamics
      verify-inputs-for-outputs
      verify-labels))

(defn dollar-name [prefix path]
  (->> path
       (map name)
       (interpose "$")
       (apply str prefix "$")
       symbol))

(defn- should-def-fn? [f]
  (and f
       (not= f ::default-fn)
       (not (var? f))))

(defn extract-def-fns [node-type-def]
  (let [output-defs (keep (fn [[output-label {:keys [fn] :as _outdef}]]
                            (when (should-def-fn? fn)
                              [[:output output-label] fn]))
                          (:output node-type-def))
        prop-defs (reduce (fn [prop-defs [property-label prop-def]]
                            (cond-> prop-defs
                                    (should-def-fn? (-> prop-def :value :fn))
                                    (update :value conj [[:property property-label :value] (-> prop-def :value :fn)])

                                    (should-def-fn? (-> prop-def :default :fn))
                                    (update :default conj [[:property property-label :default] (-> prop-def :default :fn)])

                                    (some? (:dynamics prop-def))
                                    (update :dynamics into (keep (fn [[dynamic-label {:keys [fn] :as _dyndef}]]
                                                                   (when (should-def-fn? fn)
                                                                     [[:property property-label :dynamics dynamic-label] fn]))
                                                                 (:dynamics prop-def)))

                                    (should-def-fn? (-> prop-def :setter :fn))
                                    (update :setter conj [[:property property-label :setter] (-> prop-def :setter :fn)])))
                          {:value []
                           :default []
                           :dynamics []
                           :setter []}
                          (:property node-type-def))
        behaviors (keep (fn [[output-label {:keys [fn] :as _output-behavior}]]
                          (when (should-def-fn? fn)
                            [[:behavior output-label] fn]))
                        (:behavior node-type-def))
        property-behaviors (keep (fn [[property-label {:keys [fn] :as _property-behavior}]]
                                   (when (should-def-fn? fn)
                                     [[:property-behavior property-label] fn]))
                                 (:property-behavior node-type-def))]
    (concat output-defs
            (:value prop-defs)
            (:default prop-defs)
            (:dynamics prop-defs)
            (:setter prop-defs)
            behaviors
            property-behaviors)))

;;; ----------------------------------------
;;; Code generation

(declare fnk-argument-form)

(defn- desc-has-input? [description label] (contains? (:input description) label))
(defn- desc-has-property? [description label] (contains? (:property description) label))
(defn- desc-has-output? [description label] (contains? (:output description) label))
(defn- desc-has-explicit-output? [description label]
  (contains? (get-in description [:output label :flags]) :explicit))

(defn- desc-has-multivalued-input? [description input-label]
  (contains? (get-in description [:input input-label :flags]) :array))

(defn- desc-has-singlevalued-input? [description input-label]
  (and (desc-has-input? description input-label)
       (not (desc-has-multivalued-input? description input-label))))

(defn- desc-input-substitute [description input-label not-found]
  (get-in description [:input input-label :options :substitute] not-found))

(defn pull-first-input-value
  [node input-label evaluation-context]
  (let [basis (:basis evaluation-context)
        [upstream-id output-label] (first (gt/sources basis (gt/node-id node) input-label))]
    (when-let [upstream-node (and upstream-id (gt/node-by-id-at basis upstream-id))]
      (gt/produce-value upstream-node output-label evaluation-context))))

(defn pull-first-input-with-substitute
  [sub node input-label evaluation-context]
  ;; todo - invoke substitute
  (pull-first-input-value node input-label evaluation-context))

(defn- first-input-value-form
  [node-sym input-label evaluation-context-sym]
  `(pull-first-input-value ~node-sym ~input-label ~evaluation-context-sym))

(defn pull-input-values
  [node input-label evaluation-context]
  (let [basis (:basis evaluation-context)]
    (mapv (fn [[upstream-id output-label]]
            (let [upstream-node (gt/node-by-id-at basis upstream-id)]
              (gt/produce-value upstream-node output-label evaluation-context)))
          (gt/sources basis (gt/node-id node) input-label))))

(defn pull-input-values-with-substitute
  [sub node input-label evaluation-context]
  ;; todo - invoke substitute
  (pull-input-values node input-label evaluation-context))

(defn- input-value-form
  [node-sym input-label evaluation-context-sym]
  `(pull-input-values ~node-sym ~input-label ~evaluation-context-sym))

(defn error-checked-input-value [node-id input-label input-value]
  (if (instance? ErrorValue input-value)
    (if (ie/worse-than :info input-value)
      (ie/error-aggregate [input-value] :_node-id node-id :_label input-label)
      (:value input-value))
    input-value))

(defn error-substituted-input-value [input-value substitute-fn]
  (if (instance? ErrorValue input-value)
    (if (ie/worse-than :info input-value)
      (util/apply-if-fn substitute-fn input-value)
      (:value input-value))
    input-value))

(defn error-checked-array-input-value [node-id input-label input-array]
  (if (some #(instance? ErrorValue %) input-array)
    (let [serious-errors (filter #(ie/worse-than :info %) input-array)]
      (if (empty? serious-errors)
        (mapv #(if (instance? ErrorValue %) (:value %) %) input-array)
        (ie/error-aggregate serious-errors :_node-id node-id :_label input-label)))
    input-array))

(defn error-substituted-array-input-value [input-array substitute-fn]
  (if (some #(instance? ErrorValue %) input-array)
    (let [serious-errors (filter #(ie/worse-than :info %) input-array)]
      (if (empty? serious-errors)
        (mapv #(if (instance? ErrorValue %) (:value %) %) input-array)
        (util/apply-if-fn substitute-fn input-array)))
    input-array))

(defn argument-error-aggregate [node-id label input-value]
  (when-some [input-errors (not-empty (filter #(instance? ErrorValue %) (vals input-value)))]
    (ie/error-aggregate input-errors :_node-id node-id :_label label)))

(defn- call-with-error-checked-fnky-arguments-form
  [description label node-sym node-id-sym evaluation-context-sym arguments runtime-fnk-expr & [supplied-arguments]]
  (let [base-args {:_node-id `(gt/node-id ~node-sym)}
        arglist (without arguments (keys supplied-arguments))
        argument-forms (zipmap arglist (map #(get base-args % (if (= label %)
                                                                `(gt/get-property ~node-sym (:basis ~evaluation-context-sym) ~label)
                                                                (fnk-argument-form description label % node-sym node-id-sym evaluation-context-sym)))
                                            arglist))
        argument-forms (merge argument-forms supplied-arguments)]
    (if (empty? argument-forms)
      `(~runtime-fnk-expr {})
      `(let [arguments# ~argument-forms]
         (or (argument-error-aggregate ~node-id-sym ~label arguments#)
             (~runtime-fnk-expr arguments#))))))

(defn- collect-property-value-form
  [description property-label node-sym node-id-sym evaluation-context-sym]
  (let [property-definition (get-in description [:property property-label])
        default? (not (:value property-definition))
        output-fn (get-in description [:property property-label :value :fn])]
    (if default?
      (with-tracer-calls-form node-id-sym property-label evaluation-context-sym :raw-property
        (check-dry-run-form evaluation-context-sym `(gt/get-property ~node-sym (:basis ~evaluation-context-sym) ~property-label)))
      (with-tracer-calls-form node-id-sym property-label evaluation-context-sym :property
        (call-with-error-checked-fnky-arguments-form description property-label node-sym node-id-sym evaluation-context-sym
                                                     (get-in property-definition [:value :arguments])
                                                     (check-dry-run-form evaluation-context-sym
                                                                         (if (var? output-fn)
                                                                           output-fn
                                                                           `(var ~(dollar-name (:name description) [:property property-label :value])))
                                                                         `(constantly nil)))))))

(defn- fnk-argument-form
  [description output argument node-sym node-id-sym evaluation-context-sym]
  (cond
    (= :_this argument)
    node-sym

    (and (= output argument)
         (desc-has-property? description argument)
         (desc-has-explicit-output? description output))
    (collect-property-value-form description argument node-sym node-id-sym evaluation-context-sym)

    (and (not= argument output) (desc-has-output? description argument))
    `(gt/produce-value ~node-sym ~argument ~evaluation-context-sym)

    (desc-has-property? description argument)
    (if (= output argument)
      (with-tracer-calls-form node-id-sym argument evaluation-context-sym :raw-property
        (check-dry-run-form evaluation-context-sym `(gt/get-property ~node-sym (:basis ~evaluation-context-sym) ~argument)))
      (collect-property-value-form description argument node-sym node-id-sym evaluation-context-sym))

    (desc-has-multivalued-input? description argument)
    (let [sub (desc-input-substitute description argument ::no-sub)] ; nil is a valid substitute literal
      (if (= ::no-sub sub)
        `(error-checked-array-input-value ~node-id-sym ~argument ~(input-value-form node-sym argument evaluation-context-sym))
        `(error-substituted-array-input-value ~(input-value-form node-sym argument evaluation-context-sym) ~sub)))

    (desc-has-singlevalued-input? description argument)
    (let [sub (desc-input-substitute description argument ::no-sub)] ; nil is a valid substitute literal
      (if (= ::no-sub sub)
        `(error-checked-input-value ~node-id-sym ~argument ~(first-input-value-form node-sym argument evaluation-context-sym))
        `(error-substituted-input-value ~(first-input-value-form node-sym argument evaluation-context-sym) ~sub)))

    (desc-has-output? description argument)
    `(gt/produce-value ~node-sym ~argument ~evaluation-context-sym)

    :else
    (assert false (str "A production function for " (:name description) " " output " needs an argument this node can't supply. There is no input, output, or property called " (pr-str argument)))))

(defn- original-root [node-id basis]
  (let [node (gt/node-by-id-at basis node-id)
        orig-id (:original-id node)]
    (if orig-id
      (recur orig-id basis)
      node-id)))

(defn output-jammer [node node-id label basis]
  (let [original (if (:original-id node)
                   (gt/node-by-id-at basis (original-root node-id basis))
                   node)]
    (when-some [jam-value (get (:_output-jammers original) label)]
      (if (ie/error? jam-value)
        (assoc jam-value :_label label :_node-id node-id)
        jam-value))))

(defn- check-jammed-form [description label node-sym node-id-sym label-sym evaluation-context-sym forms]
  (if (unjammable? (get-in description [:output label]))
    forms
    `(or (output-jammer ~node-sym ~node-id-sym ~label-sym (:basis ~evaluation-context-sym))
         ~forms)))

(defn- property-has-default-getter? [description label] (not (get-in description [:property label :value])))
(defn- property-has-no-overriding-output? [description label] (not (desc-has-explicit-output? description label)))

(defn- apply-default-property-shortcut-form [description label node-sym node-id-sym label-sym evaluation-context-sym forms]
  (let [default? (and (property-has-default-getter? description label)
                      (property-has-no-overriding-output? description label))]
     ; desc-has-property? this is implied if we're evaluating an output and default? holds
    (assert (or (not default?) (desc-has-property? description label)))
    (if default?
      (with-tracer-calls-form node-id-sym label-sym evaluation-context-sym :raw-property
        (check-dry-run-form evaluation-context-sym `(gt/get-property ~node-sym (:basis ~evaluation-context-sym) ~label-sym)))
      forms)))

(defn- node-type-name [node-id evaluation-context]
  (let [basis (:basis evaluation-context)
        node (gt/node-by-id-at basis node-id)]
    (type-name (gt/node-type node))))

(defn mark-in-production [node-id label evaluation-context]
  (assert (not (contains? (:in-production evaluation-context) (gt/endpoint node-id label)))
          (format "Cycle detected on node type %s and output %s"
                  (node-type-name node-id evaluation-context)
                  label))
  (update evaluation-context :in-production conj (gt/endpoint node-id label)))

(defn- mark-in-production-form [node-id-sym label-sym evaluation-context-sym forms]
  `(let [~evaluation-context-sym (mark-in-production ~node-id-sym ~label-sym ~evaluation-context-sym)]
     ~forms))

(defn cached-nil->nil [v]
  (if (= v ::cached-nil) nil v))

(defn nil->cached-nil [v]
  (if (nil? v) ::cached-nil v))

(defn check-caches! [node-id label evaluation-context]
  (let [local @(:local evaluation-context)
        global (:cache evaluation-context)
        cache-key (gt/endpoint node-id label)]
    (cond
      (contains? local cache-key)
      (trace-expr node-id label evaluation-context :cache (fn [] (nil->cached-nil (get local cache-key))))

      (contains? global cache-key)
      (trace-expr node-id label evaluation-context :cache
                   (fn []
                     (when-some [cached-result (get global cache-key)]
                       (swap! (:hits evaluation-context) conj cache-key)
                       (nil->cached-nil cached-result)))))))

(defn check-local-temp-cache [node-id label evaluation-context]
  (let [local-temp (some-> (:local-temp evaluation-context) deref)
        weak-cached-value ^WeakReference (get local-temp (gt/endpoint node-id label))]
    (and weak-cached-value (.get weak-cached-value))))

(defn- check-caches-form [description label node-id-sym label-sym evaluation-context-sym forms]
  (let [result-sym 'result]
    (if (get-in description [:output label :flags :cached])
      `(if-some [~result-sym (check-caches! ~node-id-sym ~label-sym ~evaluation-context-sym)]
         (cached-nil->nil ~result-sym)
         ~forms)
      `(if-some [~result-sym (check-local-temp-cache ~node-id-sym ~label-sym ~evaluation-context-sym)]
         ~(with-tracer-calls-form node-id-sym label-sym evaluation-context-sym :cache
            `(cached-nil->nil ~result-sym))
         ~forms))))

(defn- gather-arguments-form [description label node-sym node-id-sym evaluation-context-sym arguments-sym schema-sym forms]
  (let [arg-names (get-in description [:output label :arguments])
        argument-forms (zipmap arg-names (map #(fnk-argument-form description label % node-sym node-id-sym evaluation-context-sym) arg-names))
        argument-forms (assoc argument-forms :_node-id node-id-sym)]
    (list `let
          [arguments-sym argument-forms]
          forms)))

(defn- argument-error-check-form [description label node-id-sym label-sym evaluation-context-sym arguments-sym forms]
  (if (= :_properties label)
    forms
    `(or (argument-error-aggregate ~node-id-sym ~label-sym ~arguments-sym)
         ~forms)))

(defn- call-production-function-form [description label node-id-sym label-sym evaluation-context-sym arguments-sym result-sym forms]
  (let [output-fn (get-in description [:output label :fn])]
    `(let [~result-sym ~(argument-error-check-form description label node-id-sym label-sym evaluation-context-sym arguments-sym
                                                   (check-dry-run-form evaluation-context-sym
                                                                       (if (var? output-fn)
                                                                         `(~output-fn ~arguments-sym)
                                                                         `((var ~(symbol (dollar-name (:name description) [:output label]))) ~arguments-sym))))]
       ~forms)))

(defn update-local-cache! [node-id label evaluation-context value]
  (swap! (:local evaluation-context) assoc (gt/endpoint node-id label) value))

(defn update-local-temp-cache! [node-id label evaluation-context value]
  (when-let [local-temp (:local-temp evaluation-context)]
    (swap! local-temp assoc (gt/endpoint node-id label) (WeakReference. (nil->cached-nil value)))))

(defn- cache-result-form [description label node-id-sym label-sym evaluation-context-sym result-sym forms]
  (if (contains? (get-in description [:output label :flags]) :cached)
    `(do
       (update-local-cache! ~node-id-sym ~label-sym ~evaluation-context-sym ~result-sym)
       ~forms)
    `(do
       (update-local-temp-cache! ~node-id-sym ~label-sym ~evaluation-context-sym ~result-sym)
       ~forms)))

(defn- deduce-output-type-form
  [description label]
  (let [schema (some-> (get-in description [:output label :value-type]) value-type-schema)
        schema (if (get-in description [:output label :flags :collection])
                 (vector schema)
                 schema)]
    `(s/maybe (s/conditional ie/error-value? ErrorValue :else ~schema)))) ; allow nil's and errors

(defn schema-check-result [node-id label evaluation-context output-schema result]
  (when-not (:dry-run evaluation-context)
    (let [node-type-name (node-type-name node-id evaluation-context)]
      (try
        (when-some [validation-error (s/check output-schema result)]
          (warn-output-schema node-id label node-type-name result output-schema validation-error)
          (throw (ex-info "SCHEMA-VALIDATION"
                          {:node-id node-id
                           :type node-type-name
                           :output label
                           :expected output-schema
                           :actual result
                           :validation-error validation-error})))
        (catch IllegalArgumentException iae
          (throw (ex-info "MALFORMED-SCHEMA" {:label label :node-type node-type-name} iae)))))))

(defn- schema-check-result-form [description label node-id-sym label-sym evaluation-context-sym result-sym forms]
  (if *check-schemas*
    `(do
       (schema-check-result ~node-id-sym ~label-sym ~evaluation-context-sym ~(deduce-output-type-form description label) ~result-sym)
       ~forms)
    forms))

(defn- node-property-value-function-form [description property]
  (let [node-sym 'node
        label-sym 'label
        evaluation-context-sym 'evaluation-context
        node-id-sym 'node-id]
    `(fn [~node-sym ~label-sym ~evaluation-context-sym]
       (let [~node-id-sym (gt/node-id ~node-sym)]
         ~(collect-property-value-form description property node-sym node-id-sym evaluation-context-sym)))))

(defn- node-output-value-function-form
  [description label]
  (let [tracer-label-type (if (desc-has-explicit-output? description label) :output :property)]
    (let [node-sym 'node
          label-sym 'label
          evaluation-context-sym 'evaluation-context
          node-id-sym 'node-id
          arguments-sym 'arguments
          schema-sym 'schema
          result-sym 'result]
      `(fn [~node-sym ~label-sym ~evaluation-context-sym]
         (let [~node-id-sym (gt/node-id ~node-sym)]
           ~(check-jammed-form description label node-sym node-id-sym label-sym evaluation-context-sym
              (apply-default-property-shortcut-form description label node-sym node-id-sym label-sym evaluation-context-sym
                (mark-in-production-form node-id-sym label-sym evaluation-context-sym
                  (check-caches-form description label node-id-sym label-sym evaluation-context-sym
                    (with-tracer-calls-form node-id-sym label-sym evaluation-context-sym tracer-label-type
                      (gather-arguments-form description label node-sym node-id-sym evaluation-context-sym arguments-sym schema-sym
                        (call-production-function-form description label node-id-sym label-sym evaluation-context-sym arguments-sym result-sym
                          (schema-check-result-form description label node-id-sym label-sym evaluation-context-sym result-sym
                            (cache-result-form description label node-id-sym label-sym evaluation-context-sym result-sym
                              result-sym))))))))))))))
  
(defn- assemble-properties-map-form
  [node-id-sym value-sym display-order-sym]
  `{:properties ~value-sym
    :display-order ~display-order-sym
    :node-id ~node-id-sym})

(defn- property-dynamics
  [description property-name node-sym node-id-sym evaluation-context-sym property-type]
  (apply merge
         (for [[dynamic-label {:keys [arguments] :as dynamic}] (get property-type :dynamics)]
           (let [dynamic-fn (get-in description [:property property-name :dynamics dynamic-label :fn])]
             {dynamic-label
              (with-tracer-calls-form node-id-sym [property-name dynamic-label] evaluation-context-sym :dynamic
                ;; TODO passing dynamic-label here looks broken; what if dynamic-label == some output label?
                (call-with-error-checked-fnky-arguments-form description dynamic-label node-sym node-id-sym evaluation-context-sym
                                                             arguments
                                                             (check-dry-run-form evaluation-context-sym
                                                                                 (if (var? dynamic-fn)
                                                                                   dynamic-fn
                                                                                   `(var ~(dollar-name (:name description) [:property property-name :dynamics dynamic-label])))
                                                                                 `(constantly nil))))}))))

(defn- property-value-exprs
  [description property-label node-sym node-id-sym evaluation-context-sym prop-type]
  (let [basic-val `{:type ~(:value-type prop-type)
                    :value ~(collect-property-value-form description property-label node-sym node-id-sym evaluation-context-sym)
                    :node-id ~node-id-sym}]
    (if (not (empty? (:dynamics prop-type)))
      (let [dyn-exprs (property-dynamics description property-label node-sym node-id-sym evaluation-context-sym prop-type)]
        (merge basic-val dyn-exprs))
      basic-val)))

(defn- declared-properties-function-form
  [description]
  (let [props (:property description)]
    (let [node-sym 'node
          label-sym 'label
          evaluation-context-sym 'evaluation-context
          value-map-sym 'value-map
          node-id-sym 'node-id
          display-order-sym 'display-order]
      `(fn [~node-sym ~label-sym ~evaluation-context-sym]
         (let [~node-id-sym (gt/node-id ~node-sym)
               ~display-order-sym (property-display-order (gt/node-type ~node-sym))
               ~value-map-sym ~(apply merge {}
                                      (for [[p _] (remove (comp intrinsic-properties key) props)]
                                        {p (property-value-exprs description p node-sym node-id-sym evaluation-context-sym (get props p))}))]
           ~(check-dry-run-form evaluation-context-sym (assemble-properties-map-form node-id-sym value-map-sym display-order-sym)))))))

(defn- node-input-value-function-form
  [description input]
  (let [sub (desc-input-substitute description input ::no-sub)
        sub? (not= sub ::no-sub)
        multi? (desc-has-multivalued-input? description input)]
    (cond
      (and (not sub?) (not multi?))
      `pull-first-input-value

      (and (not sub?) multi?)
      `pull-input-values

      (not multi?)
      `(partial pull-first-input-with-substitute ~sub)

      :else
      `(partial pull-input-values-with-substitute ~sub))))

;;; ----------------------------------------
;;; Overrides

(defrecord OverrideNode [override-id node-id node-type original-id properties]
  gt/Node
  (node-id [this] node-id)
  (node-type [this] node-type)
  (get-property [this basis property]
    (let [value (get properties property ::not-found)]
      (if (identical? ::not-found value)
        (gt/get-property (gt/node-by-id-at basis original-id) basis property)
        value)))
  (set-property [this basis property value]
    (if (= :_output-jammers property)
      (throw (ex-info "Not possible to mark override nodes as defective" {}))
      (assoc-in this [:properties property] value)))
  (overridden-properties [this] properties)
  (property-overridden?  [this property] (contains? properties property))

  gt/Evaluation
  (produce-value [this output evaluation-context]
    (let [basis (:basis evaluation-context)]
      (cond
        (= :_node-id output)
        node-id

        (or (= :_declared-properties output)
            (= :_properties output))
        (let [beh (behavior node-type output)
              props ((:fn beh) this output evaluation-context)
              original (gt/node-by-id-at basis original-id)
              orig-props (:properties (gt/produce-value original output evaluation-context))
              declared? (partial contains? (all-properties node-type))]
          (when-not (:dry-run evaluation-context)
            (let [;; Values for undeclared properties must be manually propagated from the original.
                  props-with-inherited-override-values
                  (if (= :_declared-properties output)
                    (:properties props)
                    (reduce-kv (fn [props prop-kw orig-prop]
                                 (if (and (::propagate? orig-prop)
                                          (= original-id (:node-id orig-prop))
                                          (not (declared? prop-kw)))
                                   (update props prop-kw assoc :value (:value orig-prop) ::propagate? true)
                                   props))
                               (:properties props)
                               orig-props))

                  ;; Overrides of undeclared properties must be manually applied.
                  props-with-override-values
                  (if (= :_declared-properties output)
                    props-with-inherited-override-values
                    (reduce-kv (fn [props prop-kw override-value]
                                 (if (declared? prop-kw)
                                   props
                                   (let [prop-type (get-in props [prop-kw :type])
                                         prop-schema (prop-type->schema prop-type)
                                         compatible-value? (comp nil? (partial s/check prop-schema))]
                                     (if (compatible-value? override-value)
                                       (update props prop-kw assoc :value override-value ::propagate? true)
                                       props))))
                               props-with-inherited-override-values
                               properties))

                  ;; Assoc :original-value from original property entries.
                  ;; If your produced property does not make use of the
                  ;; properties map of the node you're evaluating :_properties
                  ;; on, it must set :assoc-original-value? to true in the
                  ;; produced property to set the :original-value here.
                  props-with-overrides-and-original-values
                  (reduce-kv (fn [props prop-kw orig-prop]
                               (let [original-value (:value orig-prop)]
                                 (if (or (contains? properties prop-kw)
                                         (and (not (declared? prop-kw))
                                              (get-in props [prop-kw :assoc-original-value?])))
                                   (update props prop-kw assoc :original-value original-value)
                                   props)))
                             props-with-override-values
                             orig-props)]
              (assoc props :properties props-with-overrides-and-original-values))))

        (or (has-output? node-type output)
            (has-input? node-type output))
        (let [beh (behavior node-type output)]
          ((:fn beh) this output evaluation-context))

        true
        (if (contains? (all-properties node-type) output)
          (get properties output)
          (when-some [node (gt/node-by-id-at basis original-id)]
            (gt/produce-value node output evaluation-context))))))

  gt/OverrideNode
  (clear-property [this basis property] (update this :properties dissoc property))
  (override-id [this] override-id)
  (original [this] original-id)
  (set-original [this original-id] (assoc this :original-id original-id)))

(defn make-override-node [override-id node-id node-type original-id properties]
  (->OverrideNode override-id node-id node-type original-id properties))

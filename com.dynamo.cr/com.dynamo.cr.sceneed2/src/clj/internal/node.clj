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


(ns internal.node
  (:require [camel-snake-kebab :refer [->kebab-case]]
            [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [clojure.core.match :refer [match]]
            [clojure.set :refer [rename-keys union]]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [dynamo.condition :refer :all]
            [dynamo.types :as t]
            [schema.core :as s]
            [schema.macros :as sm]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.query :as iq]
            [service.log :as log]
            [camel-snake-kebab :refer [->kebab-case]]))

(set! *warn-on-reflection* true)

; ---------------------------------------------------------------------------
; Value handling
; ---------------------------------------------------------------------------
(defn node-inputs [v] (into #{} (keys (-> v :descriptor :inputs))))
(defn node-outputs [v] (into #{} (keys (-> v :descriptor :transforms))))
(defn node-cached-outputs [v] (-> v :descriptor :cached))

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


(defn- get-value-with-restarts
  [node g label]
  (restart-case
    (:unreadable-resource
      (:use-value [v] v))
    (:empty-source-list
      (:use-value [v] v))
    (t/get-value node g label)))

(defn get-inputs [target-node g target-label]
  (if (contains? target-node target-label)
    (get target-node target-label)
    (let [schema (get-in target-node [:descriptor :inputs target-label])
          output-transform (get-in target-node [:descriptor :transforms target-label])]
      (cond
        (vector? schema)     (map (fn [[source-node source-label]]
                                    (get-value-with-restarts (dg/node g source-node) g source-label))
                                  (lg/sources g (:_id target-node) target-label))
        (not (nil? schema))  (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
                               (when first-source-node
                                 (get-value-with-restarts (dg/node g first-source-node) g first-source-label)))
        (not (nil? output-transform)) (get-value-with-restarts target-node g target-label)
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

(defn perform [transform node g]
  (cond
    (var?          transform)  (perform (var-get transform) node g)
    (t/has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform)))
    (fn?           transform)  (transform node g)
    :else transform))

(defn- hit-cache [world-state cache-key value]
  (dosync (alter world-state update-in [:cache] cache/hit cache-key))
  value)

(defn- miss-cache [world-state cache-key value]
  (dosync (alter world-state update-in [:cache] cache/miss cache-key value))
  value)

(defn- produce-value [node g label]
  (t/get-value node g label))

(defn get-node-value
  [node label]
  (let [world-state (:world-ref node)]
    (if-let [cache-key (get-in @world-state [:cache-keys (:_id node) label])]
      (let [cache (:cache @world-state)]
        (if (cache/has? cache cache-key)
            (hit-cache  world-state cache-key (get cache cache-key))
            (miss-cache world-state cache-key (produce-value node (:graph @world-state) label))))
      (produce-value node (:graph @world-state) label))))


(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextid (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn tempid [] (- (.getAndIncrement nextid)))

; ---------------------------------------------------------------------------
; Definition handling
; ---------------------------------------------------------------------------
(defn is-schema? [vals] (some :schema (map meta vals)))

(defn deep-merge
  "Recursively merges maps. If keys are not maps, the last value wins."
  [& vals]
  (cond
    (is-schema? vals)         (last vals)
    (every? map? vals)        (apply merge-with deep-merge vals)
    (every? set? vals)        (apply union vals)
    (every? vector? vals)     (into [] (apply concat vals))
    (every? sequential? vals) (apply concat vals)
    :else                     (last vals)))

(defn merge-behaviors [behaviors]
  (apply deep-merge
         (map #(cond
                 (symbol? %) (do (assert (resolve %) (str "Unable to resolve symbol " % " in this context")) (var-get (resolve %)))
                 (var? %)    (var-get (resolve %))
                 (map? %)    %
                 :else       (throw (ex-info (str "Unacceptable behavior " %) :argument %)))
              behaviors)))

(defn classname-for [prefix]  (symbol (str prefix "__")))

(defn fqsymbol [s]
  (let [{:keys [ns name]} (meta (resolve s))]
   (symbol (str ns) (str name))))

(defn- property-symbols [behavior]
  (map (comp symbol name) (keys (:properties behavior))))

(defn state-vector [behavior]
  (into [] (list* 'inputs 'transforms '_id (property-symbols behavior))))

(defn defaults
  [beh]
  (reduce-kv (fn [m k v]
               (let [v (if (seq? v) (eval v) v)]
                 (if (:default v) (assoc m k (:default v)) m)))
             {} (:properties beh)))

(def ^:private property-flags #{:cached :on-update})

(defn- compile-defnode-form
  [prefix form]
  (match [form]
     [(['inherits super] :seq)]
     (let [super-descriptor (resolve super)]
       (assert super-descriptor (str "Cannot resolve " super " to a node definition."))
       (deref super-descriptor))


     [(['property nm tp] :seq)]
     {:properties {(keyword nm) tp}}

     [(['input nm schema] :seq)]
     {:inputs     {(keyword nm) (if (coll? schema) (into [] schema) schema)}}

     [(['on evt & fn-body] :seq)]
     {:event-handlers {(keyword evt)
                       `(fn [~'self ~'event]
                          (dynamo.system/transactional ~@fn-body))}}

     [(['output nm output-type & remainder] :seq)]
     (let [oname       (keyword nm)
           flags       (take-while (every-pred property-flags keyword?) remainder)
           remainder   (drop-while (every-pred property-flags keyword?) remainder)
           args-or-ref (first remainder)
           remainder   (rest remainder)]
       (assert (not (keyword? output-type)) "The output type seems to be missing")
       (assert (or (and (vector? args-or-ref) (seq remainder))
                   (and (symbol? args-or-ref) (var? (resolve args-or-ref))))
         (str "An output clause must have a name, optional flags, and type, before the fn-tail or function name."))
       (let [tform (if (vector? args-or-ref)
                     `(fn ~args-or-ref ~@remainder)
                     (resolve args-or-ref))]
         (reduce
           (fn [m f] (assoc m f #{oname}))
           {:transforms {(keyword nm) tform}}
           flags)))

     [([nm [& args] & remainder] :seq)]
     {:methods        {nm `(fn ~args ~@remainder)}
      :record-methods [[prefix nm args]]}

     [impl :guard symbol?]
     (if (class? (resolve impl))
       {:interfaces [impl]}
       {:impl [impl]})))

(defn resolve-or-else [sym]
  (assert (symbol? sym) (pr-str "Cannot resolve " sym))
  (if-let [val (resolve sym)]
    val
    (throw (ex-info (str "Unable to resolve symbol " sym " in this context") {:symbol sym}))))

(defn map-vals [m f]
  (reduce-kv (fn [i k v] (assoc i k (f v))) {} m))

(defn- eval-default-exprs
  [prop]
  (if (:default prop)
    (update-in prop [:default] eval)
    prop))

(defn- resolve-defaults
  [m]
  (update-in m [:properties] map-vals eval-default-exprs))

(defn generate-message-processor
  [name descriptor]
  (let [event-types (keys (:event-handlers descriptor))]
    `(dynamo.types/process-one-event
       [~'self ~'event]
       (case (:type ~'event)
         ~@(mapcat (fn [e] [e `((get-in ~name [:event-handlers ~e]) ~'self ~'event)]) event-types)
         nil)
       :ok)))

(defn compile-specification
  [nm forms]
  (->> forms
    (map (partial compile-defnode-form nm))
    (reduce deep-merge {:name nm})
    (resolve-defaults)))

(defn- emit-quote [form]
  (list `quote form))

(defn- generate-record-methods [descriptor]
  (for [[defined-in method-name args] (:record-methods descriptor)]
    `(~method-name ~args ((get-in ~defined-in [:methods ~(emit-quote method-name)]) ~@args))))

(defn generate-defrecord [nm descriptor]
  (list* `defrecord (classname-for nm) (state-vector descriptor)
         `dynamo.types/Node
         `(properties [t#] (:properties ~nm))
         `(get-value [t# g# label#]
                     (assert (get-in ~nm [:transforms label#])
                             (str "There is no transform " label# " on node " (:_id t#)))
                     (perform (get-in ~nm [:transforms label#]) t# g#))
         `(inputs [t#] (into #{} (keys (:inputs ~nm))))
         `(outputs [t#] (into #{} (keys (:transforms ~nm))))
         `(cached-outputs [t#] (:cached ~nm))
         `t/MessageTarget
         (generate-message-processor nm descriptor)
         (concat
           (map fqsymbol (:impl descriptor))
           (:interfaces descriptor)
           (generate-record-methods descriptor))))

(defn quote-functions
  [descriptor]
  (-> descriptor
    (update-in [:name]           emit-quote)
    (update-in [:methods]        (fn [ms] (zipmap (map emit-quote (keys ms)) (vals ms))))
    (update-in [:record-methods] (partial mapv emit-quote))
    (update-in [:impl]           (partial mapv (comp emit-quote fqsymbol)))))

(defn generate-constructor [nm descriptor]
  (let [ctor             (symbol (str 'make- (->kebab-case (str nm))))
        record-ctor      (symbol (str 'map-> (classname-for nm)))]
    `(defn ~ctor
       ~(str "Constructor for " nm ", using default values for any property not in property-values.\nThe properties on " nm " are:\n" #_(describe-properties behavior))
       [& {:as ~'property-values}]
       (~record-ctor (merge {:_id (tempid)}
                            ~{:descriptor nm}
                            ~(defaults descriptor)
                            ~'property-values)))))

(defn generate-descriptor [nm descriptor]
  `(def ~nm ~(quote-functions descriptor)))

(defn generate-print-method [nm]
  (let [classname (classname-for nm)
        tagged-arg (vary-meta (gensym "v") assoc :tag (resolve classname))]
    `(defmethod print-method ~classname
       [~tagged-arg w#]
       (.write ^java.io.Writer w# (str "<" ~(str nm)
                                       (merge (select-keys ~tagged-arg [:_id :inputs :outputs])
                                              (select-keys ~tagged-arg (-> ~tagged-arg :descriptor :properties keys)))
                                       ">")))))

; ---------------------------------------------------------------------------
;
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

(ns internal.node
  (:require [camel-snake-kebab :refer [->kebab-case]]
            [clojure.core.match :refer [match]]
            [clojure.core.async :as a]
            [clojure.set :refer [rename-keys union]]
            [dynamo.condition :refer :all]
            [dynamo.types :as t]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [service.log :as log]))

(set! *warn-on-reflection* true)

; ---------------------------------------------------------------------------
; Value handling
; ---------------------------------------------------------------------------
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
    (t/get-value node g label)))

(defn get-inputs [target-node g target-label]
  (if (contains? target-node target-label)
    (get target-node target-label)
    (let [schema (get-in target-node [:inputs target-label])]
      (cond
        (vector? schema)     (map (fn [[source-node source-label]]
                                    (get-value-with-restarts (dg/node g source-node) g source-label))
                                  (lg/sources g (:_id target-node) target-label))
        (not (nil? schema))  (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
                               (get-value-with-restarts (dg/node g first-source-node) g first-source-label))
        :else                (let [missing (missing-input target-node target-label)]
                               (service.log/warn :missing-input missing)
                               missing)))))

(defn collect-inputs [node g input-schema]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        :project   (assoc m k (:project-ref node))
        s/Keyword  m
        (assoc m k (get-inputs node g k))))
    {} input-schema))

(defn perform [transform node g]
  (cond
    (symbol?       transform)  (perform (resolve transform) node g)
    (var?          transform)  (perform (var-get transform) node g)
    (t/has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform)))
    (fn?           transform)  (transform node g)
    :else transform))

(def ^:private ^java.util.concurrent.atomic.AtomicInteger
     nextid (java.util.concurrent.atomic.AtomicInteger. 1000000))

(defn tempid [] (- (.getAndIncrement nextid)))

(defn node-inputs [v] (into #{} (keys (:inputs v))))
(defn node-outputs [v] (into #{} (keys (:transforms v))))

(defn get-input [])
(defn refresh-inputs [g n i])

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

(defn- property-symbols [behavior]
  (map (comp symbol name) (keys (:properties behavior))))

(defn state-vector [behavior]
  (into [] (list* 'inputs 'transforms '_id (property-symbols behavior))))

(defn defaults
  [beh]
  (let [out (dissoc beh :event-handlers :impl :impl-methods)]
    (reduce-kv (fn [m k v]
                 (let [v (if (seq? v) (eval v) v)]
                   (if (:default v) (assoc m k (:default v)) m)))
               out (:properties out))))

(defn- print-md-table
  "Prints a collection of maps in a textual table suitable for converting
   to GMD-flavored markdown. Prints table headings
   ks, and then a line of output for each row, corresponding to the keys
   in ks. If ks are not specified, use the keys of the first item in rows.

   Cribbed from `clojure.pprint/print-table`."
  ([ks rows]
     (when (seq rows)
       (let [widths (map
                     (fn [k]
                       (apply max (count (str k)) (map #(count (str (get % k))) rows)))
                     ks)
             spacers (map #(apply str (repeat % "-")) widths)
             fmts (map #(str "%" % "s") widths)
             fmt-row (fn [leader divider trailer row]
                       (str leader
                            (apply str (interpose divider
                                                  (for [[col fmt] (map vector (map #(get row %) ks) fmts)]
                                                    (format fmt (str col)))))
                            trailer))]
         (println)
         (println (fmt-row "| " " | " " |" (zipmap ks ks)))
         (println (fmt-row "|-" "-|-" "-|" (zipmap ks spacers)))
         (doseq [row rows]
           (println (fmt-row "| " " | " " |" row))))))
  ([rows] (print-md-table (keys (first rows)) rows)))

(defn describe-properties [behavior]
  (with-out-str (print-md-table ["Name" "Type" "Default"]
                            (reduce-kv
                              (fn [rows k v]
                                  (conj rows (assoc (rename-keys v {:schema "Type" :default "Default"}) "Name" k)))
                              []
                              (:properties behavior)))))

(defn generate-constructor [nm behavior]
  (let [ctor             (symbol (str 'make- (->kebab-case (str nm))))
        record-ctor      (symbol (str 'map-> nm))]
    `(defn ~ctor
       ~(str "Constructor for " nm ", using default values for any property not in property-values.\nThe properties on " nm " are:\n"
             #_(describe-properties behavior))
       [& {:as ~'property-values}]
       (~record-ctor (merge {:_id (tempid)} ~(defaults behavior) ~'property-values)))))

; --------------------------------------------------------------------
(defn- replace-magic-imperatives
  [form]
  (match [form]
         [(['repaint] :seq)]
         `(dynamo.ui/repaint-current-view)

         [(['attach & rest] :seq)]
         `(conj! ~'transaction (dynamo.project/connect ~@rest))

         [(['detach & rest] :seq)]
         `(conj! ~'transaction (dynamo.project/disconnect ~@rest))

         [(['send target-node type & body] :seq)]
         `(conj! ~'message-drop [~target-node ~type ~body])

         [(['invalidate target-node output] :seq)]
         `(conj! ~'transaction (dynamo.project/update-resource ~target-node ~output))

         [(['set-property target-node & pvs] :seq)]
         `(conj! ~'transaction (dynamo.project/update-resource ~target-node assoc ~@pvs))

         [(['update-property target-node property f & args] :seq)]
         `(conj! ~'transaction (dynamo.project/update-resource ~target-node update-in [~property] ~f ~@args))

         [(['new node-type & rest] :seq)]
         (let [ctor (symbol (str 'make- (->kebab-case (str node-type))))]
           `(let [~'new-node ~(list* ctor rest)]
              (conj! ~'transaction (dynamo.project/new-resource ~'new-node))
              ~'new-node))

         :else
         form))

(defn transactional-specials
  [form]
  (cond
    (seq? form)    (map transactional-specials (replace-magic-imperatives form))
    (vector? form) (mapv transactional-specials (replace-magic-imperatives form))
    :else          (replace-magic-imperatives form)))

(defmacro transactional
  [& forms]
  `(let [~'transaction  (transient [])
         ~'message-drop (transient [])]
     (let [result# (do ~@(transactional-specials forms))]
       (when (and (< 0 (count ~'transaction))
                  (= :ok (:status (dynamo.project/transact (:project-ref ~'self) (persistent! ~'transaction)))))
         (doseq [m# (persistent! ~'message-drop)]
           (apply dynamo.project/publish m#)))
       result#)))

(def ^:private property-flags #{:cached :on-update})

(defn- compile-defnode-form
  [prefix form]
  (match [form]
     [(['inherits nm] :seq)]
     {:inherits   #{nm}}

     [(['property nm tp] :seq)]
     {:properties {(keyword nm) tp}}

     [(['input nm schema] :seq)]
     {:inputs     {(keyword nm) (if (coll? schema) (into [] schema) schema)}}

     [(['on evt & fn-body] :seq)]
     {:event-handlers {(keyword evt) (list* `do fn-body)}}

     [(['output nm output-type & remainder] :seq)]
     (let [oname       (keyword nm)
           flags       (take-while (every-pred property-flags keyword?) remainder)
           remainder   (drop-while (every-pred property-flags keyword?) remainder)
           args-or-ref (first remainder)
           remainder   (rest remainder)]
       (assert (or (and (vector? args-or-ref) (not (nil? remainder)))
                   (symbol? args-or-ref)
                   (var? args-or-ref)) (str "An output clause must have a name, optional flags, and type, before the fn-tail or function name."))
       (let [tform (cond
                     (vector? args-or-ref)  `(defn ~(symbol (str prefix ":" nm)) ~args-or-ref ~@remainder)
                     (symbol? args-or-ref)  (resolve args-or-ref)
                     :else                  args-or-ref)]
         (reduce
           (fn [m f] (assoc m f #{oname}))
           {:transforms {(keyword nm) tform}}
           flags)))

     [([nm [& args] & remainder] :seq)]
     {:impl-methods [`(~nm ~args ~@remainder)]}

     [impl :guard symbol?]
     {:impl [(if (var? (resolve impl)) (:on (deref (resolve impl))) impl)]}))

(defn- resolve-or-else [sym]
  (assert (symbol? sym) (pr-str "Cannot resolve " sym))
  (if-let [val (resolve sym)]
    val
    (throw (ex-info (str "Unable to resolve symbol " sym " in this context") {:symbol sym}))))

(defn- ancestor-behaviors
  [{:keys [inherits] :as m}]
  (let [inherits (map #(symbol (str % ":descriptor")) inherits)
        behaviors (apply deep-merge (map (comp deref resolve-or-else) inherits))]
    (deep-merge behaviors (dissoc m :inherits))))

(defn map-vals [m f]
  (reduce-kv (fn [i k v] (assoc i k (f v))) {} m))

(defn- fix-event-handlers
  [m]
  (update-in m [:event-handlers] map-vals #(list `transactional %)))

(defn- eval-default-exprs
  [prop]
  (if (:default prop)
    (update-in prop [:default] eval)
    prop))

(defn- resolve-defaults
  [m]
  (update-in m [:properties] map-vals eval-default-exprs))

(defn generate-event-loop
  [beh]
  `(dynamo.types/start-event-loop!
      [~'this ~'in]
      (a/go-loop [id# (:_id ~'this)]
        (when-let [~'msg (a/<! ~'in)]
          (try
            (dynamo.types/process-one-event (dynamo.project/node-by-id (:project-ref ~'this) id#) (:body ~'msg))
            (catch Exception ~'ex
              (service.log/error :message "Error in node event loop" :exception ~'ex)))
          (recur id#)))))

(defn- wrap-in-transaction
  [fs]
  (list* `transactional fs))

(defn generate-message-processor
  [beh]
  (let [event-cases (mapcat identity (:event-handlers beh))]
    `(dynamo.types/process-one-event
       [~'self ~'event]
       (transactional
         (case (:type ~'event)
           ~@event-cases
           nil)
         :ok))))

(defn compile-specification
  [name forms]
  (->> forms
    (map (partial compile-defnode-form name))
    (reduce deep-merge {:name name})
    (ancestor-behaviors)
    (resolve-defaults)))

(defn generate-type [name behavior]
  (let [t 'this#
        g 'g#
        o 'output#
        d 'default#]
    (list* 'defrecord name (state-vector behavior)
           'dynamo.types/Node
           `(properties [~t] ~(:properties behavior))
           `(get-value [~t ~g ~'label]
                       (assert (get-in ~t [:transforms ~'label])
                               (str "There is no transform " ~'label " on node " (:_id ~t)))
                       (perform (get-in ~t [:transforms ~'label]) ~t ~g))
           (concat
             (:impl behavior)
             (:impl-methods behavior)
             (when (< 0 (count (:event-handlers behavior)))
               ['dynamo.types/MessageTarget
                (generate-event-loop behavior)
                (generate-message-processor behavior)])))))

(defn quote-functions
  [beh]
  (-> beh
    (update-in [:event-handlers] map-vals #(list `quote %))
    (update-in [:impl-methods]   (fn [ms] (mapv #(list* `quote %) ms)))))

(defn generate-descriptor [name behavior]
  (let [descriptor-name (symbol (str name ":descriptor"))
        behavior        (quote-functions behavior)]
    `(def ~descriptor-name ~behavior)))

(defn generate-print-method [name]
  (let [tagged-arg (vary-meta (gensym "v") assoc :tag (resolve name))]
    `(defmethod print-method ~name
       [~tagged-arg w#]
       (.write ^java.io.Writer w# (str "<" ~(str name)
                                       (merge (select-keys ~tagged-arg [:_id :inputs :outputs])
                                              (select-keys ~tagged-arg (select-keys ~tagged-arg (:properties ~tagged-arg))))
                                       ">")))))

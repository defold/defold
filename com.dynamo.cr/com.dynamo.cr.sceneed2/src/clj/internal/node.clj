(ns internal.node
  (:require [clojure.core.match :refer [match]]
            [clojure.core.async :as a]
            [clojure.set :refer [rename-keys union]]
            [clojure.walk :as walk]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [dynamo.types :as t]
            [service.log :as log]
            [camel-snake-kebab :refer [->kebab-case]]))

; ---------------------------------------------------------------------------
; Value handling
; ---------------------------------------------------------------------------
(defn missing-input [n l]
  {:error     :input-not-declared
   :node      (:_id n)
   :node-type (class n)
   :label     l})

(defn get-inputs [target-node g target-label seed]
  (if (contains? target-node target-label)
    (get target-node target-label)
    (let [schema (get-in target-node [:inputs target-label])]
      (cond
        (vector? schema)     (map (fn [[source-node source-label]]
                                    (t/get-value (dg/node g source-node) g source-label seed))
                                  (lg/sources g (:_id target-node) target-label))
        (not (nil? schema))  (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
                               (t/get-value (dg/node g first-source-node) g first-source-label seed))
        :else                (let [missing (missing-input target-node target-label)]
                               (service.log/warn :missing-input missing)
                               missing)))))

(defn collect-inputs [node g input-schema seed]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        :project   m
        s/Keyword  m
        (assoc m k (get-inputs node g k seed))))
    seed input-schema))

(defn perform [transform node g seed]
  (cond
    (symbol?       transform)  (perform (resolve transform) node g seed)
    (var?          transform)  (perform (var-get transform) node g seed)
    (t/has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform) seed))
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

(defn- event-loop-name [behavior]
  (symbol (str (:name behavior) ":event-loop")))

(defn defaults [behavior]
  (reduce-kv (fn [m k v] (if (:default v) (assoc m k (:default v)) m))
             behavior (:properties behavior)))

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
     {:event-handlers {(keyword evt) (first fn-body)}}

     [(['output nm output-type & remainder] :seq)]
     (let [oname       (keyword nm)
           flags       (take-while (every-pred property-flags keyword?) remainder)
           remainder   (drop-while (every-pred property-flags keyword?) remainder)
           args-or-ref (first remainder)
           remainder   (rest remainder)]
       (assert (or (and (vector? args-or-ref) (not (nil? remainder)))
                   (symbol? args-or-ref)
                   (var? args-or-ref)) (pr-str "Expecting a variable, symbol, or fn-tail. Got " args-or-ref " " remainder))
       (let [tform (cond
                     (vector? args-or-ref)  `(defn ~(symbol (str prefix ":" nm)) ~args-or-ref ~@remainder)
                     (symbol? args-or-ref)  (resolve args-or-ref)
                     :else                  args-or-ref)]
         (reduce
           (fn [m f] (assoc m f #{oname}))
           {:transforms {(keyword nm) tform}}
           flags)))))

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

(defn- replace-magic-imperatives
  [form]
  (match [form]
         [(['attach & rest] :seq)]
         `(conj! ~'transaction (p/connect ~@rest))

         [(['dettach & rest] :seq)]
         `(conj! ~'transaction (p/disconnect ~@rest))

         [(['send target-node type & body] :seq)]
         `(conj! ~'message-drop [~target-node ~type ~body])

         [(['invalidate target-node output] :seq)]
         `(conj! ~'transaction (p/update-resource ~target-node ~output))

         [(['new node-type & rest] :seq)]
         (let [ctor (symbol (str 'make- (->kebab-case (str node-type))))]
           `(let [~'new-node ~(list* ctor rest)]
              (conj! ~'transaction (dynamo.project/new-resource ~'new-node))
              ~'new-node))

         :else
         form))

(defn- event-loop-specials
  [form]
  (cond
    (list? form)   (map event-loop-specials (replace-magic-imperatives form))
    (vector? form) (mapv event-loop-specials (replace-magic-imperatives form))
    :else          form))

(defn map-vals [m f]
  (reduce-kv (fn [i k v] (assoc i k (f v))) {} m))

(defn- fix-event-handlers
  [m]
  (update-in m [:event-handlers] map-vals event-loop-specials))

(defn- eval-default-exprs
  [prop]
  (if (:default prop)
    (update-in prop [:default] eval)
    prop))

(defn- resolve-defaults
  [m]
  (update-in m [:properties] map-vals eval-default-exprs))

(defn generate-event-loop
  [name beh]
  (let [fn-name     (symbol (str (:name beh) ":event-loop"))
        event-cases (mapcat identity (:event-handlers beh))]
    `(start-event-loop!
        [~'self ~'project-state ~'in]
        (a/go-loop []
          (when-let [~'event (a/<! ~'in)]
            (let [~'transaction (transient [])
                  ~'message-drop (transient [])]
              (case (:type ~'event) ~@event-cases)
              (when (and (< 0 (count ~'transaction))
                         (= :ok (:status (dynamo.project/transact ~'project-state ~'transaction))))
                (doseq [m# ~'message-drop]
                  (apply dynamo.project/publish ~'project-state m#)))
              (post-event ~'project-state ~'transaction ~'message-drop))
            (recur))))))

(defn compile-specification
  [name forms]
  (->> forms
    (map (partial compile-defnode-form name))
    (reduce deep-merge {:name name})
    (ancestor-behaviors)
    (fix-event-handlers)
    (resolve-defaults)))

(defn generate-type [name behavior]
  (let [t 'this#
        g 'g#
        o 'output#
        d 'default#]
    (list* 'defrecord name (state-vector behavior)
           'dynamo.types/Node
           `(properties [~t] ~(:properties behavior))
           `(get-value [~t ~g ~'label ~'seed]
                       (assert (get-in ~t [:transforms ~'label])
                               (str "There is no transform " ~'label " on node " (:_id ~t)))
                       (perform (get-in ~t [:transforms ~'label]) ~t ~g ~'seed))
           (when (< 0 (count (:event-handlers behavior)))
             ['MessageTarget
              (generate-event-loop name behavior)]))))

(defn- quote-symbols
  [form]
  (walk/postwalk (fn [x] (if (symbol? x) (list 'quote x) x)) form))

(defn generate-descriptor [name behavior]
  (let [descriptor-name (symbol (str name ":descriptor"))]
    `(def ~descriptor-name ~behavior)))
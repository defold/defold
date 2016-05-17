(ns internal.util
  "Helpful utility functions for graph"
  (:require [camel-snake-kebab :refer [->Camel_Snake_Case_String]]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [plumbing.fnk.pfnk :as pf]
            [potemkin.namespaces :as namespaces]
            [schema.core :as s]))

(set! *warn-on-reflection* true)

(defn predicate? [x] (instance? schema.core.Predicate x))
(defn schema?    [x] (satisfies? s/Schema x))
(defn protocol?  [x] (and (map? x) (contains? x :on-interface)))

(defn var-get-recursive [var-or-value]
  (if (var? var-or-value)
    (recur (var-get var-or-value))
    var-or-value))

(defn apply-if-fn [f & args]
  (if (fn? f)
    (apply f args)
    f))

(defn removev [pred coll]
  (filterv (complement pred) coll))

(defn conjv [coll x]
  (conj (or coll []) x))

(defn filterm [pred m]
  "like filter but applys the predicate to each key value pair of the map"
  (into {} (filter pred m)))

(defn key-set
  [m]
  (set (keys m)))

(defn map-keys
  [f m]
  (reduce-kv (fn [m k v] (assoc m (f k) v)) (empty m) m))

(defn map-vals
  [f m]
  (reduce-kv (fn [m k v] (assoc m k (f v))) m m))

(defn map-first
  [f pairs]
  (mapv (fn [[a b]] [(f a) b]) pairs))

(defn map-diff
  [m1 m2 sub-fn]
  (reduce-kv
    (fn [result k v]
      (if (contains? result k)
        (let [ov (get result k)]
          (let [nv (sub-fn ov v)]
            (if (empty? nv)
              (dissoc result k)
              (assoc result k nv))))
        result))
    m1
    m2))

(defn parse-number
  "Reads a number from a string. Returns nil if not a number."
  [s]
  (when (re-find #"^-?\d+\.?\d*$" s)
    (read-string (str/replace s #"^(-?)0*(\d+\.?\d*)$" "$1$2"))))

(defn parse-int
  "Reads an integer from a string. Returns nil if not an integer."
  [s]
  (when (re-find #"^-?\d+\.?0*$" s)
    (read-string (str/replace s #"^(-?)0*(\d+)\.?0*$" "$1$2"))))

(defn replace-top
  [stack value]
  (conj (pop stack) value))

(defn apply-deltas
  [old new removed-f added-f]
  (let [removed (set/difference old new)
        added   (set/difference new old)]
    [(removed-f removed) (added-f added)]))

(defn keyword->label
  "Turn a keyword into a human readable string"
  [k]
  (-> k
    name
    ->Camel_Snake_Case_String
    (str/replace "_" " ")))

(def safe-inc (fnil inc 0))

(defn collify
  [val-or-coll]
  (if (and (coll? val-or-coll) (not (map? val-or-coll))) val-or-coll [val-or-coll]))

(defn stackify [x]
  (if (coll? x)
    (vec x)
    [x]))

(defn project
  "Like clojure.set/project, but returns a vector of tuples instead of
  a set of maps."
  [xrel ks]
  (with-meta (vec (map #(vals (select-keys % ks)) xrel)) (meta xrel)))

(defn filter-vals [p m] (select-keys m (for [[k v] m :when (p v)] k)))

(defn guard [f g]
  (fn [& args]
    (when (apply f args)
      (apply g args))))

(defn fnk-schema
  [f]
  (when f
    (pf/input-schema f)))

(defn fnk-arguments
  [f]
  (when f
    (key-set (dissoc (fnk-schema f) s/Keyword))))

(defn var?! [v] (when (var? v) v))

(defn vgr [s] (some-> s resolve var?! var-get))

(defn protocol-symbol?
  [x]
  (and (symbol? x)
       (let [x' (vgr x)]
         (and (map? x') (contains? x' :on-interface)))))

(defn resolve-value-type
  [x]
  (cond
    (symbol? x)                     (resolve-value-type (resolve x))
    (var? x)                        (resolve-value-type (var-get x))
    (vector? x)                     (mapv resolve-value-type x)
    :else                           x))

(defn resolve-schema [x]
  (cond
    (symbol? x)    (resolve-schema (resolve x))
    (var? x)       (resolve-schema (var-get x))
    (predicate? x) x
    (vector? x)    (mapv resolve-schema x)
    (class? x)     x
    (protocol? x)  x
    (map? x)       x
    :else          nil))

(defn assert-form-kind [place required-kind-label required-kind-pred label form]
  (assert (not (nil? form))
          (str place " " label " requires a " required-kind-label " but got nil"))
  (assert (required-kind-pred form)
          (str place " " label " requires a " required-kind-label " not '" form "' of type " (type form))))

(defn pfnk?
  "True if the function has a schema. (I.e., it is a valid production function"
  [f]
  (and (fn? f) (contains? (meta f) :schema)))

(defn pfnksymbol? [x] (or (pfnk? x) (and (symbol? x) (pfnk? (vgr x)))))
(defn pfnkvar?    [x] (or (pfnk? x) (and (var? x) (pfnk? (var-get x)))))

(defn- quoted-var? [x] (and (seq? x) (= 'var (first x))))
(defn- pfnk-form?  [x] (seq? x))

(defn- maybe-expand-macros
  [[call & _ :as body]]
  (if (or (= "fn" (name call)) (= "fnk" (name call)))
    body
    (macroexpand-1 body)))

(defn- parse-fnk-arguments
  [f]
  (let [f (maybe-expand-macros f)]
    (loop [args  #{}
           forms (seq (second f))]
      (if-let [arg (first forms)]
        (if (= :- (first forms))
          (recur args (drop 2 forms))
          (recur (conj args (keyword (first forms))) (next forms)))
        args))))

(defn inputs-needed [x]
  (cond
    (pfnk? x)       (fnk-arguments x)
    (symbol? x)     (inputs-needed (resolve x))
    (var? x)        (inputs-needed (var-get x))
    (quoted-var? x) (inputs-needed (var-get (resolve (second x))))
    (pfnk-form? x)  (parse-fnk-arguments x)
    :else           #{}))

(defn- wrap-protocol
  [tp tp-form]
  (if (protocol? tp)
    (list `s/protocol tp-form)
    tp-form))

(defn- wrap-enum
  [tp tp-form]
  (if (set? tp)
    (do (println tp-form)
      (list `s/enum tp-form))
    tp-form))

(defn preprocess-schema
  [tp-form]
  (let [multi?    (vector? tp-form)
        schema    (if multi? (first tp-form) tp-form)
        tp        (resolve-value-type schema)
        processed (wrap-enum (wrap-protocol tp schema) schema)]
    (if multi?
      (vector processed)
      processed)))

(defn update-paths
  [m paths xf]
  (reduce (fn [m [p v]] (assoc-in m p (xf p v (get-in m p)))) m paths))

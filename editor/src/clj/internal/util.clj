(ns internal.util
  "Helpful utility functions for graph"
  (:require [camel-snake-kebab :refer [->Camel_Snake_Case_String]]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [plumbing.fnk.pfnk :as pf]
            [potemkin.namespaces :as namespaces]
            [schema.core :as s]))

(defn schema? [x] (satisfies? s/Schema x))

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

(defn push-with-size-limit
  "Given bounds, a vector, and a value, returns a new vector with value at the tail.
  The vector will contain no more than max-size elements. If the result would exceed
  this limit, elements are dropped from the front of the vector to reduce to min-size.

  Intended for use along with `clojure.core/peek` and `clojure.core/pop` to treat a
  vector as a stack with limited size."
  [min-size max-size stack value]
  (let [stack (conj stack value)
        size (count stack)]
    (if (< max-size size)
      (vec (drop (- size min-size) stack))
      stack)))

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

(ns dynamo.util
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [potemkin.namespaces :as namespaces]))

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

(defn applym
  "Like apply, but with a map. Flattens the key/value pairs
into an arglist."
  [f m]
  (apply f (mapcat identity m)))

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

(defmacro monitored-task
 [mon nm size & body]
 `(let [m# ~mon]
    (when m#
      (.beginTask m# ~nm ~size))
    (let [res# (do ~@body)]
      (when m# (.done m#))
      res#)))

(defmacro monitored-work
  [mon subtask & body]
  `(let [m# ~mon]
     (when m# (.subTask m# ~subtask))
     (let [res# (do ~@body)]
       (when m# (.worked m# 1))
       res#)))

(defmacro doseq-monitored [monitor task-name bindings & body]
  (assert (= 2 (count bindings)) "Monitored doseq only allows one binding")
  `(let [items# ~(second bindings)]
     (.beginTask ~monitor ~task-name (count items#))
     (loop [continue# true
            items#     items#]
       (when continue#
         (when (seq items#)
           (let [~(first bindings) (first items#)]
             ~@body)
           (.worked ~monitor 1)
           (recur (.isCanceled ~monitor) (rest items#)))))
     (.done ~monitor)))

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

(def mac? (-> (System/getProperty "os.name")
            (.toLowerCase)
            (.indexOf "mac")
            (>= 0)))

(defn collify
  [val-or-coll]
  (if (and (coll? val-or-coll) (not (map? val-or-coll))) val-or-coll [val-or-coll]))

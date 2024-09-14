;; Copyright 2020-2024 The Defold Foundation
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

(ns util.coll
  (:refer-clojure :exclude [bounded-count empty? mapcat not-empty some])
  (:import [clojure.lang Cons IEditableCollection MapEntry]
           [java.util ArrayList]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def empty-sorted-map (sorted-map))

(defn list-or-cons?
  "Returns true if the specified value is either a IPersistentList or a
  clojure.lang.Cons. Useful in macros, where list expressions can be either
  lists or Cons, often interchangeably."
  [value]
  ;; Note that implementing `cons?` as a separate function would be inadvisable,
  ;; since `(cons val nil)` does not return a Cons, but a IPersistentList.
  (or (list? value)
      (instance? Cons value)))

(defn mapcat
  "Like core.mapcat, but faster in the non-transducer case."
  ([f] (comp (map f) cat))
  ([f coll] (sequence (mapcat f) coll))
  ([f coll & colls] (apply sequence (mapcat f) coll colls)))

(defn ascending-order
  "Comparator that orders items in ascending order."
  [a b]
  (compare a b))

(defn descending-order
  "Comparator that orders items in descending order."
  [a b]
  (compare b a))

(defn supports-transient?
  "Returns true if the supplied persistent collection can be made into a
  transient collection."
  [coll]
  (instance? IEditableCollection coll))

(defn pair
  "Constructs a two-element collection that implements IPersistentVector from
  the supplied arguments."
  [a b]
  (MapEntry. a b))

(defmacro pair-fn
  "Returns a function that takes a value and returns a pair from it. The
  supplied key-fn is expected to take a value and return the key to use for
  that value. If supplied, the value-fn is applied to the value to produce the
  value portion of the pair. Useful when transforming sequences into maps, and
  can be a drop-in replacement for (juxt key-fn value-fn)."
  ([key-fn]
   `(let [key-fn# ~key-fn]
      (fn ~'value->pair [~'value]
        (pair (key-fn# ~'value)
              ~'value))))
  ([key-fn value-fn]
   `(let [key-fn# ~key-fn
          value-fn# ~value-fn]
      (fn ~'value->pair [~'value]
        (pair (key-fn# ~'value)
              (value-fn# ~'value))))))

(defn flipped-pair
  "Constructs a two-element collection that implements IPersistentVector from
  the reversed arguments."
  [a b]
  (MapEntry. b a))

(defn bounded-count
  "Like core.bounded-count, but tags the return value and limit argument as long
  values to avoid boxed math or reflection. If coll is counted? returns its
  count, else will count at most the first n elements of coll using its seq."
  ^long [^long n coll]
  (if (counted? coll)
    (count coll)
    (loop [i 0
           s (seq coll)]
      (if (and s (< i n))
        (recur (inc i) (next s))
        i))))

(defn resize
  "Returns a collection of the desired size, retaining elements from coll up to
  the desired size. If coll does not have enough elements, fill the subsequent
  slots in the output collection with the supplied fill-value."
  [coll ^long new-count fill-value]
  {:pre [(sequential? coll)
         (not (neg? new-count))]}
  (let [old-count (count coll)]
    (cond
      (< new-count old-count)
      (into (empty coll)
            (take new-count)
            coll)

      (> new-count old-count)
      (into coll
            (repeat (- new-count old-count)
                    fill-value))

      :else
      coll)))

(defn empty-with-meta
  "Like core.empty, but ensures the metadata is preserved for all collections."
  [coll]
  (let [original-meta (meta coll)
        empty-coll (empty coll)]
    (if (and original-meta (nil? (meta empty-coll)))
      (with-meta empty-coll original-meta)
      empty-coll)))

(defn empty?
  "Like core.empty?, but avoids generating garbage when possible."
  [coll]
  (cond
    (nil? coll)
    true

    (counted? coll)
    (zero? (count coll))

    (instance? CharSequence coll)
    (.isEmpty ^CharSequence coll)

    :else
    (not (seq coll))))

(defn not-empty
  "Like core.not-empty, but avoids generating garbage when possible."
  [coll]
  (if (empty? coll)
    nil
    coll))

(def into-set (fnil into #{}))

(def into-vector (fnil into []))

(defn pair-map-by
  "Returns a hash-map where the keys are the result of applying the supplied
  key-fn to each item in the input sequence and the values are the items
  themselves. Returns a stateless transducer if no input sequence is
  provided. Optionally, a value-fn can be supplied to transform the values."
  ([key-fn]
   {:pre [(ifn? key-fn)]}
   (map (pair-fn key-fn)))
  ([key-fn value-fn-or-coll]
   (if (fn? value-fn-or-coll)
     (map (pair-fn key-fn value-fn-or-coll))
     (into {}
           (map (pair-fn key-fn))
           value-fn-or-coll)))
  ([key-fn value-fn coll]
   (into {}
         (map (pair-fn key-fn value-fn))
         coll)))

(defn partition-all-primitives
  "Returns a lazy sequence of primitive vectors. Like core.partition-all, but
  creates a new vector of a single primitive-type for each partition. The
  primitive-type must be one of :int :long :float :double :byte :short :char, or
  :boolean. Returns a stateful transducer when no collection is provided."
  ([primitive-type ^long partition-length]
   (fn [rf]
     (let [in-progress (ArrayList. partition-length)]
       (fn
         ([] (rf))
         ([result]
          (let [result
                (if (.isEmpty in-progress)
                  result
                  (let [finished (apply vector-of primitive-type (.toArray in-progress))]
                    (.clear in-progress)
                    (unreduced (rf result finished))))]
            (rf result)))
         ([result input]
          (.add in-progress input)
          (if (= partition-length (.size in-progress))
            (let [finished (apply vector-of primitive-type (.toArray in-progress))]
              (.clear in-progress)
              (rf result finished))
            result))))))
  ([primitive-type ^long partition-length coll]
   (partition-all-primitives primitive-type partition-length partition-length coll))
  ([primitive-type ^long partition-length ^long step coll]
   (lazy-seq
     (when-let [in-progress (seq coll)]
       (let [finished (apply vector-of primitive-type (take partition-length in-progress))]
         (cons finished (partition-all-primitives primitive-type partition-length step (nthrest in-progress step))))))))

(defn separate-by
  "Separates items in the supplied collection into two based on a predicate.
  Returns a pair of [true-items, false-items]. The resulting collections will
  have the same type and metadata as the input collection."
  [pred coll]
  (cond
    ;; Avoid generating needless garbage for empty collections.
    (empty? coll)
    (pair coll coll)

    ;; Transient implementation.
    (supports-transient? coll)
    (let [coll-meta (meta coll)
          empty-coll (empty coll)
          separated (reduce (fn [result item]
                              (if (pred item)
                                (pair (conj! (key result) item)
                                      (val result))
                                (pair (key result)
                                      (conj! (val result) item))))
                            (pair (transient empty-coll)
                                  (transient empty-coll))
                            coll)]
      (pair (-> separated key persistent! (with-meta coll-meta))
            (-> separated val persistent! (with-meta coll-meta))))

    ;; Non-transient implementation.
    :else
    (let [empty-coll (with-meta (empty coll) (meta coll))]
      (reduce (fn [result item]
                (if (pred item)
                  (pair (conj (key result) item)
                        (val result))
                  (pair (key result)
                        (conj (val result) item))))
              (pair empty-coll empty-coll)
              coll))))

(defn aggregate-into
  "Aggregate a sequence of items into an associative collection. The pair-fn
  will be called on each item to produce a key-value pair. If it returns nil,
  we skip the item, otherwise we will look up the key in coll and run the
  accumulate-fn on the existing value and the value from the key-value pair
  returned by the pair-fn. The result will be assoc:ed into coll. Optionally, an
  init value can be specified to use as the first argument to the accumulate-fn
  when there is no existing value for the key in coll. If init is a function,
  it will be called with no arguments to produce a new init value when needed."
  ([coll pair-fn accumulate-fn items]
   (aggregate-into coll pair-fn accumulate-fn (accumulate-fn) items))
  ([coll pair-fn accumulate-fn init items]
   (if (empty? items)
     coll
     (let [use-transient (supports-transient? coll)
           use-fn-init (fn? init)
           assoc-fn (if use-transient assoc! assoc)
           lookup-fn (if use-fn-init
                       (fn lookup-fn [accumulated-by-key key]
                         (let [accumulated (get accumulated-by-key key ::not-found)]
                           (case accumulated
                             ::not-found (init)
                             accumulated)))
                       (fn lookup-fn [accumulated-by-key key]
                         (get accumulated-by-key key init)))]
       (cond-> (reduce (fn [accumulated-by-key item]
                         (if-some [[key value] (pair-fn item)]
                           (let [accumulated (lookup-fn accumulated-by-key key)
                                 accumulated (accumulate-fn accumulated value)]
                             (assoc-fn accumulated-by-key key accumulated))
                           accumulated-by-key))
                       (cond-> coll use-transient transient)
                       items)
               use-transient (-> (persistent!)
                                 (with-meta (meta coll))))))))

(defn mapcat-indexed
  "Returns the result of applying concat to the result of applying map-indexed
  to f and coll. Thus function f should return a collection. Returns a
  transducer when no collection is provided."
  ([f] (comp (map-indexed f) cat))
  ([f coll] (sequence (mapcat-indexed f) coll)))

(defn search
  "Traverses the supplied collection hierarchy recursively, applying the
  specified match-fn to every non-collection value. Records are considered
  values, not collections. Returns a sequence of all non-nil results returned by
  the match-fn."
  [coll match-fn]
  (cond
    (record? coll)
    (when-some [match (match-fn coll)]
      [match])

    (map? coll)
    (eduction
      (mapcat
        (fn [entry]
          (search (val entry) match-fn)))
      coll)

    (coll? coll)
    (eduction
      (mapcat
        (fn [value]
          (search value match-fn)))
      coll)

    :else
    (when-some [match (match-fn coll)]
      [match])))

(defn search-with-path
  "Traverses the supplied collection hierarchy recursively, applying the
  specified match-fn to every non-collection value. Records are considered
  values, not collections. Returns a sequence of pairs of every non-nil result
  returned by the match-fn and the path to the match from the root of the
  collection. Path tokens will be conjoined onto the supplied init-path at each
  level and the resulting path will be part of the matching result pair."
  [coll init-path match-fn]
  (cond
    (record? coll)
    (when-some [match (match-fn coll)]
      [(pair match init-path)])

    (map? coll)
    (eduction
      (mapcat
        (fn [[key value]]
          (search-with-path value (conj init-path key) match-fn)))
      coll)

    (coll? coll)
    (eduction
      (mapcat-indexed
        (fn [index value]
          (search-with-path value (conj init-path index) match-fn)))
      coll)

    :else
    (when-some [match (match-fn coll)]
      [(pair match init-path)])))

(defn sorted-assoc-in-empty-fn
  "An empty-fn for use with assoc-in-ex. Returns vectors for integer keys and
  sorted maps for non-integer keys."
  [_ _ nested-key-path]
  (if (integer? (first nested-key-path))
    []
    (sorted-map)))

(defn default-assoc-in-empty-fn
  "An empty-fn for use with assoc-in-ex. Returns vectors for integer keys and
  hash maps for non-integer keys."
  [_ _ nested-key-path]
  (if (integer? (first nested-key-path))
    []
    {}))

(defn assoc-in-ex
  "Like core.assoc-in, but the supplied empty-fn will be called whenever an
  intermediate collection along the key-path does not yet exist. The empty-fn is
  expected to return an empty collection that we will associate entries into.
  Supplying (constantly {}) as the empty-fn will mimic the behavior of the
  core.assoc-in function. The arguments supplied to the empty-fn are the parent
  collection, the key in the parent collection where we expected to find a
  nested collection, and the remaining key-path that will address into the
  missing nested collection. If coll is nil, the empty-fn will be called with
  nil, nil key-path. If no empty-fn is supplied, the default empty-fn that
  produces vectors for integer keys and hash-maps for other keys will be used."
  ([coll key-path value]
   (assoc-in-ex coll key-path value default-assoc-in-empty-fn))
  ([coll key-path value empty-fn]
   {:pre [(ifn? empty-fn)]}
   (let [key (key-path 0)
         nested-key-path (subvec key-path 1)
         coll (if (some? coll)
                coll
                (empty-fn nil nil key-path))]
     (assoc coll
       key
       (if (zero? (count nested-key-path))
         value
         (assoc-in-ex (if-some [nested-coll (get coll key)]
                        nested-coll
                        (empty-fn coll key nested-key-path))
           nested-key-path value empty-fn))))))

(def xform-nested-map->path-map
  "Transducer that takes a nested map and returns a flat map of vector paths to
  the innermost values."
  (letfn [(path-entries [path [key value]]
            (let [path (conj path key)]
              (if (coll? value)
                (eduction
                  (mapcat #(path-entries path %))
                  value)
                [(pair path value)])))]
    (mapcat #(path-entries [] %))))

(defn nested-map->path-map
  "Takes a nested map and returns a flat map of vector paths to the innermost
  values."
  [nested-map]
  {:pre [(map? nested-map)]}
  (into (empty nested-map)
        xform-nested-map->path-map
        nested-map))

(defn path-map->nested-map
  "Takes a flat map of vector paths to values and returns a nested map to the
  same values."
  [path-map]
  {:pre [(map? path-map)]}
  (let [empty-fn (if (sorted? path-map)
                   sorted-assoc-in-empty-fn
                   default-assoc-in-empty-fn)]
    (reduce (fn [nested-map [path value]]
              (assoc-in-ex nested-map path value empty-fn))
            (empty path-map)
            path-map)))

(defn- preserving-reduced [rf]
  #(let [result (rf %1 %2)]
     (cond-> result (reduced? result) reduced)))

(defn flatten-xf
  "Like clojure.core/flatten, but transducer and treats nils as empty sequences"
  [rf]
  (fn xf
    ([] (rf))
    ([result] (rf result))
    ([result input]
     (cond
       (nil? input) result
       (sequential? input) (reduce (preserving-reduced xf) result input)
       :else (rf result input)))))

(defn some
  "Like clojure.core/some, but uses reduce instead of lazy sequences"
  [pred coll]
  (reduce (fn [_ v]
            (when-let [ret (pred v)]
              (reduced ret)))
          nil
          coll))

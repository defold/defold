;; Copyright 2020-2026 The Defold Foundation
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
  (:refer-clojure :exclude [any? bounded-count empty? every? mapcat merge merge-with not-any? not-empty not-every? some update-vals])
  (:import [clojure.core Eduction Vec]
           [clojure.lang Cons Cycle IEditableCollection LazySeq MapEntry Repeat]
           [java.util ArrayList Arrays List]
           [java.util.concurrent.atomic AtomicInteger]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def empty-map {})
(def empty-sorted-map (sorted-map))
(def empty-set #{})
(def empty-sorted-set (sorted-set))

(defmacro transfer
  "Transfer the sequence supplied as the first argument into the destination
  collection specified as the second argument, using a transducer composed of
  the remaining arguments. Returns the resulting collection. Supplying :eduction
  as the destination returns an eduction instead.

  See also: transform, transmute, transmute-kv, transpire!."
  ([from to]
   (case to
     :eduction `(->Eduction identity ~from)
     `(into ~to
            ~from)))
  ([from to xform]
   (case to
     :eduction `(->Eduction ~xform ~from)
     `(into ~to
            ~xform
            ~from)))
  ([from to xform & xforms]
   (case to
     :eduction `(->Eduction (comp ~xform ~@xforms) ~from)
     `(into ~to
            (comp ~xform ~@xforms)
            ~from))))

(defmacro transpire!
  "Runs (via reduce) the side-effecting item-fn! supplied as the last argument,
  on successive items in the collection supplied as the first argument. Any
  additional arguments supplied between the first and last arguments will be
  composed into a transducer. Returns nil.

  See also: transfer, transform, transmute, transmute-kv."
  [coll item-fn! & more]
  (case (count more)
    0 `(run! ~item-fn! ~coll)
    1 (let [xform item-fn!
            item-fn! (first more)]
        `(run! ~item-fn!
               (->Eduction ~xform ~coll)))
    (let [first-xform item-fn!
          more-xforms (butlast more)
          item-fn! (last more)]
      `(run! ~item-fn!
             (->Eduction (comp ~first-xform ~@more-xforms)
                         ~coll)))))

(defmacro transmute
  "Similar to core.transduce or core.reduce, but takes the input sequence as the
  first argument, followed by a mandatory init value, and the acc-fn as the last
  argument. Any additional arguments specified between the init value and the
  acc-fn will be composed into a transducer. The acc-fn is assumed to take
  two arguments and will be used with reduce when no additional transducers are
  supplied. When transducers are supplied, we will wrap the acc-fn in a
  multi-arity function suitable for use with core.transduce.

  See also: transfer, transform, transmute-kv, transpire!."
  [coll init acc-fn & more]
  (case (count more)
    0 `(reduce ~acc-fn ~init ~coll)
    1 (let [xform acc-fn
            acc-fn (first more)]
        `(transduce ~xform
                    (let [~'acc-fn ~acc-fn]
                      (fn
                        ([~'acc] ~'acc)
                        ([~'acc ~'item] (~'acc-fn ~'acc ~'item))))
                    ~init
                    ~coll))
    (let [first-xform acc-fn
          more-xforms (butlast more)
          acc-fn (last more)]
      `(transduce (comp ~first-xform ~@more-xforms)
                  (let [~'acc-fn ~acc-fn]
                    (fn
                      ([~'acc] ~'acc)
                      ([~'acc ~'item] (~'acc-fn ~'acc ~'item))))
                  ~init
                  ~coll))))

(defmacro transmute-kv
  "Similar to core.reduce-kv, but takes the input sequence as the first
  argument, followed by a mandatory init value, and the acc-fn as the last
  argument. Any additional arguments specified between the init value and the
  acc-fn will be composed into a transducer. The acc-fn is assumed to take three
  arguments and will be used with reduce-kv when no additional transducers are
  supplied. When transducers are supplied, we will wrap the acc-fn in a
  multi-arity function suitable for use with core.transduce.

  See also: transfer, transform, transmute, transpire!."
  [coll init acc-fn & more]
  (case (count more)
    0 `(reduce-kv ~acc-fn ~init ~coll)
    1 (let [xform acc-fn
            acc-fn (first more)]
        `(transduce ~xform
                    (let [~'acc-fn ~acc-fn]
                      (fn
                        ([~'acc] ~'acc)
                        ([~'acc [~'k ~'v]] (~'acc-fn ~'acc ~'k ~'v))))
                    ~init
                    ~coll))
    (let [first-xform acc-fn
          more-xforms (butlast more)
          acc-fn (last more)]
      `(transduce (comp ~first-xform ~@more-xforms)
                  (let [~'acc-fn ~acc-fn]
                    (fn
                      ([~'acc] ~'acc)
                      ([~'acc [~'k ~'v]] (~'acc-fn ~'acc ~'k ~'v))))
                  ~init
                  ~coll))))

(defn comparable-value?
  "Returns true if the value is compatible with the default comparator used with
  sorted maps and sets."
  [value]
  (or (nil? value)
      (instance? Comparable value)))

(defn key-set
  "Returns an unordered set with all keys from the supplied map."
  [coll]
  (into #{}
        (map key)
        coll))

(defn sorted-key-set
  "Returns a sorted set with all keys from the supplied map."
  [coll]
  (into (sorted-set)
        (map key)
        coll))

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

(defn flip
  "Given a pair, returns a new pair with the elements flipped."
  [[a b]]
  (MapEntry. b a))

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

    (or (counted? coll)
        (.isArray (class coll)))
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

(definline lazy?
  "Returns true if the supplied value is a lazily evaluated collection. Lazy
  sequences might be infinite or may retain references to large objects that
  prevent them from being garbage-collected. Does not check if elements in the
  collection are themselves lazily-evaluated."
  [value]
  `(condp identical? (class ~value)
     LazySeq true
     Eduction true
     Repeat true
     Cycle true
     false))

(defn eager-seqable?
  "Returns true if the supplied value is seqable and eagerly evaluated. Useful
  in places where you want a keep around a reference to a seqable object without
  realizing it into a concrete collection. Note that this function will return
  true for nil, since it is a valid seqable value."
  [value]
  (and (not (lazy? value))
       (seqable? value)))

(defonce conj-set (fnil conj #{}))

(defonce conj-vector (fnil conj []))

(defonce into-set (fnil into #{}))

(defonce into-vector (fnil into []))

(defn transform
  "Transform the collection supplied as the first argument into a new collection
  of the same type, using a transducer composed of the remaining arguments.
  Preserves metadata. Returns coll unaltered if empty or if no transducers are
  supplied.

  See also: transfer, transmute, transmute-kv, transpire!."
  ([coll] coll)
  ([coll xform]
   (cond
     (empty? coll)
     coll

     (record? coll)
     (transduce xform
                (fn
                  ([coll] coll)
                  ([coll [key value]] (assoc coll key value)))
                coll
                coll)

     :else
     (into (empty-with-meta coll)
           xform
           coll)))
  ([coll xform & xforms]
   (if (empty? coll)
     coll
     (transform coll (apply comp xform xforms)))))

(defn update-vals
  "Like core.update-vals, but retains the type of the input map or record. Also
  accepts additional arguments to f. Preserves metadata. If coll is nil, returns
  nil without calling f."
  [coll f & args]
  (when coll
    (let [use-transient (supports-transient? coll)
          rf (if use-transient
               (if (empty? args)
                 #(assoc! %1 %2 (f %3))
                 #(assoc! %1 %2 (apply f %3 args)))
               (if (empty? args)
                 #(assoc %1 %2 (f %3))
                 #(assoc %1 %2 (apply f %3 args))))
          init (if (record? coll)
                 coll
                 (cond-> (empty coll)
                         use-transient transient))]
      (with-meta (cond-> (reduce-kv rf init coll)
                         use-transient persistent!)
                 (meta coll)))))

(defn update-vals-kv
  "Like core.update-vals, but calls f with both the key and the value of each
  map entry. Also retains the type of the input map or record and accepts
  additional arguments to f. Preserves metadata. If coll is nil, returns nil
  without calling f."
  [coll f & args]
  (when coll
    (let [use-transient (supports-transient? coll)
          rf (if use-transient
               (if (empty? args)
                 #(assoc! %1 %2 (f %2 %3))
                 #(assoc! %1 %2 (apply f %2 %3 args)))
               (if (empty? args)
                 #(assoc %1 %2 (f %2 %3))
                 #(assoc %1 %2 (apply f %2 %3 args))))
          init (if (record? coll)
                 coll
                 (cond-> (empty coll)
                         use-transient transient))]
      (with-meta (cond-> (reduce-kv rf init coll)
                         use-transient persistent!)
                 (meta coll)))))

(defn map-vals
  "Applies f to all values in the supplied associative collection. Returns a new
  associative collection of the same type with all the same keys, but the values
  being the results of applying f to each previous value. Preserves metadata. If
  coll is nil, returns nil without calling f."
  ([f]
   (map (fn [entry]
          (pair (key entry)
                (f (val entry))))))
  ([f coll]
   (update-vals coll f)))

(defn map-vals-kv
  "Calls f with the key and value of each element in the supplied associative
  collection. Returns a new associative collection of the same type with all the
  same keys, but the values being the results of applying f to each previous key
  and value. Preserves metadata. If coll is nil, returns nil without calling f."
  ([f]
   (map (fn [entry]
          (let [k (key entry)
                v (val entry)]
            (pair k
                  (f k v))))))
  ([f coll]
   (update-vals-kv coll f)))

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

(defn merge
  "Like core.merge, but makes use of transients for efficiency, and ignores
  empty collections (even in LHS position!). Also works with sets."
  ([] nil)
  ([a] a)
  ([a b]
   (cond
     (empty? a) b
     (empty? b) a
     (and (map? a) (supports-transient? a) (map? b)) (-> (reduce-kv assoc! (transient a) b) persistent! (with-meta (meta a)))
     :else (into a b)))
  ([a b & maps]
   (reduce merge
           (merge a b)
           maps)))

(defn merge-with
  "Like core.merge-with, but makes use of transients for efficiency, and ignores
  empty collections (even in LHS position!)."
  ([_f] nil)
  ([_f a] a)
  ([f a b]
   (cond
     (empty? a) b
     (empty? b) a

     :else
     (letfn [(merged-value [b-key b-value]
               (let [a-value (get a b-key ::not-found)]
                 (case a-value
                   ::not-found b-value
                   (f a-value b-value))))]
       (if (supports-transient? a)
         (-> (reduce (fn [result [b-key b-value]]
                       (assoc! result b-key (merged-value b-key b-value)))
                     (transient a)
                     b)
             (persistent!)
             (with-meta (meta a)))
         (reduce-kv (fn [result b-key b-value]
                      (assoc result b-key (merged-value b-key b-value)))
                    a
                    b)))))
  ([f a b & maps]
   (reduce #(merge-with f %1 %2)
           (merge-with f a b)
           maps)))

(defn merge-with-kv
  "Similar to merge-with, but the conflict function is called with three
  arguments: The key, the value, and the conflicting value."
  ([_f] nil)
  ([_f a] a)
  ([f a b]
   (cond
     (empty? a) b
     (empty? b) a

     :else
     (letfn [(merged-value [b-key b-value]
               (let [a-value (get a b-key ::not-found)]
                 (case a-value
                   ::not-found b-value
                   (f b-key a-value b-value))))]
       (if (supports-transient? a)
         (-> (reduce-kv (fn [result b-key b-value]
                          (assoc! result b-key (merged-value b-key b-value)))
                        (transient a)
                        b)
             (persistent!)
             (with-meta (meta a)))
         (reduce (fn [result [b-key b-value]]
                   (assoc result b-key (merged-value b-key b-value)))
                 a
                 b)))))
  ([f a b & maps]
   (reduce #(merge-with-kv f %1 %2)
           (merge-with-kv f a b)
           maps)))

(defn deep-merge
  "Deep-merge the supplied maps. Values from later maps will overwrite values in
  the previous maps. Records can be merged with maps, but are otherwise treated
  as values. Any non-map collections are treated as values. Types and metadata
  are preserved."
  ([] nil)
  ([a] a)
  ([a b]
   (cond
     (and (record? b) (record? a)) b
     (or (not (map? a)) (empty? a)) b
     (and (map? b) (not (empty? b))) (merge-with deep-merge a b)
     :else a))
  ([a b & maps]
   (reduce deep-merge
           (deep-merge a b)
           maps)))

(defn partition-all-float-arrays
  "Returns a lazy sequence of float arrays. Like core.partition-all, but creates
  new float arrays for each partition. Returns a stateful transducer when no
  collection is provided."
  ([^long partition-length]
   (fn [rf]
     (let [in-progress (float-array partition-length)
           in-progress-index (AtomicInteger.)]
       (fn
         ([] (rf))
         ([result]
          (let [finished-length (.getAndSet in-progress-index 0)
                result (if (zero? finished-length)
                         result
                         (let [finished (Arrays/copyOf in-progress finished-length)]
                           (unreduced (rf result finished))))]
            (rf result)))
         ([result input]
          (let [written-index (.getAndIncrement in-progress-index)
                finished-length (inc written-index)]
            (aset-float in-progress written-index input)
            (if (= partition-length finished-length)
              (let [finished (Arrays/copyOf in-progress partition-length)]
                (.set in-progress-index 0)
                (rf result finished))
              result)))))))
  ([^long partition-length coll]
   (partition-all-float-arrays partition-length partition-length coll))
  ([^long partition-length ^long step coll]
   (lazy-seq
     (when-let [in-progress (seq coll)]
       (let [finished (float-array (take partition-length in-progress))]
         (cons finished (partition-all-float-arrays partition-length step (nthrest in-progress step))))))))

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

(defn reduce-partitioned
  "Partitions coll into the requested partition-length, then reduces using the
  accumulate-fn with the resulting arguments from each partition. If coll cannot
  be evenly partitioned, throws IllegalArgumentException."
  [^long partition-length accumulate-fn init coll]
  (if (pos-int? partition-length)
    (transduce
      (partition-all partition-length)
      (fn
        ([result] result)
        ([result partition]
         (case (rem (count partition) partition-length)
           0 (apply accumulate-fn result partition)
           (throw (IllegalArgumentException. "The length of coll must be a multiple of the partition-length.")))))
      init
      coll)
    (throw (IllegalArgumentException. "The partition-length must be positive."))))

(definline index-of
  "Returns the index of the first item in the supplied java.util.List that
  equals the specified value. Returns -1 if there is no match."
  [^List coll value]
  `(List/.indexOf ~coll ~value))

(definline last-index-of
  "Returns the index of the last item in the supplied java.util.List that equals
  the specified value. Returns -1 if there is no match."
  [^List coll value]
  `(List/.lastIndexOf ~coll ~value))

(defn remove-index
  "Removes an item at the specified position in a vector"
  [coll ^long index]
  (-> (into (subvec coll 0 index)
            (subvec coll (inc index)))
      (with-meta (meta coll))))

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
  "Aggregate a sequence of key-value pairs into an associative collection. For
  each key-value pair, we will look up the key in coll and run the accumulate-fn
  on the existing value and the value from the key-value pair. The result will
  be assoc:ed into coll. Optionally, an init value can be specified to use as
  the first argument to the accumulate-fn when there is no existing value for
  the key in coll. If no init value is supplied, the initial value will be
  obtained by calling the accumulate-fn with no arguments at the start. If init
  is a function, it will be called for each unseen key to produce an init
  value for it."
  ([coll accumulate-fn pairs]
   (aggregate-into coll accumulate-fn (accumulate-fn) pairs))
  ([coll accumulate-fn init pairs]
   (if (empty? pairs)
     coll
     (let [use-transient (supports-transient? coll)
           use-fn-init (fn? init)
           assoc-fn (if use-transient assoc! assoc)
           lookup-fn (if use-fn-init
                       (fn lookup-fn [accumulated-by-key key]
                         (let [accumulated (get accumulated-by-key key ::not-found)]
                           (case accumulated
                             ::not-found (init key)
                             accumulated)))
                       (fn lookup-fn [accumulated-by-key key]
                         (get accumulated-by-key key init)))]
       (cond-> (reduce (fn [accumulated-by-key [key value]]
                         (let [accumulated (lookup-fn accumulated-by-key key)
                               accumulated (accumulate-fn accumulated value)]
                           (assoc-fn accumulated-by-key key accumulated)))
                       (cond-> coll use-transient transient)
                       pairs)
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
  (transform nested-map xform-nested-map->path-map))

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

(defn preserving-reduced [rf]
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

(defn tree-xf
  "Like clojure.core/tree-seq, but transducer"
  [branch? children]
  (fn [rf]
    (fn xf
      ([] (rf))
      ([result] (rf result))
      ([result input]
       (let [result (rf result input)]
         (if (or (reduced? result) (not (branch? input)))
           result
           (reduce (preserving-reduced xf) result (children input))))))))

(defn find-values
  "Performs a recursive search in the supplied collection. Returns a sequence of
  all values that match the specified predicate. If the predicate returns true
  for a collection, it will be included in the result. Otherwise, its values
  will be recursively traversed to find additional matches. For maps, the
  predicate will be called on the value of each entry, and the key is ignored.
  Returns a stateless transducer if no input collection is provided."
  ([pred]
   (fn [rf]
     (fn xf
       ([] (rf))
       ([result] (rf result))
       ([result input]
        (cond
          (pred input) (rf result input)
          (nil? input) result
          (map? input) (reduce (preserving-reduced xf) result (vals input))
          (seqable? input) (reduce (preserving-reduced xf) result input)
          :else result)))))
  ([pred coll]
   (sequence (find-values pred) coll)))

(defn first-where
  "Returns the first element in coll where pred returns true, or nil if there
  was no matching element. If coll is a map, the elements are key-value pairs."
  [pred coll]
  (reduce
    (fn [_ item]
      (when (pred item)
        (reduced item)))
    nil
    coll))

(defn first-index-where
  "Returns the index of the first element in coll where pred returns true,
  or nil if there was no matching element. If coll is a map, the elements are
  key-value pairs."
  [pred coll]
  (let [index (reduce
                (fn [^long index item]
                  (if (pred item)
                    (reduced (reduced index))
                    (inc index)))
                0
                coll)]
    (when (reduced? index)
      (unreduced index))))

(defn some
  "Like clojure.core/some, but uses reduce instead of lazy sequences."
  [pred coll]
  (reduce (fn [_ v]
            (when-let [ret (pred v)]
              (reduced ret)))
          nil
          coll))

(defn any?
  "Returns whether any item in the supplied collection matches the predicate. Do
  not confuse with clojure.core/any?, which takes a single argument and always
  returns true."
  [pred coll]
  (boolean (some pred coll)))

(defn not-any?
  "Like clojure.core/not-any?, but uses reduce instead of lazy sequences."
  [pred coll]
  (not (some pred coll)))

(defn every?
  "Like clojure.core/every?, but uses reduce instead of lazy sequences."
  [pred coll]
  (reduce (fn [result item]
            (if (pred item)
              result
              (reduced false)))
          true
          coll))

(defn not-every?
  "Like clojure.core/not-every?, but uses reduce instead of lazy sequences."
  [pred coll]
  (not (every? pred coll)))

(defn str-rf
  "Reducing function for string concatenation, useful for transduce context"
  ([] (StringBuilder.))
  ([^StringBuilder result] (.toString result))
  ([^StringBuilder acc input] (.append acc (str input))))

(defn join-to-string
  "Like clojure.string/join, but uses reduce instead of lazy sequences"
  ([coll]
   (transduce identity str-rf coll))
  ([sep coll]
   (transduce (interpose (str sep)) str-rf coll)))

(defn unanimous-value
  "Iterates over all elements in the collection. If they are all equal, return
  the last element, otherwise return not-found. Returns not-found if the
  collection is empty. If no value is provided for not-found, use nil."
  ([coll]
   (unanimous-value coll nil))
  ([coll not-found]
   (let [consensus (reduce
                     (fn [prev-value value]
                       (if (or (= prev-value value)
                               (= ::undefined prev-value))
                         value
                         (reduced not-found)))
                     ::undefined
                     coll)]
     (if (= ::undefined consensus)
       not-found
       consensus))))

(defonce ^:private primitive-types-by-array-manager-id
  (into {}
        (map (fn [primitive-type]
               (let [^Vec primitive-vector (vector-of primitive-type)
                     array-manager (.am primitive-vector)
                     array-manager-id (System/identityHashCode array-manager)]
                 (pair array-manager-id primitive-type))))
        [:boolean :char :byte :short :int :long :float :double]))

(defn primitive-vector-type
  "Returns a keyword reflecting the primitive type stored in the specified
  primitive vector, or nil if coll is not a primitive collection."
  [^Vec coll]
  (when (instance? Vec coll)
    (let [array-manager-id (System/identityHashCode (.am coll))]
      (primitive-types-by-array-manager-id array-manager-id))))

(defn mapv>
  "Like core.mapv, but takes the input sequence as the first argument and
  supplies any arguments following the transform function to it after the item
  argument. Useful with various core functions such as update."
  ([coll f]
   (mapv f coll))
  ([coll f & args]
   (mapv #(apply f % args) coll)))

(ns
  ^{:doc "An adaptation of Okasaki and Gill's \"Fast Mergeable Integer Maps`\",
          which can be found at http://ittc.ku.edu/~andygill/papers/IntMap98.pdf"}
  clojure.data.int-map-fixed
  (:refer-clojure
    :exclude [merge merge-with update range])
  (:require
    [clojure.core.reducers :as r])
  (:import
    [java.util
     BitSet
     Map
     Map$Entry]
    [clojure.data.int_map
     INode
     IntSet
     Nodes$Empty
     INode$IterationType]
    [clojure.lang Util APersistentMap]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* true)

;;;

(defn ^:private default-merge [_ x]
  x)

(definterface IRange
  (range [^long min ^long max]))

(definterface IRadix
  (mergeWith [b f])
  (update [k f]))

(defmacro ^:private compile-if [test then else]
  (if (eval test)
    then
    else))

(declare ->transient-int-map)

(deftype PersistentIntMap
  [^INode root
   ^long epoch
   meta]

  IRange
  (range [_ min max]
    (PersistentIntMap.
      (or (.range root min max) Nodes$Empty/EMPTY)
      epoch
      meta))

  IRadix
  (mergeWith [_ b f]
    (let [^PersistentIntMap b b
          epoch' (inc (Math/max (.epoch b) epoch))]
      (PersistentIntMap.
        (.merge root (.root b) epoch' f)
        epoch'
        meta)))

  (update [_ k f]
    (let [epoch' (inc epoch)
          root' (.update root k epoch' f)]
      (PersistentIntMap. root' epoch' meta)))

  clojure.lang.IObj
  (meta [_] meta)
  (withMeta [_ m] (PersistentIntMap. root epoch m))

  clojure.lang.MapEquivalence

  clojure.lang.Counted
  (count [this]
    (.count root))

  clojure.lang.IPersistentCollection

  (equiv [this x]
    (cond
      (not (map? x))
      false

      (and (instance? clojure.lang.IPersistentMap x)
           (not (instance? clojure.lang.MapEquivalence x)))
      false

      (not= (count this) (.size ^Map x))
      false

      (instance? PersistentIntMap x)
      (loop [it1 (.iterator this)
             it2 (.iterator ^PersistentIntMap x)]
        (if (and (.hasNext it1) (.hasNext it2))
          (let [^Map$Entry e1 (.next it1)
                ^Map$Entry e2 (.next it2)]
            (if (and e1 e2)
              (if (and (= (.getKey e1) (.getKey e2))
                       (Util/equiv (.getValue e1) (.getValue e2)))
                (recur it1 it2)
                false)
              false))
          true))

      :else
      (loop [it (.iterator this)]
        (if (.hasNext it)
          (let [^Map$Entry e (.next it)]
            (if e
              (if (and (.containsKey ^Map x (.getKey e))
                       (Util/equiv (.getValue e) (.get ^Map x (.getKey e))))
                (recur it)
                false)
              false))
          true))))

  (cons [this o]
    (if (map? o)
      (reduce #(apply assoc %1 %2) this o)
      (.assoc this (nth o 0) (nth o 1))))

  clojure.lang.Seqable
  (seq [this]
    (iterator-seq (.iterator this)))

  clojure.lang.Reversible
  (rseq [_]
    (iterator-seq (.iterator root INode$IterationType/ENTRIES true)))

  r/CollFold

  (coll-fold [this n combinef reducef]
    (#'r/fjinvoke #(try (.fold root n combinef reducef #'r/fjtask #'r/fjfork #'r/fjjoin) (catch Throwable e (.printStackTrace e)))))

  clojure.core.protocols.CollReduce

  (coll-reduce
    [this f]
    (let [x (.reduce root f (f))]
      (if (reduced? x)
        @x
        x)))

  (coll-reduce
    [this f val]
    (let [x (.reduce root f val)]
      (if (reduced? x)
        @x
        x)))

  clojure.core.protocols.IKVReduce
  (kv-reduce
    [this f val]
    (let [x (.kvreduce root f val)]
      (if (reduced? x)
        @x
        x)))

  Object
  (hashCode [this]
    (reduce
      (fn [acc [k v]]
        (unchecked-add acc (bit-xor (.hashCode k) (.hashCode v))))
      0
      (seq this)))

  clojure.lang.IHashEq
  (hasheq [this]
    (compile-if (resolve 'clojure.core/hash-unordered-coll)
      (hash-unordered-coll this)
      (.hashCode this)))

  (equals [this x]
    (cond
      (identical? this x)
      true

      (not (map? x))
      false

      (and (instance? clojure.lang.IPersistentMap x)
           (not (instance? clojure.lang.MapEquivalence x)))
      false

      (not= (count this) (.size ^Map x))
      false

      (instance? PersistentIntMap x)
      (loop [it1 (.iterator this)
             it2 (.iterator ^PersistentIntMap x)]
        (if (and (.hasNext it1) (.hasNext it2))
          (let [^Map$Entry e1 (.next it1)
                ^Map$Entry e2 (.next it2)]
            (if (and e1 e2)
              (if (and (= (.getKey e1) (.getKey e2))
                       (Util/equals (.getValue e1) (.getValue e2)))
                (recur it1 it2)
                false)
              false))
          true))

      :else
      (loop [it (.iterator this)]
        (if (.hasNext it)
          (let [^Map$Entry e (.next it)]
            (if e
              (if (and (.containsKey ^Map x (.getKey e))
                       (Util/equals (.getValue e) (.get ^Map x (.getKey e))))
                (recur it)
                false)
              false))
          true))))

  (toString [this]
    (str (into {} this)))

  clojure.lang.ILookup
  (valAt [this k]
    (.valAt this k nil))
  (valAt [this k default]
    (.get root (long k) default))

  clojure.lang.Associative
  (containsKey [this k]
    (not (identical? ::not-found (.valAt this k ::not-found))))

  (entryAt [this k]
    (let [v (.valAt this k ::not-found)]
      (when (not= v ::not-found)
        (clojure.lang.MapEntry. k v))))

  (assoc [this k v]
    (let [k (long k)
          epoch' (inc epoch)]
      (PersistentIntMap.
        (.assoc root (long k) epoch' default-merge v)
        epoch'
        meta)))

  (empty [this]
    (PersistentIntMap. Nodes$Empty/EMPTY 0 nil))

  clojure.lang.IEditableCollection
  (asTransient [this]
    (->transient-int-map root (inc epoch) meta))

  java.util.Map
  (get [this k]
    (.valAt this k))
  (isEmpty [this]
    (empty? (seq this)))
  (size [this]
    (count this))
  (keySet [this]
    (->> this
      seq
      (map key)
      set))
  (put [_ _ _]
    (throw (UnsupportedOperationException.)))
  (putAll [_ _]
    (throw (UnsupportedOperationException.)))
  (clear [_]
    (throw (UnsupportedOperationException.)))
  (remove [_ _]
    (throw (UnsupportedOperationException.)))
  (values [this]
    (->> this seq (map second)))
  (entrySet [this]
    (->> this seq set))
  (iterator [this]
    (.iterator root INode$IterationType/ENTRIES false))

  clojure.lang.IPersistentMap
  (assocEx [this k v]
    (if (contains? this k)
      (throw (Exception. "Key or value already present"))
      (assoc this k v)))
  (without [this k]
    (let [k (long k)
          epoch' (inc epoch)]
      (PersistentIntMap.
        (or (.dissoc root k epoch') Nodes$Empty/EMPTY)
        epoch'
        meta)))

  clojure.lang.IFn

  (invoke [this k]
    (.valAt this k))

  (invoke [this k default]
    (.valAt this k default)))

(deftype TransientIntMap
  [^INode root
   ^long epoch
   meta]

  IRadix
  (mergeWith [this b f]
    (throw (IllegalArgumentException. "Cannot call `merge-with` on transient int-map.")))

  (update [this k f]
    (let [root' (.update root k epoch f)]
      (if (identical? root root')
        this
        (TransientIntMap. root' epoch meta))))

  clojure.lang.IObj
  (meta [_] meta)
  (withMeta [_ m] (TransientIntMap. root (inc epoch) meta))

  clojure.lang.Counted
  (count [this]
    (.count root))

  clojure.lang.MapEquivalence

  (equiv [this x]
    (and (map? x) (= x (into {} this))))

  clojure.lang.Seqable
  (seq [this]
    (iterator-seq (.iterator root INode$IterationType/ENTRIES false)))

  Object
  (hashCode [this]
    (reduce
      (fn [acc [k v]]
        (unchecked-add acc (bit-xor (hash k) (hash v))))
      0
      (seq this)))

  (equals [this x]
    (or (identical? this x)
      (and
        (map? x)
        (= x (into {} this)))))

  (toString [this]
    (str (into {} this)))

  clojure.lang.ILookup
  (valAt [this k]
    (.valAt this k nil))
  (valAt [this k default]
    (.get root k default))

  clojure.lang.Associative
  (containsKey [this k]
    (not (identical? ::not-found (.valAt this k ::not-found))))

  (entryAt [this k]
    (let [v (.valAt this k ::not-found)]
      (when (not= v ::not-found)
        (clojure.lang.MapEntry. k v))))

  clojure.lang.ITransientMap

  (assoc [this k v]
    (let [k (long k)
          root' (.assoc root (long k) epoch default-merge v)]
      (if (identical? root' root)
        this
        (TransientIntMap. root' epoch meta))))

  (conj [this o]
    (if (map? o)
      (reduce #(apply assoc! %1 %2) this o)
      (.assoc this (nth o 0) (nth o 1))))

  (persistent [_]
    (PersistentIntMap. root (inc epoch) meta))

  (without [this k]
    (let [root' (or (.dissoc root (long k) epoch) Nodes$Empty/EMPTY)]
      (if (identical? root' root)
        this
        (TransientIntMap. root' epoch meta))))

  clojure.lang.IFn

  (invoke [this k]
    (.valAt this k))

  (invoke [this k default]
    (.valAt this k default)))

(defn- ->transient-int-map [root ^long epoch meta]
  (TransientIntMap. root epoch meta))

;;;

(defn int-map
  "Creates an integer map that can only have non-negative integers as keys."
  ([]
     (PersistentIntMap. Nodes$Empty/EMPTY 0 nil))
  ([a b]
     (assoc (int-map) a b))
  ([a b & rest]
     (apply assoc (int-map) a b rest)))

(defn merge-with
  "Merges together two int-maps, using `f` to resolve value conflicts."
  ([f]
     (int-map))
  ([f a b]
     (let [a' (if (instance? TransientIntMap a)
                (persistent! a)
                a)
           b'  (if (instance? TransientIntMap b)
                 (persistent! b)
                 b)]
       (.mergeWith ^IRadix a' b' f)))
  ([f a b & rest]
     (reduce #(merge-with f %1 %2) (list* a b rest))))

(defn merge
  "Merges together two int-maps, giving precedence to values from the right-most map."
  ([]
     (int-map))
  ([a b]
     (merge-with (fn [_ b] b) a b))
  ([a b & rest]
     (apply merge-with (fn [_ b] b) a b rest)))

(defn update
  "Updates the value associated with the given key.  If no such key exist, `f` is invoked
   with `nil`."
  ([m k f]
     (.update ^PersistentIntMap m k f))
  ([m k f & args]
     (update m k #(apply f % args))))

(defn update!
  "A transient variant of `update`."
  ([m k f]
     (.update ^TransientIntMap m k f))
  ([m k f & args]
     (update! m k #(apply f % args))))

(defn range
  "Returns a map or set representing all elements within [min, max], inclusive."
  [x ^long min ^long max]
  (.range ^IRange x min max))

;;;

(declare ->transient-int-set ->persistent-int-set)

(deftype PersistentIntSet
  [^IntSet int-set
   ^long epoch
   meta]

  IRange
  (range [this min max]
    (let [epoch' (inc epoch)]
      (PersistentIntSet. (.range int-set epoch' min max) epoch' meta)))

  clojure.lang.Reversible
  (rseq [_]
    (iterator-seq (.elements int-set 0 true)))

  java.lang.Object
  (hashCode [this]
    (->> this
      (map #(bit-xor (long %) (unsigned-bit-shift-right (long %) 32)))
      (reduce +')))

  (equals [this x]
    (.equiv this x))

  clojure.lang.IHashEq
  (hasheq [this]
    (compile-if (resolve 'clojure.core/hash-unordered-coll)
      (hash-unordered-coll this)
      (.hashCode this)))

  java.util.Set
  (size [this] (count this))
  (isEmpty [this] (zero? (count this)))
  (iterator [this] (clojure.lang.SeqIterator. (seq this)))
  (containsAll [this s] (every? #(contains? this %) s))

  clojure.lang.IObj
  (meta [_] meta)
  (withMeta [this meta']
    (PersistentIntSet. int-set epoch meta'))

  clojure.lang.IEditableCollection
  (asTransient [this] (->transient-int-set this))

  clojure.lang.Seqable
  (seq [_]
    (iterator-seq (.elements int-set 0 false)))

  clojure.lang.IFn
  (invoke [this idx]
    (when (contains? this idx)
      idx))

  clojure.lang.IPersistentSet
  (equiv [this x]
    (and
      (set? x)
      (= (count this) (count x))
      (every?
        #(contains? x %)
        (seq this))))
  (count [_]
    (.count int-set))
  (empty [_]
    (PersistentIntSet. (IntSet. (.leafSize int-set)) 0 nil))
  (contains [_ n]
    (.contains int-set n))
  (disjoin [this n]
    (let [epoch' (inc epoch)
          int-set' (.remove int-set epoch' n)]
      (if (identical? int-set int-set')
        this
        (PersistentIntSet. int-set' epoch' meta))))
  (cons [this n]
    (let [epoch' (inc epoch)
          int-set' (.add int-set epoch' n)]
      (if (identical? int-set int-set')
        this
        (PersistentIntSet. int-set' epoch' meta)))))

(deftype TransientIntSet
  [^IntSet int-set
   ^int epoch
   meta]

  clojure.lang.IObj
  (meta [_] meta)
  (withMeta [this meta']
    (TransientIntSet. int-set epoch meta'))

  clojure.lang.IFn
  (invoke [this idx]
    (when (contains? this idx)
      idx))

  clojure.lang.ITransientSet
  (count [_]
    (.count int-set))
  (persistent [this] (->persistent-int-set this))
  (contains [_ n]
    (.contains int-set n))
  (disjoin [this n]
    (let [int-set' (.remove int-set epoch n)]
      (if (identical? int-set int-set')
        this
        (TransientIntSet. int-set' epoch meta))))
  (conj [this n]
    (let [int-set' (.add int-set epoch n)]
      (if (identical? int-set int-set')
        this
        (TransientIntSet. int-set' epoch meta)))))

(defn- ->persistent-int-set [^TransientIntSet set]
  (PersistentIntSet.
    (.int-set set)
    (.epoch set)
    (.meta set)))

(defn- ->transient-int-set [^PersistentIntSet set]
  (TransientIntSet.
    (.int-set set)
    (inc (.epoch set))
    (.meta set)))

;;;

(defn int-set
  "Creates an immutable set which can only store integral values.  This should be used unless elements are densely
   clustered (each element has multiple elements within +/- 1000)."
  ([]
     (PersistentIntSet. (IntSet. 128) 0 nil))
  ([s]
     (into (int-set) s)))

(defn dense-int-set
  "Creates an immutable set which can only store integral values.  This should be used only if elements are densely
   clustered (each element has multiple elements within +/- 1000)."
  ([]
     (PersistentIntSet. (IntSet. 4096) 0 nil))
  ([s]
     (into (dense-int-set) s)))

(defn union
  "Returns the union of two bitsets."
  [^PersistentIntSet a ^PersistentIntSet b]
  (let [epoch (inc (long (Math/max (.epoch a) (.epoch b))))]
    (PersistentIntSet.
      (.union ^IntSet (.int-set a) epoch (.int-set b))
      epoch
      nil)))

(defn intersection
  "Returns the intersection of two bitsets."
  [^PersistentIntSet a ^PersistentIntSet b]
  (let [epoch (inc (long (Math/max (.epoch a) (.epoch b))))]
    (PersistentIntSet.
      (.intersection ^IntSet (.int-set a) epoch (.int-set b))
      epoch
      nil)))

(defn difference
  "Returns the difference between two bitsets."
  [^PersistentIntSet a ^PersistentIntSet b]
  (let [epoch (inc (long (Math/max (.epoch a) (.epoch b))))]
    (PersistentIntSet.
      (.difference ^IntSet (.int-set a) epoch (.int-set b))
      epoch
      nil)))

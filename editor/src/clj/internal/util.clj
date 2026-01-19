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

(ns internal.util
  "Helpful utility functions for graph"
  (:require [camel-snake-kebab :refer [->Camel_Snake_Case_String]]
            [clojure.set :as set]
            [clojure.string :as str]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]))

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

(defn intov
  ([] [])
  ([to]
   {:pre [(or (nil? to) (vector? to))]}
   (or to []))
  ([to from]
   {:pre [(or (nil? to) (vector? to))]}
   (into (or to []) from))
  ([to xform from]
   {:pre [(or (nil? to) (vector? to))]}
   (into (or to []) xform from)))

(defn distinct-by
  "Returns a lazy sequence of the elements of coll with duplicates removed.
  Elements are considered equal if they return equivalent values for keyfn.
  If coll does contain duplicate elements, only the first equivalent is kept.
  Returns a stateful transducer when no collection is provided."
  ([keyfn]
   (fn [rf]
     (let [seen (volatile! #{})]
       (fn
         ([] (rf))
         ([result] (rf result))
         ([result input]
          (let [key (keyfn input)]
            (if (contains? @seen key)
              result
              (do (vswap! seen conj key)
                  (rf result input)))))))))
  ([keyfn coll]
   (let [step (fn step [xs seen]
                (lazy-seq
                  ((fn [[f :as xs] seen]
                     (when-let [s (seq xs)]
                       (let [key (keyfn f)]
                         (if (contains? seen key)
                           (recur (rest s) seen)
                           (cons f (step (rest s) (conj seen key)))))))
                   xs seen)))]
     (step coll #{}))))

(defn group-into
  "Like core.group-by, but you can specify the associative container to group
  into, as well as the empty collection to use for the groups. If the optional
  value-fn is supplied, it will be used to transform each element before adding
  them to the groups. It can also be a drop-in replacement for group-by that is
  slightly more efficient since it will make use of transient vectors."
  ([key-fn coll]
   (group-into {} [] key-fn nil coll))
  ([groups-container empty-group key-fn coll]
   (group-into groups-container empty-group key-fn nil coll))
  ([groups-container empty-group key-fn value-fn coll]
   (assert (associative? groups-container))
   (assert (coll? empty-group))
   (let [group-transient? (coll/supports-transient? empty-group)
         group-prepare (if group-transient? transient identity)
         group-conj (if group-transient? conj! conj)
         group-finish (if-not group-transient?
                        identity
                        (fn [transient-group]
                          (with-meta (persistent! transient-group)
                                     (meta empty-group))))
         groups-container-transient? (coll/supports-transient? groups-container)
         groups-container-prepare (if groups-container-transient? transient identity)
         groups-container-assoc (if groups-container-transient? assoc! assoc)
         groups-container-finish (cond
                                   (and groups-container-transient? group-transient?)
                                   (fn [transient-groups-container]
                                     (with-meta (persistent!
                                                  (reduce-kv (fn [transient-groups-container key value]
                                                               (assoc! transient-groups-container key (group-finish value)))
                                                             (transient (empty groups-container))
                                                             (persistent! transient-groups-container)))
                                                (meta groups-container)))

                                   groups-container-transient?
                                   (fn [transient-groups-container]
                                     (with-meta (persistent! transient-groups-container)
                                                (meta groups-container)))

                                   group-transient?
                                   (fn [groups-container]
                                     (reduce-kv (fn [groups-container key value]
                                                  (assoc groups-container key (group-finish value)))
                                                (empty groups-container)
                                                groups-container))

                                   :else
                                   identity)]
     (groups-container-finish
       (reduce (fn [groups-container elem]
                 (let [key (key-fn elem)
                       value (if value-fn (value-fn elem) elem)
                       group (or (get groups-container key)
                                 (group-prepare empty-group))]
                   (groups-container-assoc groups-container key (group-conj group value))))
               (groups-container-prepare groups-container)
               coll)))))

(defn filterm
  "like filter but applies the predicate to each key value pair of the map"
  [pred m]
  (into {} (filter pred m)))

(defmacro key-set
  "DEPRECATED. Use coll/key-set."
  [m]
  ;; TODO: Replace all calls so we can get rid of this macro.
  `(coll/key-set ~m))

(defn map-keys
  [f m]
  (reduce-kv (fn [m k v] (assoc m (f k) v)) (empty m) m))

(defmacro map-vals
  "DEPRECATED. Use coll/map-vals."
  [f m]
  ;; TODO: Replace all calls so we can get rid of this macro.
  `(coll/map-vals ~f ~m))

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
    (if (vector? x)
      x
      (vec x))
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

(defn var?! [v] (when (var? v) v))

(defn vgr [s] (some-> s resolve var?! var-get))

(defn protocol-symbol?
  [x]
  (and (symbol? x)
       (let [x' (vgr x)]
         (and (map? x') (contains? x' :on-interface)))))

(defn class-symbol?
  [x]
  (and (symbol? x)
       (class? (resolve x))))

(defn assert-form-kind [place required-kind-label required-kind-pred label form]
  (assert (not (nil? form))
          (str place " " label " requires a " required-kind-label " but got nil"))
  (assert (required-kind-pred form)
          (str place " " label " requires a " required-kind-label " not '" form "' of type " (type form))))

(defn fnk-arguments [f] (set (:arguments (meta f))))

(defn pfnk?
  "True if the function a valid production function"
  [f]
  (and (fn? f) (contains? (meta f) :arguments)))

(defn pfnksymbol? [x] (or (pfnk? x) (and (symbol? x) (pfnk? (vgr x)))))
(defn pfnkvar?    [x] (or (pfnk? x) (and (var? x) (fn? (var-get-recursive x)))))

(defn- quoted-var? [x] (and (seq? x) (= 'var (first x))))
(defn- pfnk-form?  [x] (and (seq? x) (symbol? (first x)) (= "fnk" (name (first x)))))

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
        (if (or (= :- (first forms))
                (= :as (first forms)))
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

(defn input-annotations [x]
  (cond
    (pfnk? x)
    (:annotations (meta x))

    (symbol? x)
    (recur (resolve x))

    (var? x)
    (recur (var-get x))

    (quoted-var? x)
    (recur (var-get (resolve (second x))))

    (pfnk-form? x)
    (into {}
          (comp
            (take-while (complement #{:as :-}))
            (map (juxt keyword meta)))
          (second (maybe-expand-macros x)))

    :else
    {}))

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

(defn update-paths
  [m paths xf]
  (reduce (fn [m [p v]] (assoc-in m p (xf p v (get-in m p)))) m paths))

(defn seq-starts-with?
  [coll subcoll]
  (if-not (seq subcoll)
    true
    (if-not (seq coll)
      false
      (and (= (first coll) (first subcoll))
           (recur (next coll) (next subcoll))))))

(defn count-where
  "Count the number of elements in coll where pred returns true. If max-counted
  is specified, will stop and return max-counted once the specified number of
  elements have passed the predicate."
  (^long [pred coll]
   (count-where Long/MAX_VALUE pred coll))
  (^long [^long max-counted pred coll]
   (assert (not (neg? max-counted)))
   (reduce (fn [^long num-counted elem]
             (if (= max-counted num-counted)
               (reduced max-counted)
               (if (pred elem)
                 (inc num-counted)
                 num-counted)))
           0
           coll)))

(defmacro first-where
  "DEPRECATED. Use coll/first-where.

  Returns the first element in coll where pred returns true, or nil if there was
  no matching element. If coll is a map, key-value pairs are passed to pred."
  [pred coll]
  ;; TODO: Replace all calls so we can get rid of this macro.
  `(coll/first-where ~pred ~coll))

(defmacro first-index-where
  "DEPRECATED. Use coll/first-index-where.

  Returns the index of the first element in coll where pred returns true,
  or nil if there was no matching element. If coll is a map, key-value
  pairs are passed to pred."
  [pred coll]
  ;; TODO: Replace all calls so we can get rid of this macro.
  `(coll/first-index-where ~pred ~coll))

(defn only
  "Returns the only element in coll, or nil if there are more than one element."
  [coll]
  (when (nil? (next coll))
    (first coll)))

(defn only-or-throw
  "Returns the only element in coll, or throw an error if the collection does not have exactly one element."
  [coll]
  (case (coll/bounded-count 2 coll)
    0 (throw (ex-info "The collection is empty." {:coll coll}))
    1 (first coll)
    (throw (ex-info "The collection has more than one element." {:coll coll}))))

(defn map->sort-key
  "Given any map, returns a vector that can be used to order it relative to any other map in a deterministic order."
  [m]
  (into []
        (mapcat (juxt identity (partial get m)))
        (sort (keys m))))

(defn with-sorted-keys [coll]
  (cond
    (record? coll)
    coll

    (map? coll)
    (if (sorted? coll)
      coll
      (try
        (with-meta (into (sorted-map) coll)
                   (meta coll))
        (catch ClassCastException _
          coll)))

    (set? coll)
    (if (sorted? coll)
      coll
      (try
        (with-meta (into (sorted-set) coll)
                   (meta coll))
        (catch ClassCastException _
          coll)))

    :else
    coll))

(declare select-keys-deep)

(defn- select-keys-deep-value-helper [kept-keys value]
  (cond
    (map? value)
    (select-keys-deep value kept-keys)

    (coll? value)
    (coll/transform value
      (map (partial select-keys-deep-value-helper kept-keys)))

    :else
    value))

(defn select-keys-deep
  "Like select-keys, but applies the filter recursively to nested data structures."
  [m kept-keys]
  (assert (or (nil? m) (map? m)))
  (with-meta (into (if (or (nil? m) (record? m))
                     {}
                     (empty m))
                   (keep (fn [key]
                           (when-some [[_ value] (find m key)]
                             [key (select-keys-deep-value-helper kept-keys value)])))
                   kept-keys)
             (meta m)))

(defn deep-map
  "Performs the map operation on a nested collection. The value-fn will be
  called with the value for each entry in nested maps and sequences and is
  expected to return a replacement value for the entry. Optionally, a
  finalize-coll-value-fn function can be supplied to be called with the
  resulting collection values."
  ([value-fn value]
   (deep-map identity value-fn value))
  ([finalize-coll-value-fn value-fn value]
   (cond
     (record? value)
     (value-fn value)

     (map? value)
     (finalize-coll-value-fn
       (coll/transform value
         (map (fn [[k v]]
                (let [v' (deep-map finalize-coll-value-fn value-fn v)]
                  (pair k v'))))))

     (coll? value)
     (finalize-coll-value-fn
       (coll/transform value
         (map #(deep-map finalize-coll-value-fn value-fn %))))

     :else
     (value-fn value))))

(defn deep-map-kv-helper [finalize-coll-value-fn value-fn key value]
  (cond
    (record? value)
    (value-fn key value)

    (map? value)
    (finalize-coll-value-fn
      (coll/transform value
        (map (fn [[k v]]
               (let [v' (deep-map-kv-helper finalize-coll-value-fn value-fn k v)]
                 (pair k v'))))))

    (coll? value)
    (finalize-coll-value-fn
      (coll/transform value
        (map-indexed #(deep-map-kv-helper finalize-coll-value-fn value-fn %1 %2))))

    :else
    (value-fn key value)))

(defn deep-map-kv
  "Performs the map operation on a nested collection. The value-fn will be
  called with the key and value for each entry in nested maps and sequences.
  The key will be nil for the root entry and indices for entries in sequential
  collections. The value-fn is expected to return a value to associate with the
  key. Optionally, a finalize-coll-value-fn function can be supplied to be
  called with the resulting collection values."
  ([value-fn value]
   (deep-map-kv-helper identity value-fn nil value))
  ([finalize-coll-value-fn value-fn value]
   (deep-map-kv-helper finalize-coll-value-fn value-fn nil value)))

(defn deep-keep
  "Performs the keep operation on a nested collection. The value-fn will be
  called with the value for each entry in nested maps and sequences and is
  expected to return a replacement value for that entry, or nil to exclude the
  entry. Optionally, a finalize-coll-value-fn function can be supplied to be
  called with the resulting collection values. Returning nil from it will
  exclude the entry from the result."
  ([value-fn value]
   (deep-keep identity value-fn value))
  ([finalize-coll-value-fn value-fn value]
   (cond
     (record? value)
     (value-fn value)

     (map? value)
     (finalize-coll-value-fn
       (coll/transform value
         (keep (fn [entry]
                 (when-some [v' (deep-keep finalize-coll-value-fn value-fn (val entry))]
                   (pair (key entry) v'))))))

     (coll? value)
     (finalize-coll-value-fn
       (coll/transform value
         (keep #(deep-keep finalize-coll-value-fn value-fn %))))

     :else
     (value-fn value))))

(defn deep-keep-kv-helper [finalize-coll-value-fn value-fn key value]
  (cond
    (record? value)
    (value-fn key value)

    (map? value)
    (finalize-coll-value-fn
      (coll/transform value
        (keep (fn [[k v]]
                (when-some [v' (deep-keep-kv-helper finalize-coll-value-fn value-fn k v)]
                  (pair k v'))))))

    (coll? value)
    (finalize-coll-value-fn
      (coll/transform value
        (keep-indexed #(deep-keep-kv-helper finalize-coll-value-fn value-fn %1 %2))))

    :else
    (value-fn key value)))

(defn deep-keep-kv
  "Performs the keep operation on a nested collection. The value-fn will be
  called with the key and value for each entry in nested maps and sequences.
  The key will be nil for the root entry and indices for entries in sequential
  collections. The value-fn is expected to return a value to associate with the
  key, or nil to exclude the entry. Optionally, a finalize-coll-value-fn
  function can be supplied to be called with the resulting collection values.
  Returning nil from it will exclude the entry from the result."
  ([value-fn value]
   (deep-keep-kv-helper identity value-fn nil value))
  ([finalize-coll-value-fn value-fn value]
   (deep-keep-kv-helper finalize-coll-value-fn value-fn nil value)))

(defn into-multiple
  "Like core.into, but transfers the input into multiple collections in a single
  pass. Instead of a single destination and optional transducer, takes a seq of
  destinations and a seq of transducers. The number of destinations must match
  the number of transducers. Returns a seq of the populated destinations."
  ([] [])
  ([tos] tos)
  ([tos from]
   (if (empty? from)
     tos
     (case (coll/bounded-count 3 tos)
       ;; No destinations - return unaltered.
       0 tos

       ;; Single destination - use regular into.
       1 [(into (first tos) from)]

       ;; Optimized for two destinations.
       2 (let [transient-tos
               (reduce (fn [transient-tos item]
                         (pair (conj! (transient-tos 0) item)
                               (conj! (transient-tos 1) item)))
                       (pair (transient (nth tos 0))
                             (transient (nth tos 1)))
                       from)]
           (pair (with-meta (persistent! (transient-tos 0)) (meta (nth tos 0)))
                 (with-meta (persistent! (transient-tos 1)) (meta (nth tos 1)))))

       ;; General case.
       (mapv #(with-meta (persistent! %2) (meta %1))
             tos
             (reduce (fn [transient-tos item]
                       (mapv conj! transient-tos (repeat item)))
                     (mapv transient tos)
                     from)))))
  ([tos xforms from]
   (if (empty? from)
     tos
     (case (coll/bounded-count 3 tos)
       ;; No destinations - return unaltered.
       0 (do
           (assert (= 0 (coll/bounded-count 1 xforms)))
           tos)

       ;; Single destination - use regular into.
       1 (do
           (assert (= 1 (coll/bounded-count 2 xforms)))
           [(into (first tos) (first xforms) from)])

       ;; Optimized for two destinations.
       2 (do
           (assert (= 2 (coll/bounded-count 3 xforms)))
           (let [f0 ((nth xforms 0) conj!)
                 f1 ((nth xforms 1) conj!)

                 transient-tos
                 (reduce (fn [transient-tos item]
                           (let [transient-to0 (transient-tos 0)
                                 transient-to1 (transient-tos 1)]
                             (let [transient-to0' (cond-> transient-to0 (not (reduced? transient-to0)) (f0 item))
                                   transient-to1' (cond-> transient-to1 (not (reduced? transient-to1)) (f1 item))]
                               (cond-> (pair transient-to0'
                                             transient-to1')

                                       (and (reduced? transient-to0')
                                            (reduced? transient-to1'))
                                       reduced))))
                         (pair (transient (nth tos 0))
                               (transient (nth tos 1)))
                         from)]
             (pair (with-meta (persistent! (f0 (unreduced (transient-tos 0))))
                              (meta (nth tos 0)))
                   (with-meta (persistent! (f1 (unreduced (transient-tos 1))))
                              (meta (nth tos 1))))))

       ;; General case.
       (let [fs (mapv (fn [xform]
                        (xform conj!))
                      xforms)]
         (assert (let [xform-count (count fs)]
                   (= xform-count (coll/bounded-count (inc xform-count) tos))))
         (mapv (fn [orig-to f transient-to']
                 (with-meta (persistent! (f (unreduced transient-to')))
                            (meta orig-to)))
               tos
               fs
               (reduce (fn [transient-tos item]
                         (let [[transient-tos' all-reduced]
                               (reduce-kv (fn [[transient-tos' all-reduced] index transient-to]
                                            (let [f (fs index)
                                                  transient-to' (cond-> transient-to (not (reduced? transient-to)) (f item))]
                                              (pair (conj transient-tos' transient-to')
                                                    (and all-reduced
                                                         (reduced? transient-to')))))
                                          (pair []
                                                true)
                                          transient-tos)]
                           (cond-> transient-tos'
                                   all-reduced reduced)))
                       (mapv transient tos)
                       from)))))))

(defn make-dedupe-fn
  "Given a sequence of non-unique items, returns a function that will return the
  same instance of each item if it is determined to be equal to an item already
  in the ever-growing set of unique items."
  [items]
  (let [unique-volatile (volatile! (set items))]
    (fn dedupe-fn [item]
      (or (get @unique-volatile item)
          (do
            (vswap! unique-volatile conj item)
            item)))))

(defn name-index
  "Create an index for items' names that, while identify the items, are not
  guaranteed to be unique within the coll

  Returns a map of [name name-order] => item-index

  Args:
    coll           nil or vector of items
    get-name-fn    fn that extracts the item's name"
  [coll get-name-fn]
  {:pre [(or (nil? coll) (vector? coll))]}
  (let [n (count coll)]
    (loop [i 0
           name->order (transient {})
           name+order->index (transient {})]
      (if (= i n)
        (persistent! name+order->index)
        (let [item (coll i)
              name (get-name-fn item)
              order (long (name->order name 0))]
          (recur (inc i)
                 (assoc! name->order name (inc order))
                 (assoc! name+order->index (pair name order) i)))))))

(defn detect-renames
  "Given 2 name indices, detect renames

  Renames are left-over names that don't match between 2 name indices

  Returns a map from old name+order to new name+order

  Args:
    old-name-index    map of name+order->item-index produced by [[name-index]]
    new-name-index    map of name+order->item-index produced by [[name-index]]"
  [old-name-index new-name-index]
  (into {}
        (mapv (fn [removed-entry added-entry]
                (pair (key removed-entry) (key added-entry)))
              (filterv (comp not new-name-index key) old-name-index)
              (filterv (comp not old-name-index key) new-name-index))))

(defn detect-deletions
  "Given 2 name indices, detect deletions from the old name index

  Returns a set of deleted old name+orders

  Args:
    old-name-index    map of name+order->item-index produced by [[name-index]]
    new-name-index    map of name+order->item-index produced by [[name-index]]"
  [old-name-index new-name-index]
  (let [renames (detect-renames old-name-index new-name-index)]
    (into #{}
          (comp
            (map key)
            (remove #(or (contains? renames %)
                         (contains? new-name-index %))))
          old-name-index)))

(defn detect-and-apply-renames
  "Given 2 collections of maps, detect and apply the renames to first one

  Returns updated first coll

  Args:
    old-coll        nil or vector of maps
    old-name-key    key used to extract names from maps in the old-coll
    new-coll        nil or vector of maps
    new-name-key    key used to extract names from maps in the new-coll"
  [old-coll old-name-key new-coll new-name-key]
  (let [old-index (name-index old-coll old-name-key)
        new-index (name-index new-coll new-name-key)
        renames (detect-renames old-index new-index)]
    (reduce-kv
      (fn [acc old-name+order new-name+order]
        (update acc (old-index old-name+order) assoc old-name-key (key new-name+order)))
      old-coll
      renames)))

(defn detect-renames-and-update-all
  "Given 2 collections, update every item in 1st with a matching item form 2nd

  Returns updated first coll

  Args:
    old-coll        nil or vector of maps
    old-name-key    key used to extract names from maps in the old-coll
    new-coll        nil or vector of maps
    new-name-key    key used to extract names from maps in the new-coll
    update-fn       fn of 2 args - an item from the old coll and an item from
                    the new-coll (or nil if there is no match for the old item)

  Optional kv-args:
    :not-found    fallback value to pass to update-fn when there is no matching
                  item in the new coll, defaults to nil"
  [old-coll old-name-key new-coll new-name-key update-fn & {:keys [not-found]}]
  (let [old-index (name-index old-coll old-name-key)
        new-index (name-index new-coll new-name-key)
        renames (detect-renames old-index new-index)]
    (persistent!
      (reduce-kv
        (fn [acc old-name+order old-item-index]
          (let [old-item (acc old-item-index)
                new-name+order (renames old-name+order)
                new-item-index (new-index (or new-name+order old-name+order))
                new-item (if new-item-index (new-coll new-item-index) not-found)]
            (assoc! acc old-item-index (update-fn old-item new-item))))
        (transient old-coll)
        old-index))))

(defn first-rf
  "first as a reducing function"
  ([] nil)
  ([acc] acc)
  ([_ v] (ensure-reduced v)))
;; Copyright 2020-2023 The Defold Foundation
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
  (:refer-clojure :exclude [bounded-count empty?])
  (:import [clojure.lang IEditableCollection MapEntry]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def empty-sorted-map (sorted-map))

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

(defn empty?
  "Like core.empty?, but avoids generating garbage for counted collections."
  [coll]
  (cond
    (counted? coll)
    (zero? (count coll))

    (nil? coll)
    true

    :else
    (not (seq coll))))

(defn pair-map-by
  "Returns a hash-map where the keys are the result of applying the supplied
  key-fn to each item in the input sequence and the values are the items
  themselves. Returns a stateless transducer if no input sequence is
  provided. Optionally, a value-fn can be supplied to transform the values."
  ([key-fn]
   {:pre [(ifn? key-fn)]}
   (map (pair-fn key-fn)))
  ([key-fn coll]
   (into {}
         (map (pair-fn key-fn))
         coll))
  ([key-fn value-fn coll]
   (into {}
         (map (pair-fn key-fn value-fn))
         coll)))

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

(defn sorted-assoc-in
  "Like core.assoc-in, but sorted maps will be created for any levels that do not exist."
  [coll [key & remaining-keys] value]
  (if remaining-keys
    (assoc coll key (sorted-assoc-in (get coll key empty-sorted-map) remaining-keys value))
    (assoc coll key value)))

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
  (reduce (if (sorted? path-map)
            (fn [nested-map [path value]]
              (sorted-assoc-in nested-map path value))
            (fn [nested-map [path value]]
              (assoc-in nested-map path value)))
          (empty path-map)
          path-map))

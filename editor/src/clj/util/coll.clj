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

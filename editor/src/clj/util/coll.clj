;; Copyright 2020-2022 The Defold Foundation
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
  (:refer-clojure :exclude [bounded-count])
  (:import [clojure.lang MapEntry]
           [java.util List]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(definline index-of
  "Return the index of the first occurrence of the specified item in the
  supplied vector, or -1 if not found."
  [^List coll item]
  `(.indexOf ~coll ~item))

(defn remove-at
  "Return a vector without the item at the specified index."
  [coll ^long removed-index]
  {:pre [(vector? coll)]}
  (persistent!
    (reduce-kv (fn [result ^long index value]
                 (if (= removed-index index)
                   result
                   (conj! result value)))
               (transient [])
               coll)))

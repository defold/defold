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

(ns util.bit-set
  (:refer-clojure :exclude [bit-set empty? into not-empty reduce seq transduce])
  (:require [util.coll :as coll])
  (:import [clojure.lang LazilyPersistentVector]
           [java.util BitSet]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmacro bit-set? [value]
  `(instance? BitSet ~value))

(defmacro hinted [bit-set]
  (some-> bit-set (with-meta {:tag `BitSet})))

(defmacro empty? [bit-set]
  `(let [^BitSet bit-set# ~bit-set]
     (or (nil? bit-set#)
         (.isEmpty bit-set#))))

(defmacro not-empty [bit-set]
  `(let [^BitSet bit-set# ~bit-set]
     (when (and bit-set#
                (not (.isEmpty bit-set#)))
       bit-set#)))

(defmacro ^:private ensure-counted [items]
  `(let [items# ~items]
     (if (counted? items#)
       items#
       (LazilyPersistentVector/create items#))))

(defn- empty-seq-or-bit-set? [value]
  (if (bit-set? value)
    (empty? value)
    (coll/empty? value)))

(defn- with-set-indices!
  ^BitSet [^BitSet bit-set indices]
  (clojure.core/reduce
    (fn [_ ^long index]
      (.set bit-set index))
    nil
    indices)
  bit-set)

(defn of
  ([]
   (BitSet. 0))
  ([^long a]
   (doto (BitSet.)
     (.set a)))
  ([^long a ^long b]
   (doto (BitSet.)
     (.set a)
     (.set b)))
  ([^long a ^long b ^long c]
   (doto (BitSet.)
     (.set a)
     (.set b)
     (.set c)))
  ([^long a ^long b ^long c ^long d]
   (doto (BitSet.)
     (.set a)
     (.set b)
     (.set c)
     (.set d)))
  ([a b c d & more]
   (with-set-indices! (of a b c d) more)))

(defn of-capacity
  {:inline-arities #{1}
   :inline (fn [capacity] `(BitSet. (int ~capacity)))}
  ([^long capacity]
   (BitSet. capacity))
  ([^long capacity ^long a]
   (doto (BitSet. capacity)
     (.set a)))
  ([^long capacity ^long a ^long b]
   (doto (BitSet. capacity)
     (.set a)
     (.set b)))
  ([^long capacity ^long a ^long b ^long c]
   (doto (BitSet. capacity)
     (.set a)
     (.set b)
     (.set c)))
  ([capacity a b c & more]
   (with-set-indices! (of-capacity capacity a b c) more)))

(defn from
  ([indices]
   (let [indices (ensure-counted indices)
         length (count indices)]
     (from length indices)))
  ([^long length indices]
   (with-set-indices! (of-capacity length) indices)))

(defmacro assign! [target-bit-set source-bit-set]
  `(doto (hinted ~target-bit-set)
     (.clear)
     (.or (hinted ~source-bit-set))))

(defmacro cardinality [bit-set]
  `(.cardinality (hinted ~bit-set)))

(defmacro bit [bit-set index]
  `(.get (hinted ~bit-set) (int ~index)))

(defmacro set-bit [bit-set index]
  `(let [^BitSet bit-set# ~bit-set
         index# (int ~index)]
     (if (.get bit-set# index#)
       bit-set#
       (doto ^BitSet (.clone bit-set#)
         (.set index#)))))

(defmacro set-bit! [bit-set index]
  `(doto (hinted ~bit-set)
     (.set (int ~index))))

(defmacro clear-bit [bit-set index]
  `(let [^BitSet bit-set# ~bit-set
         index# (int ~index)]
     (if (.get bit-set# index#)
       (doto ^BitSet (.clone bit-set#)
         (.clear index#))
       bit-set#)))

(defmacro clear-bit! [bit-set index]
  `(doto (hinted ~bit-set)
     (.clear (int ~index))))

(defmacro and-bits [bit-set-a bit-set-b]
  `(let [^BitSet bit-set-a# ~bit-set-a
         ^BitSet result# (.clone bit-set-a#)]
     (.and result# (hinted ~bit-set-b))
     (if (= (.cardinality bit-set-a#) (.cardinality result#))
       bit-set-a#
       result#)))

(defmacro and-bits! [bit-set-a bit-set-b]
  `(doto (hinted ~bit-set-a)
     (.and (hinted ~bit-set-b))))

(defmacro or-bits [bit-set-a bit-set-b]
  `(let [^BitSet bit-set-a# ~bit-set-a
         ^BitSet result# (.clone bit-set-a#)]
     (.or result# (hinted ~bit-set-b))
     (if (= (.cardinality bit-set-a#) (.cardinality result#))
       bit-set-a#
       result#)))

(defmacro or-bits! [bit-set-a bit-set-b]
  `(doto (hinted ~bit-set-a)
     (.or (hinted ~bit-set-b))))

(defmacro seq [bit-set]
  `(let [^BitSet bit-set# ~bit-set]
     (when-not (empty? bit-set#)
       (stream-seq! (.stream bit-set#)))))

(defmacro reduce
  ([f bit-set]
   `(stream-reduce! ~f (.stream (hinted ~bit-set))))
  ([f init bit-set]
   `(stream-reduce! ~f ~init (.stream (hinted ~bit-set)))))

(defmacro transduce
  ([xform f bit-set]
   `(stream-transduce! ~xform ~f (.stream (hinted ~bit-set))))
  ([xform f init bit-set]
   `(stream-transduce! ~xform ~f ~init (.stream (hinted ~bit-set)))))

(defn into
  ([to] to)
  ([to from]
   (if (empty-seq-or-bit-set? from)
     to
     (let [to-is-bit-set (bit-set? to)
           from-is-bit-set (bit-set? from)]
       (cond
         (and to-is-bit-set from-is-bit-set)
         (doto ^BitSet (.clone ^BitSet to)
           (.or ^BitSet from))

         to-is-bit-set
         (with-set-indices! (.clone ^BitSet to) from)

         from-is-bit-set
         (stream-into! to (.stream ^BitSet from))

         :else
         (clojure.core/into to from)))))
  ([to xform from]
   (cond
     (empty-seq-or-bit-set? from)
     to

     (bit-set? to)
     (let [^BitSet result (.clone ^BitSet to)
           reduce-fn (fn reduce-fn
                       ([_] _)
                       ([_ index]
                        (.set result (int index))))]
       (if (bit-set? from)
         (transduce xform reduce-fn nil from)
         (clojure.core/transduce xform reduce-fn nil from))
       result)

     (bit-set? from)
     (stream-into! to xform (.stream ^BitSet from))

     :else
     (clojure.core/into to xform from))))

(defmacro transfer
  ([bit-set to]
   `(into ~to ~bit-set))
  ([bit-set to xform]
   `(into ~to ~xform ~bit-set))
  ([bit-set to xform & xforms]
   `(into ~to (comp ~xform ~@xforms) ~bit-set)))

(defmacro indices [bit-set]
  `(into (vector-of :int) ~bit-set))

(defmacro previous-set-bit [bit-set from-index]
  `(.previousSetBit (hinted ~bit-set) (int ~from-index)))

(defmacro next-set-bit [bit-set from-index]
  `(.nextSetBit (hinted ~bit-set) (int ~from-index)))

(defmacro first-set-bit [bit-set]
  `(.nextSetBit (hinted ~bit-set) 0))

(defmacro last-set-bit [bit-set]
  `(let [^BitSet bit-set# ~bit-set]
     (.previousSetBit bit-set# (dec (.length bit-set#)))))

;; Copyright 2020-2025 The Defold Foundation
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

(ns util.array
  (:require [util.fn :as fn])
  (:import [clojure.lang LazilyPersistentVector]
           [java.lang.reflect Array]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- empty-of-type-raw [^Class item-type]
  (Array/newInstance item-type 0))

(defonce empty-of-type (fn/memoize empty-of-type-raw))

(defonce ^"[Ljava.lang.Object;" empty-object-array (empty-of-type Object))
(defonce ^"[Z" empty-boolean-array (empty-of-type Boolean/TYPE))
(defonce ^"[C" empty-char-array (empty-of-type Character/TYPE))
(defonce ^"[B" empty-byte-array (empty-of-type Byte/TYPE))
(defonce ^"[S" empty-short-array (empty-of-type Short/TYPE))
(defonce ^"[I" empty-int-array (empty-of-type Integer/TYPE))
(defonce ^"[J" empty-long-array (empty-of-type Long/TYPE))
(defonce ^"[F" empty-float-array (empty-of-type Float/TYPE))
(defonce ^"[D" empty-double-array (empty-of-type Double/TYPE))

(defmacro ^:private set-items-impl! [array items start-index]
  {:pre [(symbol? array)]}
  `(reduce
     (fn [^long index# item#]
       (Array/set ~array index# item#)
       (inc index#))
     (long ~start-index)
     ~items))

(defmacro ^:private of-type-impl
  [item-type length & items]
  (when (integer? length)
    (assert (<= (count items) (int length))))
  `(doto (Array/newInstance ~item-type (int ~length))
     ~@(map-indexed #(list `Array/set %1 %2) items)))

(defmacro ensure-counted [items]
  `(let [items# ~items]
     (if (counted? items#)
       items#
       (LazilyPersistentVector/create items#))))

(defn of
  (^"[Ljava.lang.Object;" []
   empty-object-array)
  (^"[Ljava.lang.Object;" [a]
   (of-type-impl Object 1 a))
  (^"[Ljava.lang.Object;" [a b]
   (of-type-impl Object 2 a b))
  (^"[Ljava.lang.Object;" [a b c]
   (of-type-impl Object 3 a b c))
  (^"[Ljava.lang.Object;" [a b c d]
   (of-type-impl Object 4 a b c d))
  (^"[Ljava.lang.Object;" [a b c d e]
   (of-type-impl Object 5 a b c d e))
  (^"[Ljava.lang.Object;" [a b c d e f]
   (of-type-impl Object 6 a b c d e f))
  (^"[Ljava.lang.Object;" [a b c d e f g]
   (of-type-impl Object 7 a b c d e f g))
  (^"[Ljava.lang.Object;" [a b c d e f g h]
   (of-type-impl Object 8 a b c d e f g h))
  (^"[Ljava.lang.Object;" [a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (of-type-impl Object length a b c d e f g h)]
     (set-items-impl! array more 8)
     array)))

(defn of-type
  (^"[Ljava.lang.Object;" [^Class item-type]
   (empty-of-type item-type))
  (^"[Ljava.lang.Object;" [^Class item-type a]
   (of-type-impl item-type 1 a))
  (^"[Ljava.lang.Object;" [^Class item-type a b]
   (of-type-impl item-type 2 a b))
  (^"[Ljava.lang.Object;" [^Class item-type a b c]
   (of-type-impl item-type 3 a b c))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d]
   (of-type-impl item-type 4 a b c d))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d e]
   (of-type-impl item-type 5 a b c d e))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d e f]
   (of-type-impl item-type 6 a b c d e f))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d e f g]
   (of-type-impl item-type 7 a b c d e f g))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d e f g h]
   (of-type-impl item-type 8 a b c d e f g h))
  (^"[Ljava.lang.Object;" [^Class item-type a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (of-type-impl item-type length a b c d e f g h)]
     (set-items-impl! array more 8)
     array)))

;; TODO: Add primitive equivalents:
;; of-booleans
;; of-chars
;; of-bytes
;; of-shorts
;; of-ints
;; of-longs
;; of-floats
;; of-doubles

(defn from
  (^"[Ljava.lang.Object;" [items]
   (let [items (ensure-counted items)
         length (count items)]
     (from length items)))
  (^"[Ljava.lang.Object;" [length items]
   (let [length (int length)]
     (case length
       0 empty-object-array
       (let [array (Array/newInstance Object length)]
         (set-items-impl! array items 0)
         array)))))

(defn from-type
  (^"[Ljava.lang.Object;" [^Class item-type items]
   (let [items (ensure-counted items)
         length (count items)]
     (from-type item-type length items)))
  (^"[Ljava.lang.Object;" [^Class item-type length items]
   (let [length (int length)]
     (case length
       0 (empty-of-type item-type)
       (let [array (Array/newInstance item-type length)]
         (set-items-impl! array items 0)
         array)))))

;; TODO: Add primitive equivalents:
;; from-booleans
;; from-chars
;; from-bytes
;; from-shorts
;; from-ints
;; from-longs
;; from-floats
;; from-doubles

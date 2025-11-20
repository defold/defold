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
  (:import [clojure.lang LazilyPersistentVector]
           [java.lang.reflect Array]
           [java.util Arrays]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- empty-of-type-raw [^Class item-type]
  (Array/newInstance item-type 0))

(defonce empty-of-type (memoize empty-of-type-raw))

(defonce ^boolean/1 empty-boolean-array (empty-of-type Boolean/TYPE))
(defonce ^char/1 empty-char-array (empty-of-type Character/TYPE))
(defonce ^byte/1 empty-byte-array (empty-of-type Byte/TYPE))
(defonce ^short/1 empty-short-array (empty-of-type Short/TYPE))
(defonce ^int/1 empty-int-array (empty-of-type Integer/TYPE))
(defonce ^long/1 empty-long-array (empty-of-type Long/TYPE))
(defonce ^float/1 empty-float-array (empty-of-type Float/TYPE))
(defonce ^double/1 empty-double-array (empty-of-type Double/TYPE))
(defonce ^Object/1 empty-object-array (empty-of-type Object))

(defn primitive-type [array]
  (condp = (class array)
    boolean/1 :boolean
    char/1 :char
    byte/1 :byte
    short/1 :short
    int/1 :int
    long/1 :long
    float/1 :float
    double/1 :double
    nil))

(defn array? [value]
  (and (some? value)
       (.isArray (class value))))

(defn primitive-array? [value]
  (some? (primitive-type value)))

(defn length
  ^long [array]
  (condp = (class array)
    boolean/1 (alength ^boolean/1 array)
    char/1 (alength ^char/1 array)
    byte/1 (alength ^byte/1 array)
    short/1 (alength ^short/1 array)
    int/1 (alength ^int/1 array)
    long/1 (alength ^long/1 array)
    float/1 (alength ^float/1 array)
    double/1 (alength ^double/1 array)
    (alength ^Object/1 array)))

(defn- nth-boolean [^boolean/1 array ^long index] (aget array index))
(defn- nth-char [^char/1 array ^long index] (aget array index))
(defn- nth-byte [^byte/1 array ^long index] (aget array index))
(defn- nth-short [^short/1 array ^long index] (aget array index))
(defn- nth-int [^int/1 array ^long index] (aget array index))
(defn- nth-long [^long/1 array ^long index] (aget array index))
(defn- nth-float [^float/1 array ^long index] (aget array index))
(defn- nth-double [^double/1 array ^long index] (aget array index))
(defn- nth-object [^Object/1 array ^long index] (aget array index))

(defn nth-fn [array]
  (condp = (class array)
    boolean/1 nth-boolean
    char/1 nth-char
    byte/1 nth-byte
    short/1 nth-short
    int/1 nth-int
    long/1 nth-long
    float/1 nth-float
    double/1 nth-double
    (if (array? array)
      nth-object
      (throw
        (IllegalArgumentException. "array must be an Array")))))

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
  (^Object/1 []
   empty-object-array)
  (^Object/1 [a]
   (of-type-impl Object 1 a))
  (^Object/1 [a b]
   (of-type-impl Object 2 a b))
  (^Object/1 [a b c]
   (of-type-impl Object 3 a b c))
  (^Object/1 [a b c d]
   (of-type-impl Object 4 a b c d))
  (^Object/1 [a b c d e]
   (of-type-impl Object 5 a b c d e))
  (^Object/1 [a b c d e f]
   (of-type-impl Object 6 a b c d e f))
  (^Object/1 [a b c d e f g]
   (of-type-impl Object 7 a b c d e f g))
  (^Object/1 [a b c d e f g h]
   (of-type-impl Object 8 a b c d e f g h))
  (^Object/1 [a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (of-type-impl Object length a b c d e f g h)]
     (set-items-impl! array more 8)
     array)))

(defn of-type
  (^Object/1 [^Class item-type]
   (empty-of-type item-type))
  (^Object/1 [^Class item-type a]
   (of-type-impl item-type 1 a))
  (^Object/1 [^Class item-type a b]
   (of-type-impl item-type 2 a b))
  (^Object/1 [^Class item-type a b c]
   (of-type-impl item-type 3 a b c))
  (^Object/1 [^Class item-type a b c d]
   (of-type-impl item-type 4 a b c d))
  (^Object/1 [^Class item-type a b c d e]
   (of-type-impl item-type 5 a b c d e))
  (^Object/1 [^Class item-type a b c d e f]
   (of-type-impl item-type 6 a b c d e f))
  (^Object/1 [^Class item-type a b c d e f g]
   (of-type-impl item-type 7 a b c d e f g))
  (^Object/1 [^Class item-type a b c d e f g h]
   (of-type-impl item-type 8 a b c d e f g h))
  (^Object/1 [^Class item-type a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (of-type-impl item-type length a b c d e f g h)]
     (set-items-impl! array more 8)
     array)))

(defmacro of-floats [& numbers]
  (let [arg-count (count numbers)]
    (case arg-count
      0 `empty-float-array
      `(doto (float-array ~arg-count)
         ~@(map-indexed #(list `Array/setFloat %1 %2) numbers)))))

;; TODO: Add all primitive equivalents:
;; of-booleans
;; of-chars
;; of-bytes
;; of-shorts
;; of-ints
;; of-longs
;; of-floats
;; of-doubles

(defn from
  (^Object/1 [items]
   (let [items (ensure-counted items)
         length (count items)]
     (from length items)))
  (^Object/1 [length items]
   (let [length (int length)]
     (case length
       0 empty-object-array
       (let [array (Array/newInstance Object length)]
         (set-items-impl! array items 0)
         array)))))

(defn from-type
  (^Object/1 [^Class item-type items]
   (let [items (ensure-counted items)
         length (count items)]
     (from-type item-type length items)))
  (^Object/1 [^Class item-type length items]
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

(defn hash-code
  (^long [array]
   (condp = (class array)
     boolean/1 (Arrays/hashCode ^booleans array)
     char/1 (Arrays/hashCode ^chars array)
     byte/1 (Arrays/hashCode ^bytes array)
     short/1 (Arrays/hashCode ^shorts array)
     int/1 (Arrays/hashCode ^ints array)
     long/1 (Arrays/hashCode ^longs array)
     float/1 (Arrays/hashCode ^floats array)
     double/1 (Arrays/hashCode ^doubles array)
     (Arrays/hashCode ^Object/1 array)))
  (^long [array ^long start ^long end]
   (if (and (zero? start)
            (= (Array/getLength array) end))
     (hash-code array)
     ;; TODO: Write non-allocating implementations for all these.
     (condp = (class array)
       boolean/1 (Arrays/hashCode (Arrays/copyOfRange ^booleans array start end))
       char/1 (Arrays/hashCode (Arrays/copyOfRange ^chars array start end))
       byte/1 (Arrays/hashCode (Arrays/copyOfRange ^bytes array start end))
       short/1 (Arrays/hashCode (Arrays/copyOfRange ^shorts array start end))
       int/1 (Arrays/hashCode (Arrays/copyOfRange ^ints array start end))
       long/1 (Arrays/hashCode (Arrays/copyOfRange ^longs array start end))
       float/1 (Arrays/hashCode (Arrays/copyOfRange ^floats array start end))
       double/1 (Arrays/hashCode (Arrays/copyOfRange ^doubles array start end))
       (Arrays/hashCode (Arrays/copyOfRange ^Object/1 array start end))))))

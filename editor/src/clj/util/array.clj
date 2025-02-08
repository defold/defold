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
  (:import [java.lang.reflect Array]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^"[J" empty-long-array (long-array 0))
(def ^"[Ljava.lang.Object;" empty-object-array (object-array 0))

(defn of
  (^"[Ljava.lang.Object;" [] empty-object-array)
  (^"[Ljava.lang.Object;" [a]
   (doto (Array/newInstance Object 1)
     (Array/set 0 a)))
  (^"[Ljava.lang.Object;" [a b]
   (doto (Array/newInstance Object 2)
     (Array/set 0 a)
     (Array/set 1 b)))
  (^"[Ljava.lang.Object;" [a b c]
   (doto (Array/newInstance Object 3)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)))
  (^"[Ljava.lang.Object;" [a b c d]
   (doto (Array/newInstance Object 4)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)
     (Array/set 3 d)))
  (^"[Ljava.lang.Object;" [a b c d e]
   (doto (Array/newInstance Object 5)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)
     (Array/set 3 d)
     (Array/set 4 e)))
  (^"[Ljava.lang.Object;" [a b c d e f]
   (doto (Array/newInstance Object 6)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)
     (Array/set 3 d)
     (Array/set 4 e)
     (Array/set 5 f)))
  (^"[Ljava.lang.Object;" [a b c d e f g]
   (doto (Array/newInstance Object 7)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)
     (Array/set 3 d)
     (Array/set 4 e)
     (Array/set 5 f)
     (Array/set 6 g)))
  (^"[Ljava.lang.Object;" [a b c d e f g h]
   (doto (Array/newInstance Object 8)
     (Array/set 0 a)
     (Array/set 1 b)
     (Array/set 2 c)
     (Array/set 3 d)
     (Array/set 4 e)
     (Array/set 5 f)
     (Array/set 6 g)
     (Array/set 7 h)))
  (^"[Ljava.lang.Object;" [a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (doto (Array/newInstance Object (int length))
                 (Array/set 0 a)
                 (Array/set 1 b)
                 (Array/set 2 c)
                 (Array/set 3 d)
                 (Array/set 4 e)
                 (Array/set 5 f)
                 (Array/set 6 g)
                 (Array/set 7 h))]
     (reduce (fn [^long index item]
               (Array/set array index item)
               (inc index))
             (- length more-count)
             more)
     array)))

(defn of-longs
  (^longs [] empty-long-array)
  (^longs [^long a]
   (doto (long-array 1)
     (Array/setLong 0 a)))
  (^longs [^long a ^long b]
   (doto (long-array 2)
     (Array/setLong 0 a)
     (Array/setLong 1 b)))
  (^longs [^long a ^long b ^long c]
   (doto (long-array 3)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)))
  (^longs [^long a ^long b ^long c ^long d]
   (doto (long-array 4)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)
     (Array/setLong 3 d)))
  (^longs [a b c d e]
   (doto (long-array 5)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)
     (Array/setLong 3 d)
     (Array/setLong 4 e)))
  (^longs [a b c d e f]
   (doto (long-array 6)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)
     (Array/setLong 3 d)
     (Array/setLong 4 e)
     (Array/setLong 5 f)))
  (^longs [a b c d e f g]
   (doto (long-array 7)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)
     (Array/setLong 3 d)
     (Array/setLong 4 e)
     (Array/setLong 5 f)
     (Array/setLong 6 g)))
  (^longs [a b c d e f g h]
   (doto (long-array 8)
     (Array/setLong 0 a)
     (Array/setLong 1 b)
     (Array/setLong 2 c)
     (Array/setLong 3 d)
     (Array/setLong 4 e)
     (Array/setLong 5 f)
     (Array/setLong 6 g)
     (Array/setLong 7 h)))
  (^longs [a b c d e f g h & more]
   (let [more-count (count more)
         length (+ 8 more-count)
         array (doto (long-array (int length))
                 (Array/setLong 0 a)
                 (Array/setLong 1 b)
                 (Array/setLong 2 c)
                 (Array/setLong 3 d)
                 (Array/setLong 4 e)
                 (Array/setLong 5 f)
                 (Array/setLong 6 g)
                 (Array/setLong 7 h))]
     (reduce (fn [^long index ^long item]
               (Array/setLong array index item)
               (inc index))
             (- length more-count)
             more)
     array)))

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

(ns editor.texture.math
  (:require [dynamo.graph :as g]
            [schema.core :as s]))

(set! *warn-on-reflection* true)

(s/defn doubling :- g/Num
  "Return a lazy infinite sequence of doublings of i"
  ([i :- g/Num]
    (iterate #(bit-shift-left % 1) i)))

(s/defn closest-power-of-two :- g/Num
  "Return the next higher or preceeding lower power-of-two,
   whichever is nearer the input"
  [x :- g/Num]
  (assert (> x 0) "Does not work for negative numbers")
  (let [[lesser greater] (split-with #(> x %) (doubling 1))
        prev             (last lesser)
        next             (first greater)]
    (if (<= (- next x) (- x prev))
      next
      prev)))

(defn next-power-of-two
  [x]
  (if (= 0 x)
    1
    (let [x (dec x)
          x (bit-or x (bit-shift-right x 1))
          x (bit-or x (bit-shift-right x 2))
          x (bit-or x (bit-shift-right x 4))
          x (bit-or x (bit-shift-right x 8))
          x (bit-or x (bit-shift-right x 16))]
      (int (inc x)))))

(def log-2 (Math/log 2))

(defn nbits [x] (int (/ (Math/log (next-power-of-two x)) log-2)))

(defn binary-search-current
  [[_ _ current]]
  (when current
    (int (Math/pow 2 current))))

(defn binary-search-next
  [[low high current :as bs] up?]
  (when (and bs (< low high))
    (let [low  (int (if     up? (inc  low)  low))
          high (int (if-not up? (dec high) high))]
      (when (> (Math/abs (- low high)) 0)
        [low high (bit-shift-right (+ low high) 1)]))))

(defn binary-search-start
  [min max]
  (let [min (nbits min)
        max (nbits max)]
    [min max (bit-shift-right (+ min max) 1)]))

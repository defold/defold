(ns internal.texture.math
  (:require [schema.core :as s]
            [schema.macros :as sm]))

(set! *warn-on-reflection* true)

(sm/defn doubling :- s/Num
  "Return a lazy infinite sequence of doublings of i"
  ([i :- s/Num]
    (iterate #(bit-shift-left % 1) i)))

(sm/defn closest-power-of-two :- s/Num
  "Return the next higher or preceeding lower power-of-two,
   whichever is nearer the input"
  [x :- s/Num]
  (assert (> x 0) "Does not work for negative numbers")
  (let [[lesser greater] (split-with #(> x %) (doubling 1))
        prev             (last lesser)
        next             (first greater)]
    (if (<= (- next x) (- x prev))
      next
      prev)))

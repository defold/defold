(ns dynamo.util-test
  (:require [clojure.test :refer :all]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [dynamo.util :refer :all]))

(deftest test-parse-number-parse-int
  (are [input expected-number expected-int]
    (and (= expected-number (parse-number input))
         (= expected-int (parse-int input)))
    "43"           43     43
    "43.0"         43.0   43
    "43."          43.0   43
    "72.02"        72.02  nil
    "009.0008"     9.0008 nil
    "009"          9      9
    "010"          10     10
    "-010"         -10    -10
    "0.025"        0.025  nil
    ".025"         nil    nil
    "."            nil    nil
    ".0"           nil    nil
    "0."           0.0    0
    "0"            0      0
    "000"          0      0
    "89blah"       nil    nil
    "z29"          nil    nil
    "(exploit-me)" nil    nil
    "-92837482734982347.00789" -9.2837482734982352E16 nil))

(def gen-bounds
  (gen/bind (gen/tuple gen/s-pos-int gen/nat)
    (fn [[min range]] (gen/tuple (gen/return min) (gen/return (+ min range))))))

(defspec push-with-size-limit-adds-value-to-top-of-stack
  100
  (prop/for-all [[min-size max-size] gen-bounds
                 stack (gen/vector gen/keyword)
                 v gen/keyword]
    (= v (peek (push-with-size-limit min-size max-size stack v)))))

(defspec push-with-size-limit-respects-max-size
  100
  (prop/for-all [[min-size max-size] gen-bounds
                 stack (gen/vector gen/keyword)
                 v gen/keyword]
    (>= max-size (count (push-with-size-limit min-size max-size stack v)))))

(defspec push-with-size-limit-shrinks-stack-to-min-size
  100
  (prop/for-all [[min-size max-size] gen-bounds
                 stack (gen/vector gen/keyword)
                 v gen/keyword]
    (let [old-size (count stack)
          new-size (count (push-with-size-limit min-size max-size stack v))]
      (or
        (= new-size (inc old-size))
        (= new-size min-size)))))

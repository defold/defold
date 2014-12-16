(ns dynamo.util-test
  (:require [clojure.test :refer :all]
            [dynamo.util :refer :all]))

(deftest test-parse-number
  (are [expected input] (= expected (parse-number input))
    43     "43"
    72.02  "72.02"
    9.0008 "009.0008"
    9      "009"
    10     "010"
    -10    "-010"
    0.025  "0.025"
    nil    ".025"
    0.0    "0."
    0      "0"
    0      "000"
    nil    "89blah"
    nil    "z29"
    nil    "(exploit-me)"
    -9.2837482734982352E16 "-92837482734982347.00789"))

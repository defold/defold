(ns internal.util-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [dynamo.graph :as g]
            [internal.util :refer :all]
            [schema.core :as s]))

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

(def gen-set (gen/fmap set (gen/vector gen/nat)))

(defspec apply-deltas-invariants
  100
  (prop/for-all [[oldset newset] (gen/tuple gen-set gen-set)]
    (let [[removal-ops addition-ops] (apply-deltas oldset newset (fn [out] (reduce conj [] out)) (fn [in] (reduce conj [] in)))
          old-minus-new              (set/difference oldset newset)
          new-minus-old              (set/difference newset oldset)]
      (and
        (= (count removal-ops) (count old-minus-new))
        (= (count addition-ops) (count new-minus-old))
        (= 1 (apply max 1 (vals (frequencies removal-ops))))
        (= 1 (apply max 1 (vals (frequencies addition-ops))))))))

(deftest stringifying-keywords
  (are [s k] (= s (keyword->label k))
    "Word"                :word
    "Two Words"           :two-words
    "2words"              :2words
    "2 Words"             :2-words
    "More Than Two Words" :more-than-two-words
    "More Than 2words"    :more-than2words))

(g/defnk external-fnk [a b c d])

(deftest determining-inputs-required
  (testing "fnks"
    (are [f i] (= i (inputs-needed f))
      external-fnk                                                            #{:a :b :c :d}
      'external-fnk                                                           #{:a :b :c :d}
      #'external-fnk                                                          #{:a :b :c :d}
      (g/fnk [one two three])                                                 #{:one :two :three}
      '(g/fnk [one two three])                                                #{:one :two :three}
      (g/fnk [commands :- [g/Str] roles :- g/Any blah :- {g/Keyword g/Num}])  #{:commands :roles :blah}
      '(g/fnk [commands :- [g/Str] roles :- g/Any blah :- {g/Keyword g/Num}]) #{:commands :roles :blah})))

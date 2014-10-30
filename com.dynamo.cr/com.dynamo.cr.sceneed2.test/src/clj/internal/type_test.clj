(ns internal.type-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.types :as t]))

(sm/defrecord T1
  [ident :- String])

(sm/defrecord T2
  [value :- Integer])

(deftest type-compatibility
  (testing "identity rules"
    (is (t/compatible? T1 T1))
    (is (not (t/compatible? T1 T2))))
  (testing "collection rule"
    (is (t/compatible? T1 [T1]))
    (is (not (t/compatible? T1 [T2]))))
  (testing "classes"
    (is (t/compatible? String String))
    (is (t/compatible? String [String])))
  (testing "subclasses"
    (is (t/compatible? Integer Number))
    (is (t/compatible? Integer s/Num)))
  (testing "wildcards"
    (is (t/compatible? T1 s/Any))
    (is (t/compatible? String s/Any))
    (is (t/compatible? [String] s/Any))))

(run-tests)
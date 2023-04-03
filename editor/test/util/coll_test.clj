(ns util.coll-test
  (:require [clojure.test :refer :all]
            [util.coll :as coll])
  (:import [clojure.lang IPersistentVector]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest pair-test
  (instance? IPersistentVector (coll/pair 1 2))
  (let [[a b] (coll/pair 1 2)]
    (is (= 1 a))
    (is (= 2 b))))

(deftest flipped-pair-test
  (instance? IPersistentVector (coll/flipped-pair 1 2))
  (let [[a b] (coll/flipped-pair 1 2)]
    (is (= 2 a))
    (is (= 1 b))))

(deftest bounded-count-test
  (testing "Counted sequence"
    (let [coll [1 2]]
      (is (counted? coll))
      (is (= 2 (count coll)))
      (is (= 2 (coll/bounded-count 0 coll)))
      (is (= 2 (coll/bounded-count 1 coll)))
      (is (= 2 (coll/bounded-count 2 coll)))
      (is (= 2 (coll/bounded-count 3 coll)))))
  (testing "Non-counted sequence"
    (let [coll (iterate inc 0)]
      (is (not (counted? coll)))
      (is (= 0 (coll/bounded-count 0 coll)))
      (is (= 1 (coll/bounded-count 1 coll)))
      (is (= 2 (coll/bounded-count 2 coll)))
      (is (= 3 (coll/bounded-count 3 coll))))))

(deftest empty?-test
  (is (true? (coll/empty? nil)))
  (is (true? (coll/empty? "")))
  (is (true? (coll/empty? [])))
  (is (true? (coll/empty? '())))
  (is (true? (coll/empty? {})))
  (is (true? (coll/empty? #{})))
  (is (true? (coll/empty? (sorted-map))))
  (is (true? (coll/empty? (sorted-set))))
  (is (true? (coll/empty? (range 0))))
  (is (true? (coll/empty? (repeatedly 0 rand))))
  (is (false? (coll/empty? "a")))
  (is (false? (coll/empty? [1])))
  (is (false? (coll/empty? '(1))))
  (is (false? (coll/empty? {:a 1})))
  (is (false? (coll/empty? #{1})))
  (is (false? (coll/empty? (sorted-map :a 1))))
  (is (false? (coll/empty? (sorted-set 1))))
  (is (false? (coll/empty? (range 1))))
  (is (false? (coll/empty? (repeatedly 1 rand)))))

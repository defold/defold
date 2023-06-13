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

(deftest resize-test
  (testing "Asserts on invalid input"
    (is (thrown? AssertionError (coll/resize [] -1 nil)) "Negative count")
    (is (thrown? AssertionError (coll/resize {} 0 nil)) "Non-resizeable coll - unordered")
    (is (thrown? AssertionError (coll/resize (sorted-set) 0 nil)) "Non-resizeable coll - can't hold duplicate fill values"))

  (testing "Returns original coll when already at desired count"
    (let [old-coll [0 1 2]
          new-coll (coll/resize old-coll (count old-coll) nil)]
      (is (identical? old-coll new-coll))))

  (testing "Fills with fill-value when grown"
    (is (= [:a] (coll/resize [] 1 :a)))
    (is (= [:b :b] (coll/resize [] 2 :b))))

  (testing "Retains existing values when grown"
    (is (= [:a nil] (coll/resize [:a] 2 nil)))
    (is (= [0 1 2 0] (coll/resize [0 1 2] 4 0))))

  (testing "Truncates when shrunk"
    (is (= [0 1] (coll/resize [0 1 2] 2 nil)))
    (is (= [] (coll/resize [0 1] 0 nil))))

  (testing "Result has the same type as the input"
    (let [old-coll (vector-of :long 0 1 2)
          new-coll (coll/resize old-coll (dec (count old-coll)) 0)]
      (is (= (type old-coll) (type new-coll))))
    (let [old-coll (vector-of :byte 0 1 2)
          new-coll (coll/resize old-coll (inc (count old-coll)) 0)]
      (is (= (type old-coll) (type new-coll))))))

;; Copyright 2020-2023 The Defold Foundation
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

(deftest separate-by-test
  (testing "Separates by predicate"
    (let [[odds evens] (coll/separate-by odd? [0 1 2 3 4])]
      (is (= [1 3] odds))
      (is (= [0 2 4] evens)))
    (let [[keywords non-keywords] (coll/separate-by keyword? [:a "b" 'c :D "E" 'F])]
      (is (= [:a :D] keywords))
      (is (= ["b" 'c "E" 'F] non-keywords))))

  (testing "Output collections have the same type as the input collection"
    (are [empty-coll]
      (let [[odds evens] (coll/separate-by odd? empty-coll)]
        (is (= empty-coll odds))
        (is (= empty-coll evens))
        (is (= (type empty-coll) (type odds) (type evens))))

      nil '() [] #{} {} (sorted-set) (sorted-map) (vector-of :long)))

  (testing "Works on maps"
    (let [[keywords non-keywords] (coll/separate-by (comp keyword? key) {:a 1 "b" 2 'c 3 :D 4 "E" 5 'F 6})]
      (is (= {:a 1 :D 4} keywords))
      (is (= {"b" 2 'c 3 "E" 5 'F 6} non-keywords)))))

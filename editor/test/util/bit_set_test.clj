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

(ns util.bit-set-test
  (:refer-clojure :exclude [bit-test])
  (:require [clojure.test :refer :all]
            [util.bit-set :as bit-set])
  (:import [java.util BitSet]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- to-vec [^BitSet bit-set]
  (stream-into! [] (.stream bit-set)))

(deftest bit-set?-test
  (is (false? (bit-set/bit-set? nil)))
  (is (false? (bit-set/bit-set? (Object.))))
  (is (false? (bit-set/bit-set? #{})))
  (is (true? (bit-set/bit-set? (bit-set/of-capacity 0)))))

(deftest empty?-test
  (is (true? (bit-set/empty? nil)))
  (is (true? (bit-set/empty? (bit-set/of-capacity 0))))
  (is (false? (bit-set/empty? (bit-set/of 1))))
  (is (true? (bit-set/empty? (bit-set/clear-bit! (bit-set/of 2) 2)))))

(deftest of-test
  (letfn [(check! [expected actual]
            (is (bit-set/bit-set? actual))
            (is (= expected (to-vec actual))))]
    (check! [] (bit-set/of))
    (check! [1] (bit-set/of 1))
    (check! [2 3] (bit-set/of 2 3))
    (check! [4 5 6] (bit-set/of 4 5 6))
    (check! [7 8 9 10] (bit-set/of 7 8 9 10))
    (check! [11 12 13 14 15] (bit-set/of 11 12 13 14 15))))

(deftest of-capacity-test
  (letfn [(check! [expected ^BitSet actual]
            (is (bit-set/bit-set? actual))
            (is (= expected (to-vec actual))))]
    (check! [] (bit-set/of-capacity 0))
    (check! [1] (bit-set/of-capacity 8 1))
    (check! [2 3] (bit-set/of-capacity 16 2 3))
    (check! [4 5 6] (bit-set/of-capacity 32 4 5 6))
    (check! [7 8 9 10] (bit-set/of-capacity 64 7 8 9 10))
    (check! [11 12 13 14 15] (bit-set/of-capacity 128 11 12 13 14 15)))

  (testing "Internal capacity."
    (letfn [(size [^BitSet bit-set] (.size bit-set))]
      (is (= 0 (size (bit-set/of-capacity 0))))
      (is (= 64 (size (bit-set/of-capacity 1))))
      (is (= 64 (size (bit-set/of-capacity 64))))
      (is (= 128 (size (bit-set/of-capacity 65))))
      (is (= 128 (size (bit-set/of-capacity 128))))
      (is (= 192 (size (bit-set/of-capacity 129))))
      (is (= 192 (size (bit-set/of-capacity 192))))
      (is (= 256 (size (bit-set/of-capacity 193))))
      (is (= 256 (size (bit-set/of-capacity 256))))
      (is (= 320 (size (bit-set/of-capacity 257)))))))

(deftest from-test
  (letfn [(check! [expected actual]
            (is (bit-set/bit-set? actual))
            (is (= expected (to-vec actual))))]

    (testing "Without capacity."
      (check! [] (bit-set/from nil))
      (check! [] (bit-set/from []))
      (check! [1] (bit-set/from [1]))
      (check! [2 3] (bit-set/from [2 3]))
      (check! [4 5 6] (bit-set/from [4 5 6]))
      (check! [7 8 9 10] (bit-set/from [7 8 9 10]))
      (check! [11 12 13 14 15] (bit-set/from [11 12 13 14 15])))

    (testing "With capacity."
      (check! [] (bit-set/from 0 nil))
      (check! [] (bit-set/from 8 []))
      (check! [1] (bit-set/from 16 [1]))
      (check! [2 3] (bit-set/from 32 [2 3]))
      (check! [4 5 6] (bit-set/from 64 [4 5 6]))
      (check! [7 8 9 10] (bit-set/from 128 [7 8 9 10]))
      (check! [11 12 13 14 15] (bit-set/from 256 [11 12 13 14 15])))

    (testing "Internal capacity."
      (letfn [(size [^BitSet bit-set] (.size bit-set))]
        (is (= 0 (size (bit-set/from 0 []))))
        (is (= 64 (size (bit-set/from 1 []))))
        (is (= 64 (size (bit-set/from 64 []))))
        (is (= 128 (size (bit-set/from 65 []))))
        (is (= 128 (size (bit-set/from 128 []))))
        (is (= 192 (size (bit-set/from 129 []))))
        (is (= 192 (size (bit-set/from 192 []))))
        (is (= 256 (size (bit-set/from 193 []))))
        (is (= 256 (size (bit-set/from 256 []))))
        (is (= 320 (size (bit-set/from 257 []))))))))

(deftest equality-test
  (is (= (bit-set/of) (bit-set/of)))
  (is (= (bit-set/of 1) (bit-set/of 1)))
  (is (= (bit-set/of 1 2) (bit-set/of 2 1)))
  (is (not= (bit-set/of) (bit-set/of 0)))
  (is (not= (bit-set/of 0) (bit-set/of 1))))

(deftest assign!-test
  (doseq [source [(bit-set/of)
                  (bit-set/of 0)
                  (bit-set/from (range 1 5))]
          target [(bit-set/of)
                  (bit-set/of 10)
                  (bit-set/from (range 3 8))]]
    (is (identical? target (bit-set/assign! target source)))
    (is (= source target))))

(deftest cardinality-test
  (is (= 0 (bit-set/cardinality (bit-set/of))))
  (is (= 1 (bit-set/cardinality (bit-set/of 0))))
  (is (= 2 (bit-set/cardinality (bit-set/of 0 1))))
  (is (= 4 (bit-set/cardinality (bit-set/of 1 3 5 7))))
  (is (= 100 (bit-set/cardinality (bit-set/from (range 100))))))

(deftest bit-test
  (is (false? (bit-set/bit (bit-set/of) 0)))
  (is (true? (bit-set/bit (bit-set/of 0) 0)))
  (is (false? (bit-set/bit (bit-set/of 0) 1)))
  (is (false? (bit-set/bit (bit-set/of 1) 0)))
  (is (true? (bit-set/bit (bit-set/of 1) 1)))
  (is (thrown? IndexOutOfBoundsException (bit-set/bit (bit-set/of 0) -1))))

(deftest set-bit-test
  (letfn [(check! [expected-values original index]
            (let [original-values (to-vec original)
                  actual (bit-set/set-bit original index)]
              (is (bit-set/bit-set? actual))
              (is (not (identical? original actual)))
              (is (= original-values (to-vec original)))
              (is (= expected-values (to-vec actual)))))]
    (check! [0] (bit-set/of) 0)
    (check! [1] (bit-set/of) 1)
    (check! [2 3] (bit-set/of 2) 3))

  (testing "Unaltered returns original."
    (let [original (bit-set/of 3 5)
          actual (bit-set/set-bit original 3)]
      (is (= [3 5] (to-vec actual)))
      (is (identical? original actual)))))

(deftest set-bit!-test
  (letfn [(check! [expected-values original index]
            (let [actual (bit-set/set-bit! original index)]
              (is (bit-set/bit-set? actual))
              (is (identical? original actual))
              (is (= expected-values (to-vec original)))
              (is (= expected-values (to-vec actual)))))]
    (check! [0] (bit-set/of) 0)
    (check! [1] (bit-set/of) 1)
    (check! [2 3] (bit-set/of 2) 3)))

(deftest clear-bit-test
  (letfn [(check! [expected-values original index]
            (let [original-values (to-vec original)
                  actual (bit-set/clear-bit original index)]
              (is (bit-set/bit-set? actual))
              (is (not (identical? original actual)))
              (is (= original-values (to-vec original)))
              (is (= expected-values (to-vec actual)))))]
    (check! [] (bit-set/of 0) 0)
    (check! [4] (bit-set/of 2 4) 2)
    (check! [2 3] (bit-set/of 2 3 4) 4))

  (testing "Unaltered returns original."
    (let [original (bit-set/of 5 7)
          actual (bit-set/clear-bit original 4)]
      (is (= [5 7] (to-vec actual)))
      (is (identical? original actual)))))

(deftest clear-bit!-test
  (letfn [(check! [expected-values original index]
            (let [actual (bit-set/clear-bit! original index)]
              (is (bit-set/bit-set? actual))
              (is (identical? original actual))
              (is (= expected-values (to-vec original)))
              (is (= expected-values (to-vec actual)))))]
    (check! [] (bit-set/of 0) 0)
    (check! [4] (bit-set/of 2 4) 2)
    (check! [2 3] (bit-set/of 2 3 4) 4)))

(deftest or-bits-test
  (letfn [(check! [expected-values original other]
            (let [original-values (to-vec original)
                  other-values (to-vec other)
                  actual (bit-set/or-bits original other)]
              (is (bit-set/bit-set? actual))
              (is (not (identical? original actual)))
              (is (not (identical? other actual)))
              (is (= original-values (to-vec original)))
              (is (= other-values (to-vec other)))
              (is (= expected-values (to-vec actual)))))]
    (check! [1] (bit-set/of) (bit-set/of 1))
    (check! [3 5 7] (bit-set/of 3 7) (bit-set/of 5)))

  (testing "Unaltered returns original."
    (let [original (bit-set/of 3 5)
          other (bit-set/of 3 5)
          actual (bit-set/or-bits original other)]
      (is (= [3 5] (to-vec actual)))
      (is (identical? original actual)))))

(deftest or-bits!-test
  (letfn [(check! [expected-values original other]
            (let [other-values (to-vec other)
                  actual (bit-set/or-bits! original other)]
              (is (bit-set/bit-set? actual))
              (is (identical? original actual))
              (is (not (identical? other actual)))
              (is (= expected-values (to-vec original)))
              (is (= other-values (to-vec other)))
              (is (= expected-values (to-vec actual)))))]
    (check! [1] (bit-set/of) (bit-set/of 1))
    (check! [3 5 7] (bit-set/of 3 7) (bit-set/of 5))))

(deftest and-bits-test
  (letfn [(check! [expected-values original other]
            (let [original-values (to-vec original)
                  other-values (to-vec other)
                  actual (bit-set/and-bits original other)]
              (is (bit-set/bit-set? actual))
              (is (not (identical? original actual)))
              (is (not (identical? other actual)))
              (is (= original-values (to-vec original)))
              (is (= other-values (to-vec other)))
              (is (= expected-values (to-vec actual)))))]
    (check! [2] (bit-set/of 1 2) (bit-set/of 2 3))
    (check! [3 4 5] (bit-set/of 1 2 3 4 5) (bit-set/of 3 4 5 6)))

  (testing "Unaltered returns original."
    (let [original (bit-set/of 3 5)
          other (bit-set/of 3 5)
          actual (bit-set/and-bits original other)]
      (is (= [3 5] (to-vec actual)))
      (is (identical? original actual)))))

(deftest and-bits!-test
  (letfn [(check! [expected-values original other]
            (let [other-values (to-vec other)
                  actual (bit-set/and-bits! original other)]
              (is (bit-set/bit-set? actual))
              (is (identical? original actual))
              (is (not (identical? other actual)))
              (is (= expected-values (to-vec original)))
              (is (= other-values (to-vec other)))
              (is (= expected-values (to-vec actual)))))]
    (check! [2] (bit-set/of 1 2) (bit-set/of 2 3))
    (check! [3 4 5] (bit-set/of 1 2 3 4 5) (bit-set/of 3 4 5 6))))

(deftest seq-test
  (is (nil? (bit-set/seq nil)))
  (is (nil? (bit-set/seq (bit-set/of))))
  (is (seq? (bit-set/seq (bit-set/of 0))))
  (is (= [0] (bit-set/seq (bit-set/of 0))))
  (is (= [2 3 4] (bit-set/seq (bit-set/of 2 3 4)))))

(deftest reduce-test
  (testing "Without init."
    (is (= (reduce + (range 1 5))
           (bit-set/reduce + (bit-set/from (range 1 5)))))
    (is (= (reduce * (range 1 5))
           (bit-set/reduce * (bit-set/from (range 1 5))))))

  (testing "With init."
    (is (= (reduce + 10 (range 1 5))
           (bit-set/reduce + 10 (bit-set/from (range 1 5)))))
    (is (= (reduce * 10 (range 1 5))
           (bit-set/reduce * 10 (bit-set/from (range 1 5)))))))

(deftest transduce-test
  (testing "Without init."
    (is (= (transduce (filter odd?) + (range 1 5))
           (bit-set/transduce (filter odd?) + (bit-set/from (range 1 5)))))
    (is (= (transduce (filter odd?) * (range 1 5))
           (bit-set/transduce (filter odd?) * (bit-set/from (range 1 5))))))

  (testing "With init."
    (is (= (transduce (filter odd?) + 10 (range 1 5))
           (bit-set/transduce (filter odd?) + 10 (bit-set/from (range 1 5)))))
    (is (= (transduce (filter odd?) * 10 (range 1 5))
           (bit-set/transduce (filter odd?) * 10 (bit-set/from (range 1 5)))))))

(deftest into-test
  (testing "bit-set into vector."
    (is (= [1] (bit-set/into [1])))
    (is (= [1] (bit-set/into [1] (bit-set/of))))
    (is (= [1 2 3] (bit-set/into [1] (bit-set/of 2 3))))
    (is (= [1 "1" "2"] (bit-set/into [1] (map str) (bit-set/of 1 2)))))

  (testing "bit-set into bit-set."
    (is (= (bit-set/of 0) (bit-set/into (bit-set/of 0))))
    (is (= (bit-set/of 1) (bit-set/into (bit-set/of) (bit-set/of 1))))
    (is (= (bit-set/of 1 2 3 4) (bit-set/into (bit-set/of 2 4) (bit-set/of 1 3))))
    (is (= (bit-set/of 0 1 3 4) (bit-set/into (bit-set/of 0 1) (map inc) (bit-set/of 2 3)))))

  (testing "vector into bit-set."
    (is (= (bit-set/of 0) (bit-set/into (bit-set/of 0))))
    (is (= (bit-set/of 1) (bit-set/into (bit-set/of) [1])))
    (is (= (bit-set/of 1 2 3 4) (bit-set/into (bit-set/of 2 4) [1 3])))
    (is (= (bit-set/of 0 1 3 4) (bit-set/into (bit-set/of 0 1) (map inc) [2 3]))))

  (testing "vector into vector."
    (is (= [0] (bit-set/into [0])))
    (is (= [1] (bit-set/into [] [1])))
    (is (= [2 4 1 3] (bit-set/into [2 4] [1 3])))
    (is (= [0 1 3 4] (bit-set/into [0 1] (map inc) [2 3])))))

(deftest into->-test
  (testing "bit-set into vector."
    (is (= [] (bit-set/into-> (bit-set/of) [])))
    (is (= [1 2 3] (bit-set/into-> (bit-set/of 1 2 3) [])))
    (is (= [0 1 2 3] (bit-set/into-> (bit-set/of 1 2 3) [0])))
    (is (= ["1" "2" "3"] (bit-set/into-> (bit-set/of 1 2 3) [] (map str))))
    (is (= [4 6 8]
           (bit-set/into-> (bit-set/of 1 2 3) []
             (map inc)
             (map (fn [^long x] (* 2 x)))))))

  (testing "bit-set into bit-set."
    (is (= (bit-set/of) (bit-set/into-> (bit-set/of) (bit-set/of))))
    (is (= (bit-set/of 1 2 3) (bit-set/into-> (bit-set/of 1 2 3) (bit-set/of))))
    (is (= (bit-set/of 0 1 2 3) (bit-set/into-> (bit-set/of 1 2 3) (bit-set/of 0))))
    (is (= (bit-set/of 2 3 4) (bit-set/into-> (bit-set/of 1 2 3) (bit-set/of) (map inc))))
    (is (= (bit-set/of 4 6 8)
           (bit-set/into-> (bit-set/of 1 2 3) (bit-set/of)
             (map inc)
             (map (fn [^long x] (* 2 x))))))))

(deftest indices-test
  (is (= (vector-of :int) (bit-set/indices (bit-set/of))))
  (is (= (vector-of :int 2 3 4) (bit-set/indices (bit-set/of 4 2 3)))))

(deftest previous-set-bit-test
  (is (= -1 (bit-set/previous-set-bit (bit-set/of) 0)))
  (is (= 3 (bit-set/previous-set-bit (bit-set/of 0 2 3) 4)))
  (is (= 3 (bit-set/previous-set-bit (bit-set/of 0 2 3) 3)))
  (is (= 2 (bit-set/previous-set-bit (bit-set/of 0 2 3) 2)))
  (is (= 0 (bit-set/previous-set-bit (bit-set/of 0 2 3) 1)))
  (is (= 0 (bit-set/previous-set-bit (bit-set/of 0 2 3) 0)))
  (is (= -1 (bit-set/previous-set-bit (bit-set/of 0 2 3) -1))))

(deftest next-set-bit-test
  (is (= -1 (bit-set/next-set-bit (bit-set/of) 0)))
  (is (= 0 (bit-set/next-set-bit (bit-set/of 0 2 3) 0)))
  (is (= 2 (bit-set/next-set-bit (bit-set/of 0 2 3) 1)))
  (is (= 2 (bit-set/next-set-bit (bit-set/of 0 2 3) 2)))
  (is (= 3 (bit-set/next-set-bit (bit-set/of 0 2 3) 3)))
  (is (= -1 (bit-set/next-set-bit (bit-set/of 0 2 3) 4))))

(deftest first-set-bit-test
  (is (= -1 (bit-set/first-set-bit (bit-set/of))))
  (is (= 0 (bit-set/first-set-bit (bit-set/of 0))))
  (is (= 1 (bit-set/first-set-bit (bit-set/of 1))))
  (is (= 5 (bit-set/first-set-bit (bit-set/of 8 5)))))

(deftest last-set-bit-test
  (is (= -1 (bit-set/last-set-bit (bit-set/of))))
  (is (= 0 (bit-set/last-set-bit (bit-set/of 0))))
  (is (= 1 (bit-set/last-set-bit (bit-set/of 1))))
  (is (= 8 (bit-set/last-set-bit (bit-set/of 8 5)))))

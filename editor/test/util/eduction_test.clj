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

(ns util.eduction-test
  (:require [clojure.test :refer :all]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [clojure.core Eduction]))

(set! *warn-on-reflection* true)

(defn- eduction? [value]
  (instance? Eduction value))

;; Note: Equality checks are performed twice to root out issues from repeat
;; evaluation of the eduction.

(deftest eduction?-test
  (is (true? (e/eduction? (eduction identity nil))))
  (is (false? (e/eduction? nil)))
  (is (false? (e/eduction? "a")))
  (is (false? (e/eduction? [1])))
  (is (false? (e/eduction? (vector-of :long 1))))
  (is (false? (e/eduction? '(1))))
  (is (false? (e/eduction? #{1})))
  (is (false? (e/eduction? (sorted-set 1))))
  (is (false? (e/eduction? (double-array 1))))
  (is (false? (e/eduction? (object-array 1))))
  (is (false? (e/eduction? (range 1))))
  (is (false? (e/eduction? (repeatedly 1 rand))))
  (is (false? (e/eduction? (sequence (map identity) (range 1))))))

(deftest cat-test
  (let [expected (sequence cat [(range 0 3) (range 3 5)])
        actual (e/cat [(range 0 3) (range 3 5)])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest concat-test
  (let [expected (concat)
        actual (e/concat)]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual)))
  (let [single-arg (range 0 3)
        expected single-arg
        actual (e/concat single-arg)]
    (is (identical? expected actual)))
  (let [ranges (partition-all 4 (range))]
    (doseq [arg-count (range 2 20)]
      (let [args (take arg-count ranges)
            expected (apply concat args)
            actual (apply e/concat args)]
        (is (eduction? actual))
        (is (= expected actual))
        (is (= expected actual))))))

(deftest conj-test
  (let [expected (conj)
        actual (e/conj)]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual)))
  (let [single-arg (range 0 3)
        expected single-arg
        actual (e/conj single-arg)]
    (is (identical? expected actual)))
  (doseq [arg-count (range 2 20)]
    (let [args (range arg-count)
          expected (apply conj [:first] args)
          actual (apply e/conj [:first] args)]
      (is (eduction? actual))
      (is (= expected actual))
      (is (= expected actual)))))

(deftest cons-test
  (let [expected (cons :first (range 0 3))
        actual (e/cons :first (range 0 3))]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest dedupe-test
  (let [expected (dedupe [1 2 1 1 2])
        actual (e/dedupe [1 2 1 1 2])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest distinct-test
  (let [expected (distinct [1 2 1 1 2])
        actual (e/distinct [1 2 1 1 2])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest drop-test
  (let [expected (drop 2 [0 1 2 3 4])
        actual (e/drop 2 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest drop-while-test
  (let [expected (drop-while zero? [0 0 0 1 2 3 4])
        actual (e/drop-while zero? [0 0 0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest filter-test
  (let [expected (filter even? [0 1 2 3 4])
        actual (e/filter even? [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest interpose-test
  (let [expected (interpose :and [0 1 2 3 4])
        actual (e/interpose :and [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest keep-test
  (let [expected (keep #(when (odd? %) (* % %)) [0 1 2 3 4])
        actual (e/keep #(when (odd? %) (* % %)) [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest keep-indexed-test
  (let [expected (keep-indexed #(when (even? %2) (vector %1 %2)) [0 1 2 3 4])
        actual (e/keep-indexed #(when (even? %2) (vector %1 %2)) [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest map-test
  (let [expected (map #(* % %) [0 1 2 3 4])
        actual (e/map #(* % %) [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest map-indexed-test
  (let [expected (map-indexed vector [0 1 2 3 4])
        actual (e/map-indexed vector [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest mapcat-test
  (let [expected (mapcat range [0 1 2 3 4])
        actual (e/mapcat range [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest mapcat-indexed-test
  (let [expected (coll/mapcat-indexed repeat [0 1 2 3 4])
        actual (e/mapcat-indexed repeat [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest partition-all-test
  (let [expected (partition-all 2 [0 1 2 3 4])
        actual (e/partition-all 2 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest partition-by-test
  (let [expected (partition-by even? [0 1 2 3 4])
        actual (e/partition-by even? [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest random-sample-test
  (let [expected (random-sample 0.0 [0 1 2 3 4])
        actual (e/random-sample 0.0 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual)))
  (let [expected (random-sample 1.0 [0 1 2 3 4])
        actual (e/random-sample 1.0 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest remove-test
  (let [expected (remove even? [0 1 2 3 4])
        actual (e/remove even? [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest replace-test
  (let [expected (replace {1 :one 3 :three} [0 1 2 3 4])
        actual (e/replace {1 :one 3 :three} [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest take-test
  (let [expected (take 2 [0 1 2 3 4])
        actual (e/take 2 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest take-nth-test
  (let [expected (take-nth 2 [0 1 2 3 4])
        actual (e/take-nth 2 [0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

(deftest take-while-test
  (let [expected (take-while zero? [0 0 0 1 2 3 4])
        actual (e/take-while zero? [0 0 0 1 2 3 4])]
    (is (eduction? actual))
    (is (= expected actual))
    (is (= expected actual))))

;; Copyright 2020-2024 The Defold Foundation
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
            [util.eduction :as e])
  (:import [clojure.core Eduction]))

(defn- eduction? [value]
  (instance? Eduction value))

(deftest dedupe-test
  (is (= (dedupe [1 2 1 1 2])
         (e/dedupe [1 2 1 1 2])))
  (is (eduction? (e/dedupe [1 2 1 1 2]))))

(deftest distinct-test
  (is (= (distinct [1 2 1 1 2])
         (e/distinct [1 2 1 1 2])))
  (is (eduction? (e/distinct [1 2 1 1 2]))))

(deftest drop-test
  (is (= (drop 2 [0 1 2 3 4])
         (e/drop 2 [0 1 2 3 4])))
  (is (eduction? (e/drop 2 [0 1 2 3 4]))))

(deftest drop-while-test
  (is (= (drop-while zero? [0 0 0 1 2 3 4])
         (e/drop-while zero? [0 0 0 1 2 3 4])))
  (is (eduction? (e/drop-while zero? [0 0 0 1 2 3 4]))))

(deftest filter-test
  (is (= (filter even? [0 1 2 3 4])
         (e/filter even? [0 1 2 3 4])))
  (is (eduction? (e/filter even? [0 1 2 3 4]))))

(deftest interpose-test
  (is (= (interpose :and [0 1 2 3 4])
         (e/interpose :and [0 1 2 3 4])))
  (is (eduction? (e/interpose :and [0 1 2 3 4]))))

(deftest keep-test
  (is (= (keep #(when (odd? %) (* % %)) [0 1 2 3 4])
         (e/keep #(when (odd? %) (* % %)) [0 1 2 3 4])))
  (is (eduction? (e/keep #(when (odd? %) (* % %)) [0 1 2 3 4]))))

(deftest keep-indexed-test
  (is (= (keep-indexed #(when (even? %2) (vector %1 %2)) [0 1 2 3 4])
         (e/keep-indexed #(when (even? %2) (vector %1 %2)) [0 1 2 3 4])))
  (is (eduction? (e/keep-indexed #(when (even? %2) (vector %1 %2)) [0 1 2 3 4]))))

(deftest map-test
  (is (= (map #(* % %) [0 1 2 3 4])
         (e/map #(* % %) [0 1 2 3 4])))
  (is (eduction? (e/map #(* % %) [0 1 2 3 4]))))

(deftest map-indexed-test
  (is (= (map-indexed vector [0 1 2 3 4])
         (e/map-indexed vector [0 1 2 3 4])))
  (is (eduction? (e/map-indexed vector [0 1 2 3 4]))))

(deftest mapcat-test
  (is (= (mapcat range [0 1 2 3 4])
         (e/mapcat range [0 1 2 3 4])))
  (is (eduction? (e/mapcat range [0 1 2 3 4]))))

(deftest partition-all-test
  (is (= (partition-all 2 [0 1 2 3 4])
         (e/partition-all 2 [0 1 2 3 4])))
  (is (eduction? (e/partition-all 2 [0 1 2 3 4]))))

(deftest partition-by-test
  (is (= (partition-by even? [0 1 2 3 4])
         (e/partition-by even? [0 1 2 3 4])))
  (is (eduction? (e/partition-by even? [0 1 2 3 4]))))

(deftest random-sample-test
  (is (= (random-sample 0.0 [0 1 2 3 4])
         (e/random-sample 0.0 [0 1 2 3 4])))
  (is (= (random-sample 1.0 [0 1 2 3 4])
         (e/random-sample 1.0 [0 1 2 3 4])))
  (is (eduction? (e/random-sample 1.0 [0 1 2 3 4]))))

(deftest remove-test
  (is (= (remove even? [0 1 2 3 4])
         (e/remove even? [0 1 2 3 4])))
  (is (eduction? (e/remove even? [0 1 2 3 4]))))

(deftest replace-test
  (is (= (replace {1 :one 3 :three} [0 1 2 3 4])
         (e/replace {1 :one 3 :three} [0 1 2 3 4])))
  (is (eduction? (e/replace {1 :one 3 :three} [0 1 2 3 4]))))

(deftest take-test
  (is (= (take 2 [0 1 2 3 4])
         (e/take 2 [0 1 2 3 4])))
  (is (eduction? (e/take 2 [0 1 2 3 4]))))

(deftest take-nth-test
  (is (= (take-nth 2 [0 1 2 3 4])
         (e/take-nth 2 [0 1 2 3 4])))
  (is (eduction? (e/take-nth 2 [0 1 2 3 4]))))

(deftest take-while-test
  (is (= (take-while zero? [0 0 0 1 2 3 4])
         (e/take-while zero? [0 0 0 1 2 3 4])))
  (is (eduction? (e/take-while zero? [0 0 0 1 2 3 4]))))

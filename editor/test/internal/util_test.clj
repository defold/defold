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

(ns internal.util-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [dynamo.graph :as g]
            [internal.util :as util]))

(deftest test-parse-number-parse-int
  (are [input expected-number expected-int]
    (and (= expected-number (util/parse-number input))
         (= expected-int (util/parse-int input)))
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
    (let [[removal-ops addition-ops] (util/apply-deltas oldset newset (fn [out] (reduce conj [] out)) (fn [in] (reduce conj [] in)))
          old-minus-new              (set/difference oldset newset)
          new-minus-old              (set/difference newset oldset)]
      (and
        (= (count removal-ops) (count old-minus-new))
        (= (count addition-ops) (count new-minus-old))
        (= 1 (apply max 1 (vals (frequencies removal-ops))))
        (= 1 (apply max 1 (vals (frequencies addition-ops))))))))

(deftest stringifying-keywords
  (are [s k] (= s (util/keyword->label k))
    "Word"                :word
    "Two Words"           :two-words
    "2words"              :2words
    "2 Words"             :2-words
    "More Than Two Words" :more-than-two-words
    "More Than 2words"    :more-than2words))

(g/defnk external-fnk [a b c d])

(deftest determining-inputs-required
  (testing "fnks"
    (are [f i] (= i (util/inputs-needed f))
      external-fnk                                                            #{:a :b :c :d}
      #'external-fnk                                                          #{:a :b :c :d}
      (g/fnk [one two three])                                                 #{:one :two :three}
      '(g/fnk [one two three])                                                #{:one :two :three})))

(deftest seq-starts-with?-test
  (is (true? (util/seq-starts-with? [] [])))
  (is (true? (util/seq-starts-with? [1] [])))
  (is (true? (util/seq-starts-with? [1] [1])))
  (is (true? (util/seq-starts-with? [1 2 3] [1 2])))
  (is (true? (util/seq-starts-with? [nil] [nil])))
  (is (true? (util/seq-starts-with? [nil 1 2] [nil 1])))
  (is (true? (util/seq-starts-with? (range 3) (range 2))))
  (is (true? (util/seq-starts-with? "abc" "ab")))
  (is (false? (util/seq-starts-with? [] [1])))
  (is (false? (util/seq-starts-with? [1 2] [1 2 3])))
  (is (false? (util/seq-starts-with? [nil 1] [nil 1 2]))))

(deftest only-test
  (is (= :a (util/only [:a])))
  (is (= :b (util/only #{:b})))
  (is (= :c (util/only '(:c))))
  (is (= [:d 4] (util/only {:d 4})))
  (is (nil? (util/only [nil])))
  (is (nil? (util/only nil)))
  (is (nil? (util/only [])))
  (is (nil? (util/only [:a :b])))
  (is (nil? (util/only #{:a :b})))
  (is (nil? (util/only '(:a :b))))
  (is (nil? (util/only {:a 1 :b 2}))))

(deftest into-multiple-test

  (testing "Standard operation."
    (is (= [] (util/into-multiple)))
    (is (= [] (util/into-multiple [])))
    (is (= [[]] (util/into-multiple [[]])))
    (is (= [{}] (util/into-multiple [{}])))
    (is (= [#{}] (util/into-multiple [#{}])))
    (is (= [#{0 1 2 3 4 5 6 7 8 9}]
           (util/into-multiple [#{}] (range 10))))
    (is (= [#{0 1 2 3 4 5 6 7 8 9} [0 1 2 3 4 5 6 7 8 9]]
           (util/into-multiple [#{} []] (range 10))))
    (is (= [#{0 1 2 3 4 5 6 7 8 9} [0 1 2 3 4 5 6 7 8 9] [0 1 2 3 4 5 6 7 8 9]]
           (util/into-multiple [#{} [] []] (range 10))))
    (is (= [[1 3 5 7 9]]
           (util/into-multiple [[]] [(filter odd?)] (range 10))))
    (is (= [[1 3 5 7 9] [0 2 4 6 8]]
           (util/into-multiple [[] []] [(filter odd?) (filter even?)] (range 10))))
    (is (= [[1 3 5 7 9] [0 2 4 6 8] [1 2 3 4 5 6 7 8 9 10]]
           (util/into-multiple [[] [] []] [(filter odd?) (filter even?) (map inc)] (range 10))))
    (let [[a] (util/into-multiple [[]] [(take 10)] (repeatedly rand))]
      (is (= 10 (count a))))
    (let [[a b] (util/into-multiple [[] []] [(take 10) (take 10)] (repeatedly rand))]
      (is (= a b)))
    (let [[a b c] (util/into-multiple [[] [] []] [(take 10) (take 10) (take 10)] (repeatedly rand))]
      (is (= a b c))))

  (testing "Reduced and completion steps."

    (testing "Take."
      (letfn [(xf []
                (take 3))]
        (is (= [[0 1 2]]
               (util/into-multiple [[]] [(xf)] (range 9))))
        (is (= [[0 1 2]
                [0 1 2]]
               (util/into-multiple [[] []] [(xf) (xf)] (range 9))))
        (is (= [[0 1 2]
                [0 1 2]
                [0 1 2]]
               (util/into-multiple [[] [] []] [(xf) (xf) (xf)] (range 9))))))

    (testing "Partition."
      (letfn [(xf []
                (partition-all 2))]
        (is (= [[[0 1] [2 3] [4 5] [6 7] [8]]]
               (util/into-multiple [[]] [(xf)] (range 9))))
        (is (= [[[0 1] [2 3] [4 5] [6 7] [8]]
                [[0 1] [2 3] [4 5] [6 7] [8]]]
               (util/into-multiple [[] []] [(xf) (xf)] (range 9))))
        (is (= [[[0 1] [2 3] [4 5] [6 7] [8]]
                [[0 1] [2 3] [4 5] [6 7] [8]]
                [[0 1] [2 3] [4 5] [6 7] [8]]]
               (util/into-multiple [[] [] []] [(xf) (xf) (xf)] (range 9))))))

    (testing "Take, then partition."
      (letfn [(xf []
                (comp (take 3)
                      (partition-all 2)))]
        (is (= [[[0 1] [2]]]
               (util/into-multiple [[]] [(xf)] (range 9))))
        (is (= [[[0 1] [2]]
                [[0 1] [2]]]
               (util/into-multiple [[] []] [(xf) (xf)] (range 9))))
        (is (= [[[0 1] [2]]
                [[0 1] [2]]
                [[0 1] [2]]]
               (util/into-multiple [[] [] []] [(xf) (xf) (xf)] (range 9))))))

    (testing "Partition, then take."
      (letfn [(xf []
                (comp (partition-all 2)
                      (take 3)))]
        (is (= [[[0 1] [2 3] [4 5]]]
               (util/into-multiple [[]] [(xf)] (range 9))))
        (is (= [[[0 1] [2 3] [4 5]]
                [[0 1] [2 3] [4 5]]]
               (util/into-multiple [[] []] [(xf) (xf)] (range 9))))
        (is (= [[[0 1] [2 3] [4 5]]
                [[0 1] [2 3] [4 5]]
                [[0 1] [2 3] [4 5]]]
               (util/into-multiple [[] [] []] [(xf) (xf) (xf)] (range 9)))))))

  (testing "Asserts when the number of destinations differ from the number of xforms."
    (let [xf (map inc)
          coll (range 10)]
      (is (thrown? AssertionError (util/into-multiple [] [xf] coll)))

      (is (thrown? AssertionError (util/into-multiple [[]] [] coll)))
      (is (thrown? AssertionError (util/into-multiple [[]] [xf xf] coll)))

      (is (thrown? AssertionError (util/into-multiple [[] []] [] coll)))
      (is (thrown? AssertionError (util/into-multiple [[] []] [xf] coll)))
      (is (thrown? AssertionError (util/into-multiple [[] []] [xf xf xf] coll)))

      (is (thrown? AssertionError (util/into-multiple [[] [] []] [] coll)))
      (is (thrown? AssertionError (util/into-multiple [[] [] []] [xf] coll)))
      (is (thrown? AssertionError (util/into-multiple [[] [] []] [xf xf] coll)))
      (is (thrown? AssertionError (util/into-multiple [[] [] []] [xf xf xf xf] coll))))))

(defn elements-identical? [a b]
  (every? true? (map identical? a b)))

(deftest make-dedupe-fn-test
  (let [original-items (mapv #(str (mod % 10)) (range 100))
        dedupe-fn (util/make-dedupe-fn original-items)
        additional-items (mapv #(str (mod % 10)) (range 100))
        partitions (partition 10 (map dedupe-fn (concat original-items additional-items)))]
    (is (every? (partial elements-identical? (first partitions))
                partitions))))

(deftest name-index-test
  (testing "renames"
    (letfn [(renames [xs ys]
              (util/detect-renames
                (util/name-index xs identity)
                (util/name-index ys identity)))]
      (is (= {}
             (renames [:a :b :c]
                      [:b :c :a])))
      (is (= {[:a 0] [:d 0]}
             (renames [:a :b :c]
                      [:b :c :d])))
      (is (= {[:a 1] [:c 0]}
             (renames [:a :b :b :a]
                      [:b :a :b :c])))
      (is (= {}
             (renames [:b :a :b :c]
                      [:a :b])))
      (is (= {}
             (renames [:a :b]
                      [:b :a :b :c])))))
  (testing "deletions"
    (letfn [(deletions [xs ys]
              (util/detect-deletions
                (util/name-index xs identity)
                (util/name-index ys identity)))]
      (is (= #{[:b 0]}
             (deletions [:a :b :c]
                        [:a :c])))
      (is (= #{}
             (deletions [:a :b :c]
                        [:a :c :d :e])))
      (is (= #{[:a 0] [:b 0]}
             (deletions [:a :b :c]
                        [:c])))
      (is (= #{[:b 0] [:c 0]}
             (deletions [:a :b :c]
                        [:d]))))))

(ns internal.util-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [internal.util :refer :all]))

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
      #'external-fnk                                                          #{:a :b :c :d}
      (g/fnk [one two three])                                                 #{:one :two :three}
      '(g/fnk [one two three])                                                #{:one :two :three})))

(deftest seq-starts-with?-test
  (is (true? (seq-starts-with? [] [])))
  (is (true? (seq-starts-with? [1] [])))
  (is (true? (seq-starts-with? [1] [1])))
  (is (true? (seq-starts-with? [1 2 3] [1 2])))
  (is (true? (seq-starts-with? [nil] [nil])))
  (is (true? (seq-starts-with? [nil 1 2] [nil 1])))
  (is (true? (seq-starts-with? (range 3) (range 2))))
  (is (true? (seq-starts-with? "abc" "ab")))
  (is (false? (seq-starts-with? [] [1])))
  (is (false? (seq-starts-with? [1 2] [1 2 3])))
  (is (false? (seq-starts-with? [nil 1] [nil 1 2]))))

(deftest first-where-test
  (is (= 2 (first-where even? (range 1 4))))
  (is (nil? (first-where nil? [:a :b nil :d])))
  (is (= [:d 4] (first-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= :e (first-where #(= :e %) (list :a nil :c nil :e))))
  (is (= "f" (first-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (first-where nil? nil)))
  (is (nil? (first-where even? nil)))
  (is (nil? (first-where even? [])))
  (is (nil? (first-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (test-util/make-call-logger (constantly true))]
      (is (= 0 (first-where pred (range 10))))
      (is (= 1 (count (test-util/call-logger-calls pred)))))))

(deftest first-index-where-test
  (is (= 1 (first-index-where even? (range 1 4))))
  (is (= 2 (first-index-where nil? [:a :b nil :d])))
  (is (= 3 (first-index-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= 4 (first-index-where #(= :e %) (list :a nil :c nil :e))))
  (is (= 5 (first-index-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (first-index-where nil? nil)))
  (is (nil? (first-index-where even? nil)))
  (is (nil? (first-index-where even? [])))
  (is (nil? (first-index-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (test-util/make-call-logger (constantly true))]
      (is (= 0 (first-index-where pred (range 10))))
      (is (= 1 (count (test-util/call-logger-calls pred)))))))

(deftest only-test
  (is (= :a (only [:a])))
  (is (= :b (only #{:b})))
  (is (= :c (only '(:c))))
  (is (= [:d 4] (only {:d 4})))
  (is (nil? (only [nil])))
  (is (nil? (only nil)))
  (is (nil? (only [])))
  (is (nil? (only [:a :b])))
  (is (nil? (only #{:a :b})))
  (is (nil? (only '(:a :b))))
  (is (nil? (only {:a 1 :b 2}))))

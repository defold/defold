(ns internal.util-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
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

(deftest first-where-test
  (is (= 2 (util/first-where even? (range 1 4))))
  (is (nil? (util/first-where nil? [:a :b nil :d])))
  (is (= [:d 4] (util/first-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= :e (util/first-where #(= :e %) (list :a nil :c nil :e))))
  (is (= "f" (util/first-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (util/first-where nil? nil)))
  (is (nil? (util/first-where even? nil)))
  (is (nil? (util/first-where even? [])))
  (is (nil? (util/first-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (test-util/make-call-logger (constantly true))]
      (is (= 0 (util/first-where pred (range 10))))
      (is (= 1 (count (test-util/call-logger-calls pred)))))))

(deftest first-index-where-test
  (is (= 1 (util/first-index-where even? (range 1 4))))
  (is (= 2 (util/first-index-where nil? [:a :b nil :d])))
  (is (= 3 (util/first-index-where #(Character/isDigit %) "abc123def")))
  (is (= 3 (util/first-index-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= 4 (util/first-index-where #(= :e %) (list :a nil :c nil :e))))
  (is (= 5 (util/first-index-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (util/first-index-where nil? nil)))
  (is (nil? (util/first-index-where even? nil)))
  (is (nil? (util/first-index-where even? [])))
  (is (nil? (util/first-index-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (test-util/make-call-logger (constantly true))]
      (is (= 0 (util/first-index-where pred (range 10))))
      (is (= 1 (count (test-util/call-logger-calls pred)))))))

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

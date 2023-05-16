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

(deftest mapcat-indexed-test
  (is (= [[:< 0 :a]
          [:> 0 :a]
          [:< 1 :b]
          [:> 1 :b]]
         (coll/mapcat-indexed (fn [index value]
                                [[:< index value]
                                 [:> index value]])
                              [:a :b])))
  (is (= #{[:< 0 :a]
           [:> 0 :a]
           [:< 1 :b]
           [:> 1 :b]}
         (into #{}
               (coll/mapcat-indexed (fn [index value]
                                      [[:< index value]
                                       [:> index value]]))
               [:a :b]))))

(defrecord SearchTestRecord [name])

(deftest search-test
  (testing "Traverses maps and seqs."
    (is (= (repeat 9 "needle")
           (coll/search
             {:list [{:list-in-list ["other" "needle"]
                      :map-in-list {"string-key" "other" :keyword-key "needle"}
                      :set-in-list (sorted-set "other" "needle")}]
              :map {:list-in-map ["other" "needle"]
                    :map-in-map {"string-key" "other" :keyword-key "needle"}
                    :set-in-map (sorted-set "other" "needle")}
              :set #{{:list-in-set ["other" "needle"]
                      :map-in-set {"string-key" "other" :keyword-key "needle"}
                      :set-in-set (sorted-set "other" "needle")}}}
             (fn [value]
               (when (= "needle" value)
                 value))))))

  (testing "Does not traverse record fields."
    (let [record (SearchTestRecord. "needle")
          coll {:record record}]
      (is (= []
             (coll/search
               coll
               (fn [value]
                 (when (= "needle" value)
                   value)))))
      (is (= [record]
             (coll/search
               coll
               (fn [value]
                 (when (= record value)
                   value)))))))

  (testing "Applies transformation."
    (is (= ["needle!!!"]
           (coll/search
             {:list ["other" "needle"]}
             (fn [value]
               (when (= "needle" value)
                 (str value "!!!"))))))))

(deftest search-with-path-test
  (testing "Traverses maps and seqs."
    (is (= [["needle" [:list 0 :list-in-list 1]]
            ["needle" [:list 0 :map-in-list :keyword-key]]
            ["needle" [:list 0 :set-in-list 0]]
            ["needle" [:map :list-in-map 1]]
            ["needle" [:map :map-in-map :keyword-key]]
            ["needle" [:map :set-in-map 0]]
            ["needle" [:set 0 :list-in-set 1]]
            ["needle" [:set 0 :map-in-set :keyword-key]]
            ["needle" [:set 0 :set-in-set 0]]]
           (coll/search-with-path
             {:list [{:list-in-list ["other" "needle"]
                      :map-in-list {"string-key" "other" :keyword-key "needle"}
                      :set-in-list (sorted-set "other" "needle")}]
              :map {:list-in-map ["other" "needle"]
                    :map-in-map {"string-key" "other" :keyword-key "needle"}
                    :set-in-map (sorted-set "other" "needle")}
              :set #{{:list-in-set ["other" "needle"]
                      :map-in-set {"string-key" "other" :keyword-key "needle"}
                      :set-in-set (sorted-set "other" "needle")}}}
             []
             (fn [value]
               (when (= "needle" value)
                 value))))))

  (testing "Does not traverse record fields."
    (let [record (SearchTestRecord. "needle")
          coll {:record record}]
      (is (= []
             (coll/search-with-path
               coll
               []
               (fn [value]
                 (when (= "needle" value)
                   value)))))
      (is (= [[record [:record]]]
             (coll/search-with-path
               coll
               []
               (fn [value]
                 (when (= record value)
                   value)))))))

  (testing "Applies transformation."
    (is (= [["needle!!!" [:list 1]]]
           (coll/search-with-path
             {:list ["other" "needle"]}
             []
             (fn [value]
               (when (= "needle" value)
                 (str value "!!!")))))))

  (testing "Conjoins path tokens into init-path."
    (is (= [["needle" '(1 :list-in-list 0 :list)]
            ["needle" '(:keyword-key :map-in-list 0 :list)]
            ["needle" '(0 :set-in-list 0 :list)]
            ["needle" '(1 :list-in-map :map)]
            ["needle" '(:keyword-key :map-in-map :map)]
            ["needle" '(0 :set-in-map :map)]
            ["needle" '(1 :list-in-set 0 :set)]
            ["needle" '(:keyword-key :map-in-set 0 :set)]
            ["needle" '(0 :set-in-set 0 :set)]]
           (coll/search-with-path
             {:list [{:list-in-list ["other" "needle"]
                      :map-in-list {"string-key" "other" :keyword-key "needle"}
                      :set-in-list (sorted-set "other" "needle")}]
              :map {:list-in-map ["other" "needle"]
                    :map-in-map {"string-key" "other" :keyword-key "needle"}
                    :set-in-map (sorted-set "other" "needle")}
              :set #{{:list-in-set ["other" "needle"]
                      :map-in-set {"string-key" "other" :keyword-key "needle"}
                      :set-in-set (sorted-set "other" "needle")}}}
             '()
             (fn [value]
               (when (= "needle" value)
                 value)))))))

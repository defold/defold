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

(deftest supports-transient?-test
  (is (true? (coll/supports-transient? [])))
  (is (true? (coll/supports-transient? {})))
  (is (true? (coll/supports-transient? #{})))
  (is (false? (coll/supports-transient? nil)))
  (is (false? (coll/supports-transient? "")))
  (is (false? (coll/supports-transient? '())))
  (is (false? (coll/supports-transient? (vector-of :long))))
  (is (false? (coll/supports-transient? (sorted-map))))
  (is (false? (coll/supports-transient? (sorted-set))))
  (is (false? (coll/supports-transient? (range 0))))
  (is (false? (coll/supports-transient? (repeatedly 0 rand)))))

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

(deftest empty?-test
  (is (true? (coll/empty? nil)))
  (is (true? (coll/empty? "")))
  (is (true? (coll/empty? [])))
  (is (true? (coll/empty? (vector-of :long))))
  (is (true? (coll/empty? '())))
  (is (true? (coll/empty? {})))
  (is (true? (coll/empty? #{})))
  (is (true? (coll/empty? (sorted-map))))
  (is (true? (coll/empty? (sorted-set))))
  (is (true? (coll/empty? (range 0))))
  (is (true? (coll/empty? (repeatedly 0 rand))))
  (is (false? (coll/empty? "a")))
  (is (false? (coll/empty? [1])))
  (is (false? (coll/empty? (vector-of :long 1))))
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

(deftest separate-by-test
  (testing "Separates by predicate"
    (let [[odds evens] (coll/separate-by odd? [0 1 2 3 4])]
      (is (= [1 3] odds))
      (is (= [0 2 4] evens)))
    (let [[keywords non-keywords] (coll/separate-by keyword? [:a "b" 'c :D "E" 'F])]
      (is (= [:a :D] keywords))
      (is (= ["b" 'c "E" 'F] non-keywords))))

  (testing "Works on maps"
    (let [[keywords non-keywords] (coll/separate-by (comp keyword? key) {:a 1 "b" 2 'c 3 :D 4 "E" 5 'F 6})]
      (is (= {:a 1 :D 4} keywords))
      (is (= {"b" 2 'c 3 "E" 5 'F 6} non-keywords))))

  (testing "Returns identical when empty"
    (doseq [empty-coll [nil '() [] #{} {} (sorted-set) (sorted-map) (vector-of :long)]]
      (let [[odds evens] (coll/separate-by odd? empty-coll)]
        (is (identical? empty-coll odds))
        (is (identical? empty-coll evens)))))

  (testing "Output collections have the same type as the input collection"
    (testing "List types"
      (doseq [empty-coll ['() [] #{} (sorted-set) (vector-of :long)]]
        (let [coll (into empty-coll
                         (range 5))
              [odds evens] (coll/separate-by odd? coll)]
          (is (= (type coll) (type odds) (type evens))))))

    (testing "Map types"
      (doseq [empty-coll [{} (sorted-map)]]
        (let [coll (into empty-coll
                         (map (juxt identity identity))
                         (range 5))
              [odds evens] (coll/separate-by (comp odd? key) coll)]
          (is (= (type coll) (type odds) (type evens)))))))

  (testing "Output collections retain metadata from the input collection"
    (testing "List types"
      (doseq [empty-coll ['() [] #{} (sorted-set) (vector-of :long)]]
        (let [coll (with-meta (into empty-coll
                                    (range 5))
                              {:meta-key "meta-value"})
              [odds evens] (coll/separate-by odd? coll)]
          (is (identical? (meta coll) (meta odds)))
          (is (identical? (meta coll) (meta evens))))))

    (testing "Map types"
      (doseq [empty-coll [{} (sorted-map)]]
        (let [coll (with-meta (into empty-coll
                                    (map (juxt identity identity))
                                    (range 5))
                              {:meta-key "meta-value"})
              [odds evens] (coll/separate-by (comp odd? key) coll)]
          (is (identical? (meta coll) (meta odds)))
          (is (identical? (meta coll) (meta evens))))))))

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

(ns util.coll-test
  (:require [clojure.core :as core]
            [clojure.test :refer :all]
            [util.coll :as coll])
  (:import [clojure.lang IPersistentVector]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest list-or-cons?-test
  (is (true? (coll/list-or-cons? '())))
  (is (true? (coll/list-or-cons? `())))
  (is (true? (coll/list-or-cons? '(1))))
  (is (true? (coll/list-or-cons? `(1))))
  (is (true? (coll/list-or-cons? '(1 2))))
  (is (true? (coll/list-or-cons? `(1 2))))
  (is (true? (coll/list-or-cons? (list))))
  (is (true? (coll/list-or-cons? (list 1))))
  (is (true? (coll/list-or-cons? (list 1 2))))
  (is (true? (coll/list-or-cons? (cons 1 nil))))
  (is (true? (coll/list-or-cons? (cons 1 (cons 2 nil)))))
  (is (false? (coll/list-or-cons? nil)))
  (is (false? (coll/list-or-cons? "")))
  (is (false? (coll/list-or-cons? "a")))
  (is (false? (coll/list-or-cons? [])))
  (is (false? (coll/list-or-cons? [1])))
  (is (false? (coll/list-or-cons? (vector-of :long))))
  (is (false? (coll/list-or-cons? (vector-of :long 1))))
  (is (false? (coll/list-or-cons? {})))
  (is (false? (coll/list-or-cons? {:a 1})))
  (is (false? (coll/list-or-cons? #{})))
  (is (false? (coll/list-or-cons? #{1})))
  (is (false? (coll/list-or-cons? (sorted-map))))
  (is (false? (coll/list-or-cons? (sorted-map :a 1))))
  (is (false? (coll/list-or-cons? (sorted-set))))
  (is (false? (coll/list-or-cons? (sorted-set 1))))
  (is (false? (coll/list-or-cons? (coll/pair 1 2))))
  (is (false? (coll/list-or-cons? (range 1))))
  (is (false? (coll/list-or-cons? (repeatedly 1 rand)))))

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
  (is (instance? IPersistentVector (coll/pair 1 2)))
  (is (counted? (coll/pair 1 2)))
  (is (= 2 (count (coll/pair 1 2))))
  (is (= [1 2] (coll/pair 1 2))))

(deftest pair-fn-test
  (testing "Specified key-fn"
    (let [make-pair (coll/pair-fn keyword)
          pair-one (make-pair "one")]
      (is (instance? IPersistentVector pair-one))
      (is (counted? pair-one))
      (is (= 2 (count pair-one)))
      (is (= [:one "one"] pair-one))))

  (testing "Specified key-fn and value-fn"
    (let [make-pair (coll/pair-fn symbol keyword)
          pair-two (make-pair "two")]
      (is (instance? IPersistentVector pair-two))
      (is (counted? pair-two))
      (is (= 2 (count pair-two)))
      (is (= ['two :two] pair-two))))

  (testing "Argument expressions should not be inlined"
    (let [key-fn-atom (atom {"item" 1})
          value-fn-atom (atom {"item" :a})
          make-pair (coll/pair-fn @key-fn-atom)
          make-transformed-pair (coll/pair-fn @key-fn-atom @value-fn-atom)]
      (is (= [1 "item"] (make-pair "item")))
      (is (= [1 :a] (make-transformed-pair "item")))
      (swap! key-fn-atom assoc "item" 2)
      (swap! value-fn-atom assoc "item" :b)
      (is (= [1 "item"] (make-pair "item")))
      (is (= [1 :a] (make-transformed-pair "item"))))))

(deftest flip-test
  (doseq [original [[1 2] (coll/pair 1 2)]]
    (is (instance? IPersistentVector (coll/flip original)))
    (is (counted? (coll/flip original)))
    (is (= 2 (count (coll/flip original))))
    (is (= [2 1] (coll/flip original)))))

(deftest flipped-pair-test
  (is (instance? IPersistentVector (coll/flipped-pair 1 2)))
  (is (counted? (coll/flipped-pair 1 2)))
  (is (= 2 (count (coll/flipped-pair 1 2))))
  (is (= [2 1] (coll/flipped-pair 1 2))))

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

(deftest empty-with-meta-test
  (letfn [(check! [coll]
            (is (not (core/empty? coll)) "Tested collections should have items to ensure emptiness is tested.")
            (let [original-meta {:version "original"}
                  coll-with-meta (with-meta coll original-meta)
                  empty-coll (coll/empty-with-meta coll-with-meta)]
              (is (core/empty? empty-coll))
              (is (identical? original-meta (meta empty-coll)))))]
    (check! [1])
    (check! (vector-of :long 1))
    (check! '(1))
    (check! {1 1})
    (check! #{1})
    (check! (sorted-map 1 1))
    (check! (sorted-set 1))
    (check! (range 1))
    (check! (range (double 1)))
    (check! (repeatedly 1 rand))))

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

(deftest not-empty-test
  (is (nil? (coll/not-empty nil)))
  (is (nil? (coll/not-empty "")))
  (is (nil? (coll/not-empty [])))
  (is (nil? (coll/not-empty (vector-of :long))))
  (is (nil? (coll/not-empty '())))
  (is (nil? (coll/not-empty {})))
  (is (nil? (coll/not-empty #{})))
  (is (nil? (coll/not-empty (sorted-map))))
  (is (nil? (coll/not-empty (sorted-set))))
  (is (nil? (coll/not-empty (range 0))))
  (is (nil? (coll/not-empty (repeatedly 0 rand))))
  (letfn [(returns-input? [input]
            (identical? input (coll/not-empty input)))]
    (is (returns-input? "a"))
    (is (returns-input? [1]))
    (is (returns-input? (vector-of :long 1)))
    (is (returns-input? '(1)))
    (is (returns-input? {:a 1}))
    (is (returns-input? #{1}))
    (is (returns-input? (sorted-map :a 1)))
    (is (returns-input? (sorted-set 1)))
    (is (returns-input? (range 1)))
    (is (returns-input? (repeatedly 1 rand)))))

(deftest pair-map-by-test
  (testing "Works as a transducer with key-fn"
    (let [result (into (sorted-map)
                       (coll/pair-map-by keyword)
                       ["one" "two"])]
      (is (sorted? result))
      (is (= {:one "one" :two "two"} result))))

  (testing "Works as a transducer with key-fn and value-fn"
    (let [result (into (sorted-map)
                       (coll/pair-map-by symbol keyword)
                       ["one" "two"])]
      (is (sorted? result))
      (is (= {'one :one 'two :two} result))))

  (testing "Applies key-fn on input sequence"
    (let [result (coll/pair-map-by keyword ["one" "two"])]
      (is (map? result))
      (is (= {:one "one" :two "two"} result))))

  (testing "Applies key-fn and value-fn on input sequence"
    (let [result (coll/pair-map-by symbol keyword ["one" "two"])]
      (is (map? result))
      (is (= {'one :one 'two :two} result)))))

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

(deftest aggregate-into-test
  (testing "Aggregates items into the specified associative."
    (is (= {\a 5
            \b 7
            \c 0}
           (coll/aggregate-into {\c 0}
                                (juxt first count)
                                +
                                ["apple" "book" "box"]))))

  (testing "Excludes nil pair-fn results."
    (is (= {\a 5
            \b 3}
           (coll/aggregate-into {}
                                (fn pair-fn [item]
                                  (when (not= "book" item)
                                    [(first item)
                                     (count item)]))
                                +
                                ["apple" "book" "box"]))))

  (testing "Uses supplied init values as the starting point."
    (is (= {\a #{:init :a1 :a2}
            \b #{:init :b1}
            \c #{:c1}}
           (coll/aggregate-into {\c #{:c1}}
                                (juxt first keyword)
                                conj
                                #{:init}
                                ["a1" "b1" "a2"]))))

  (testing "Uses values produced by supplied init function as the starting point."
    (is (= {\a [:a1 :a2]
            \b [:b1]}
           (into {}
                 (map (fn [[key value]]
                        [key (persistent! value)]))
                 (coll/aggregate-into {}
                                      (juxt first keyword)
                                      conj!
                                      #(transient [])
                                      ["a1" "b1" "a2"])))))

  (testing "Does nothing and returns target coll when there are no items."
    (doseq [empty-items [nil '() [] #{} {} (sorted-set) (sorted-map) (vector-of :long)]]
      (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
        (let [result (coll/aggregate-into target-coll
                                          (fn pair-fn [_] (assert false))
                                          (fn accumulate-fn [_ _] (assert false))
                                          (fn init-fn [] (assert false))
                                          empty-items)]
          (is (identical? target-coll result))))))

  (testing "Preserves metadata."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta target-coll original-meta)
            altered-map (coll/aggregate-into original-map
                                             (juxt first keyword)
                                             conj
                                             ["a1" "b1" "a2"])]
        (is (identical? original-meta (meta altered-map)))))))

(deftest mapcat-test
  (testing "Over collection."
    (is (= [[:< :a]
            [:> :a]
            [:< :b]
            [:> :b]]
           (coll/mapcat (fn [value]
                          [[:< value]
                           [:> value]])
                        [:a :b])))
    (is (= [[:< 0 :a]
            [:> 0 :a]
            [:< 1 :b]
            [:> 1 :b]]
           (coll/mapcat (fn [num value]
                          [[:< num value]
                           [:> num value]])
                        (range 10)
                        [:a :b])))
    (is (= [[:< 0 :a \A]
            [:> 0 :a \A]
            [:< 1 :b \B]
            [:> 1 :b \B]]
           (coll/mapcat (fn [num value char]
                          [[:< num value char]
                           [:> num value char]])
                        (range 10)
                        [:a :b]
                        "ABC"))))

  (testing "As transducer."
    (is (= [[:< :a]
            [:> :a]
            [:< :b]
            [:> :b]]
           (into []
                 (coll/mapcat (fn [value]
                                [[:< value]
                                 [:> value]]))
                 [:a :b])))
    (is (= [[:< 0 :a]
            [:> 0 :a]
            [:< 1 :b]
            [:> 1 :b]]
           (sequence
             (coll/mapcat (fn [num value]
                            [[:< num value]
                             [:> num value]]))
             (range 10)
             [:a :b])))
    (is (= [[:< 0 :a \A]
            [:> 0 :a \A]
            [:< 1 :b \B]
            [:> 1 :b \B]]
           (sequence
             (coll/mapcat (fn [num value char]
                            [[:< num value char]
                             [:> num value char]]))
             (range 10)
             [:a :b]
             "ABC")))))

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

(deftest assoc-in-ex-test
  (testing "Calls empty-fn with the key-path for levels that do not exist."
    (let [empty-fn (fn empty-fn [parent-coll coll-key nested-key-path]
                     (is (vector? nested-key-path))
                     {:parent-coll parent-coll
                      :coll-key coll-key
                      :nested-key-path nested-key-path})]
      (is (= {:parent-coll nil
              :coll-key nil
              :nested-key-path [:a]
              :a 1}
             (coll/assoc-in-ex nil [:a] 1 empty-fn)))
      (is (= {:parent-coll nil
              :coll-key nil
              :nested-key-path [:a :b]
              :a {:parent-coll {:parent-coll nil
                                :coll-key nil
                                :nested-key-path [:a :b]}
                  :coll-key :a
                  :nested-key-path [:b]
                  :b 1}}
             (coll/assoc-in-ex nil [:a :b] 1 empty-fn)))
      (is (= {:parent-coll nil
              :coll-key nil
              :nested-key-path [:a :b :c]
              :a {:parent-coll {:parent-coll nil
                                :coll-key nil
                                :nested-key-path [:a :b :c]}
                  :coll-key :a
                  :nested-key-path [:b :c]
                  :b {:parent-coll {:parent-coll {:parent-coll nil
                                                  :coll-key nil
                                                  :nested-key-path [:a :b :c]}
                                    :coll-key :a
                                    :nested-key-path [:b :c]}
                      :coll-key :b
                      :nested-key-path [:c]
                      :c 1}}}
             (coll/assoc-in-ex nil [:a :b :c] 1 empty-fn)))))

  (testing "Retains data in existing levels."
    (let [empty-fn (fn empty-fn [key-path]
                     (throw (ex-info "empty-fn called unexpectedly."
                                     {:key-path key-path})))
          original-map (sorted-map :a 0 :b (sorted-map :A 1 :B 2))
          altered-map (coll/assoc-in-ex original-map [:b :C] 3 empty-fn)]
      (is (= {:a 0 :b {:A 1 :B 2 :C 3}} altered-map))))

  (testing "Preserves metadata."
    (let [empty-fn (fn empty-fn [key-path]
                     (throw (ex-info "empty-fn called unexpectedly."
                                     {:key-path key-path})))
          original-meta {:meta-key "meta-value"}
          original-map (with-meta (sorted-map :a (with-meta (sorted-map) original-meta)) original-meta)
          altered-map (coll/assoc-in-ex original-map [:a :b] 1 empty-fn)]
      (is (identical? original-meta (meta altered-map)))
      (is (identical? original-meta (meta (:a altered-map))))))

  (testing "Throws on conflict."
    (let [empty-fn (constantly {})]
      (is (thrown-with-msg? ClassCastException #"cannot be cast to class clojure.lang.Associative"
                            (coll/assoc-in-ex {:a "existing"} [:a :b] 1 empty-fn)))
      (is (thrown-with-msg? ClassCastException #"cannot be cast to class clojure.lang.Associative"
                            (coll/assoc-in-ex {:a false} [:a :b] 1 empty-fn)))))

  (testing "Existing nil value treated as empty."
    (let [empty-fn (fn empty-fn [parent-coll coll-key nested-key-path]
                     {:parent-coll parent-coll
                      :coll-key coll-key
                      :nested-key-path nested-key-path})]
      (is (= {:a {:parent-coll {:a nil}
                  :coll-key :a
                  :nested-key-path [:b]
                  :b 1}}
             (coll/assoc-in-ex {:a nil} [:a :b] 1 empty-fn))))))

(deftest default-assoc-in-empty-fn-test
  (letfn [(default-assoc-in [coll key-path value]
            (coll/assoc-in-ex coll key-path value coll/default-assoc-in-empty-fn))]

    (testing "Introduces hash maps for levels that do not exist."
      (let [altered-map (default-assoc-in nil [:a] 1)]
        (is (map? altered-map))
        (is (not (sorted? altered-map)))
        (is (= 1 (:a altered-map))))
      (let [original-map (sorted-map)
            altered-map (default-assoc-in original-map [:a :b] 1)
            created-level (:a altered-map)]
        (is (map? created-level))
        (is (not (sorted? created-level)))
        (is (= 1 (:b created-level)))))

    (testing "Introduces vectors for integer-addressed levels that do not exist."
      (is (= [1] (default-assoc-in nil [0] 1)))
      (is (= {:a [1]} (default-assoc-in nil [:a 0] 1)))
      (is (= {:a [{:b 1}]} (default-assoc-in nil [:a 0 :b] 1)))
      (is (= {:a [{:b [1]}]} (default-assoc-in nil [:a 0 :b 0] 1))))))

(deftest sorted-assoc-in-empty-fn-test
  (letfn [(sorted-assoc-in [coll key-path value]
            (coll/assoc-in-ex coll key-path value coll/sorted-assoc-in-empty-fn))]

    (testing "Introduces sorted maps for levels that do not exist."
      (let [altered-map (sorted-assoc-in nil [:a] 1)]
        (is (map? altered-map))
        (is (sorted? altered-map))
        (is (= 1 (:a altered-map))))
      (let [original-map (sorted-map)
            altered-map (sorted-assoc-in original-map [:a :b] 1)
            created-level (:a altered-map)]
        (is (map? created-level))
        (is (sorted? created-level))
        (is (= 1 (:b created-level)))))

    (testing "Introduces vectors for integer-addressed levels that do not exist."
      (is (= [1] (sorted-assoc-in nil [0] 1)))
      (is (= {:a [1]} (sorted-assoc-in nil [:a 0] 1)))
      (is (= {:a [{:b 1}]} (sorted-assoc-in nil [:a 0 :b] 1)))
      (is (= {:a [{:b [1]}]} (sorted-assoc-in nil [:a 0 :b 0] 1))))))

(deftest nested-map->path-map-test
  (testing "Transforms nested map to flat map of paths"
    (is (= {[:a :A] 1
            [:a :B] 2
            [:b :A] 1
            [:b :B "A"] 1
            [:c] 3}
           (coll/nested-map->path-map
             {:a {:A 1 :B 2}
              :b {:A 1 :B {"A" 1}}
              :c 3}))))

  (testing "Preserves type"
    (is (not (sorted? (coll/nested-map->path-map {:a {:A 1}}))))
    (is (sorted? (coll/nested-map->path-map (sorted-map :a (sorted-map :A 1)))))))

(deftest path-map->nested-map-test
  (testing "Transforms flat map of paths to nested map"
    (is (= {:a {:A 1 :B 2}
            :b {:A 1 :B {"A" 1}}
            :c 3}
           (coll/path-map->nested-map
             {[:a :A] 1
              [:a :B] 2
              [:b :A] 1
              [:b :B "A"] 1
              [:c] 3}))))

  (testing "Preserves type"
    (is (not (sorted? (coll/path-map->nested-map {}))))
    (is (sorted? (coll/path-map->nested-map (sorted-map))))

    (is (not (sorted? (:a (coll/path-map->nested-map {[:a :A] 1})))))
    (is (sorted? (:a (coll/path-map->nested-map (sorted-map [:a :A] 1)))))))

(deftest flatten-xf-test
  (testing "flatten behavior"
    (are [test-coll] (= (flatten test-coll) (into [] coll/flatten-xf test-coll))
      nil
      []
      [[[]] [[[]]] () () []]
      [1 [3 [4 [3 [2 [22 [4]] 6] 6] 6] 54 [3]]]))
  (testing "difference with clojure.core/flatten in nil handling"
    (is (= [1 2 nil] (flatten [[1 [2 [nil]]]])))
    (is (= [1 2] (into [] coll/flatten-xf [[1 [2 [nil]]]]))))
  (testing "reduced support"
    (is (= :stop
           (transduce
             (comp
               coll/flatten-xf
               (halt-when #{:stop}))
             conj
             []
             [(range 100)
              [[]]
              [[:stop
                (repeatedly #(throw (Exception. "Should not be reduced!")))]]])))))

(deftest some-test
  (testing "some behavior"
    (are [pred coll ret] (= ret (some pred coll) (coll/some pred coll))
      #{100} (range 1000) 100
      #(= % 100) (range 1000) true
      #(= % 100) (range 50) nil
      #(= % 100) [] nil)))
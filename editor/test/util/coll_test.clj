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

(ns util.coll-test
  (:require [clojure.core :as core]
            [clojure.test :refer :all]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.fn :as fn])
  (:import [clojure.lang IPersistentVector PersistentArrayMap PersistentHashMap PersistentHashSet PersistentTreeMap PersistentTreeSet]
           [java.util Hashtable]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record Nothing [])
(defonce/record JustA [a])
(defonce/record PairAB [a b])

(defn- java-map
  ^Hashtable [& key-vals]
  {:pre [(even? (count key-vals))]}
  (let [coll (Hashtable.)]
    (doseq [[key value] (partition-all 2 key-vals)]
      (.put coll key value))
    coll))

(deftest transform-test
  (testing "Empty collection."
    (let [colls [nil
                 ""
                 []
                 (vector-of :long)
                 '()
                 {}
                 #{}
                 (sorted-map)
                 (sorted-set)
                 (double-array 0)
                 (object-array 0)
                 (range 0)
                 (repeatedly 0 (constantly 1))
                 (Nothing.)]]

      (testing "No transducer."
        (doseq [coll colls]
          (testing (or (some-> coll class .getSimpleName) "nil")
            (let [transformed-coll (coll/transform coll)]
              (is (identical? coll transformed-coll))))))

      (testing "Single transducer."
        (doseq [coll colls]
          (testing (or (some-> coll class .getSimpleName) "nil")
            (let [transformed-coll (coll/transform coll
                                     (take 1))]
              (is (identical? coll transformed-coll))))))

      (testing "Multiple transducers."
        (doseq [coll colls]
          (testing (or (some-> coll class .getSimpleName) "nil")
            (let [transformed-coll (coll/transform coll
                                     (take 1)
                                     (mapcat (juxt identity identity identity))
                                     (drop 2))]
              (is (identical? coll transformed-coll))))))))

  (testing "Non-empty sequence."
    (let [colls (mapv #(with-meta % {:version "original"})
                      [[1]
                       (vector-of :long 1)
                       '(1)
                       #{1}
                       (sorted-set 1)])]

      (testing "No transducer."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll)]
              (is (identical? coll transformed-coll))))))

      (testing "Single transducer."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll
                                     (take 1))]
              (is (= (class coll) (class transformed-coll)))
              (is (= 1 (bounded-count 2 transformed-coll)))
              (is (= (first coll) (first transformed-coll)))
              (is (identical? (meta coll) (meta transformed-coll)))))))

      (testing "Multiple transducers."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll
                                     (take 1)
                                     (mapcat (juxt identity identity identity))
                                     (drop 2))]
              (is (= 1 (bounded-count 2 transformed-coll)))
              (is (= (first coll) (first transformed-coll)))
              (is (identical? (meta coll) (meta transformed-coll)))))))))

  (testing "Non-empty map."
    (let [colls (mapv #(with-meta % {:version "original"})
                      [{:a 1}
                       (sorted-map :a 1)
                       (JustA. 1)])]

      (testing "No transducer."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll)]
              (is (identical? coll transformed-coll))))))

      (testing "Single transducer."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll
                                     (map (fn [entry]
                                            [(key entry)
                                             (inc (long (val entry)))])))]
              (is (= (class coll) (class transformed-coll)))
              (is (= (map inc (vals coll)) (vals transformed-coll)))
              (is (identical? (meta coll) (meta transformed-coll)))))))

      (testing "Multiple transducers."
        (doseq [coll colls]
          (testing (.getSimpleName (class coll))
            (let [transformed-coll (coll/transform coll
                                     (take 1)
                                     (mapcat (juxt identity identity identity))
                                     (map (fn [entry]
                                            [(key entry)
                                             (inc (long (val entry)))]))
                                     (drop 2))]
              (is (= 1 (bounded-count 2 transformed-coll)))
              (is (= (map inc (vals coll)) (vals transformed-coll)))
              (is (identical? (meta coll) (meta transformed-coll))))))))))

(deftest key-set-test
  (letfn [(check! [expected actual]
            (is (set? actual))
            (is (not (sorted? actual)))
            (is (= expected actual)))]
    (check! #{} (coll/key-set nil))
    (check! #{:a} (coll/key-set {:a 1}))
    (check! #{:a :b} (coll/key-set (sorted-map :a 1 :b 2)))
    (check! #{:a :b :c} (coll/key-set (java-map :a 1 :b 2 :c 3)))))

(deftest sorted-key-set-test
  (letfn [(check! [expected actual]
            (is (set? actual))
            (is (sorted? actual))
            (is (= expected actual)))]
    (check! (sorted-set) (coll/sorted-key-set nil))
    (check! (sorted-set :a) (coll/sorted-key-set {:a 1}))
    (check! (sorted-set :a :b) (coll/sorted-key-set (sorted-map :a 1 :b 2)))
    (check! (sorted-set :a :b :c) (coll/sorted-key-set (java-map :a 1 :b 2 :c 3)))))

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
  (is (true? (coll/empty? (double-array 0))))
  (is (true? (coll/empty? (object-array 0))))
  (is (true? (coll/empty? (range 0))))
  (is (true? (coll/empty? (repeatedly 0 rand))))
  (is (true? (coll/empty? (Nothing.))))
  (is (false? (coll/empty? "a")))
  (is (false? (coll/empty? [1])))
  (is (false? (coll/empty? (vector-of :long 1))))
  (is (false? (coll/empty? '(1))))
  (is (false? (coll/empty? {:a 1})))
  (is (false? (coll/empty? #{1})))
  (is (false? (coll/empty? (sorted-map :a 1))))
  (is (false? (coll/empty? (sorted-set 1))))
  (is (false? (coll/empty? (double-array 1))))
  (is (false? (coll/empty? (object-array 1))))
  (is (false? (coll/empty? (range 1))))
  (is (false? (coll/empty? (repeatedly 1 rand))))
  (is (false? (coll/empty? (JustA. 1)))))

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
  (is (nil? (coll/not-empty (double-array 0))))
  (is (nil? (coll/not-empty (object-array 0))))
  (is (nil? (coll/not-empty (range 0))))
  (is (nil? (coll/not-empty (repeatedly 0 rand))))
  (is (nil? (coll/not-empty (Nothing.))))
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
    (is (returns-input? (double-array 1)))
    (is (returns-input? (object-array 1)))
    (is (returns-input? (range 1)))
    (is (returns-input? (repeatedly 1 rand)))
    (is (returns-input? (JustA. 1)))))

(deftest lazy?-test
  (is (false? (coll/lazy? nil)))
  (is (false? (coll/lazy? "")))
  (is (false? (coll/lazy? [])))
  (is (false? (coll/lazy? (vector-of :long))))
  (is (false? (coll/lazy? '())))
  (is (false? (coll/lazy? {})))
  (is (false? (coll/lazy? #{})))
  (is (false? (coll/lazy? (sorted-map))))
  (is (false? (coll/lazy? (sorted-set))))
  (is (false? (coll/lazy? (double-array 0))))
  (is (false? (coll/lazy? (object-array 0))))
  (is (false? (coll/lazy? (range 1))))
  (is (true? (coll/lazy? (repeat 1))))
  (is (true? (coll/lazy? (repeatedly 1 rand))))
  (is (true? (coll/lazy? (cycle [1]))))
  (is (true? (coll/lazy? (map identity [1]))))
  (is (true? (coll/lazy? (eduction (map identity) [1])))))

(deftest eager-seqable?-test
  (is (true? (coll/eager-seqable? nil)))
  (is (true? (coll/eager-seqable? "")))
  (is (true? (coll/eager-seqable? [])))
  (is (true? (coll/eager-seqable? (vector-of :long))))
  (is (true? (coll/eager-seqable? '())))
  (is (true? (coll/eager-seqable? {})))
  (is (true? (coll/eager-seqable? #{})))
  (is (true? (coll/eager-seqable? (sorted-map))))
  (is (true? (coll/eager-seqable? (sorted-set))))
  (is (true? (coll/eager-seqable? (double-array 0))))
  (is (true? (coll/eager-seqable? (object-array 0))))
  (is (true? (coll/eager-seqable? (range 1))))
  (is (false? (coll/eager-seqable? (repeat 1))))
  (is (false? (coll/eager-seqable? (repeatedly 1 rand))))
  (is (false? (coll/eager-seqable? (cycle [1]))))
  (is (false? (coll/eager-seqable? (map identity [1]))))
  (is (false? (coll/eager-seqable? (eduction (map identity) [1]))))
  (is (false? (coll/eager-seqable? (map identity [1]))))
  (is (false? (coll/eager-seqable? (eduction (map identity) [1]))))
  (is (false? (coll/eager-seqable? 0)))
  (is (false? (coll/eager-seqable? (Object.))))
  (is (false? (coll/eager-seqable? (Object.)))))

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

(deftest reduce-partitioned-test
  (testing "Applies accumulate-fn on arguments partitioned from the input sequence."
    (is (= [[[[[[[] 0] 1] 2] 3] 4] 5] (coll/reduce-partitioned 1 vector [] (range 6))))
    (is (= [[[[] 0 1] 2 3] 4 5] (coll/reduce-partitioned 2 vector [] (range 6))))
    (is (= [[[] 0 1 2] 3 4 5] (coll/reduce-partitioned 3 vector [] (range 6))))
    (is (= {:a 1
            :b 2
            :c 3}
           (coll/reduce-partitioned 2 assoc {:a 1} [:b 2 :c 3]))))

  (testing "Throws when the input sequence cannot be evenly partitioned."
    (is (thrown-with-msg?
          IllegalArgumentException
          #"The length of coll must be a multiple of the partition-length."
          (coll/reduce-partitioned 2 vector [] (range 5)))))

  (testing "Throws when partition-length is not a positive number."
    (doseq [partition-length [-1 0]]
      (is (thrown-with-msg?
            IllegalArgumentException
            #"The partition-length must be positive."
            (coll/reduce-partitioned partition-length vector [] (range 6)))))))

(deftest index-of-test
  (is (= 0 (coll/index-of [:a :b :a :b] :a)))
  (is (= 1 (coll/index-of [:a :b :a :b] :b)))
  (is (= -1 (coll/index-of [:a :b :a :b] :c)))
  (is (= [-1 1 0] (map #(coll/index-of [:a :b :a :b] %) [:c :b :a]))))

(deftest last-index-of-test
  (is (= 2 (coll/last-index-of [:a :b :a :b] :a)))
  (is (= 3 (coll/last-index-of [:a :b :a :b] :b)))
  (is (= -1 (coll/last-index-of [:a :b :a :b] :c)))
  (is (= [-1 3 2] (map #(coll/last-index-of [:a :b :a :b] %) [:c :b :a]))))

(deftest remove-index-test
  (testing "Returns a vector without the item at the specified index."
    (is (= [:b :c] (coll/remove-index [:a :b :c] 0)))
    (is (= [:a :c] (coll/remove-index [:a :b :c] 1)))
    (is (= [:a :b] (coll/remove-index [:a :b :c] 2))))

  (testing "Throws when index out of bounds."
    (is (thrown? IndexOutOfBoundsException (coll/remove-index [:a :b :c] -1)))
    (is (thrown? IndexOutOfBoundsException (coll/remove-index [:a :b :c] 3))))

  (testing "Preserves metadata."
    (let [original-meta {:meta-key "meta-value"}]
      (doseq [checked-coll [[:a :b :c]
                            (subvec [:a :b :c] 1)
                            (vector-of :long 10 20 30)
                            (vector-of :double 10.0 20.0 30.0)]]
        (let [original-coll (with-meta checked-coll original-meta)
              altered-coll (coll/remove-index original-coll 1)]
          (is (identical? original-meta (meta altered-coll))))))))

(deftest merge-test
  (testing "Replaces entries."
    (is (= {:a 11
            :m {:b 2}}
           (coll/merge {:a 1
                        :m {:a 1}}
                       {:a 11
                        :m {:b 2}})
           (core/merge {:a 1
                        :m {:a 1}}
                       {:a 11
                        :m {:b 2}}))))

  (testing "Adds entries."
    (is (= {:a 1
            :b 2
            :c 3}
           (coll/merge {:a 1}
                       {:b 2
                        :c 3})
           (core/merge {:a 1}
                       {:b 2
                        :c 3})))
    (is (= #{:a :b :c}
           (coll/merge #{:a}
                       #{:b :c}))))

  (testing "Multiple collections."
    (is (= {:a 1
            :b 2
            :c 3}
           (coll/merge {:a 1}
                       {:b 2}
                       {:c 3})
           (core/merge {:a 1}
                       {:b 2}
                       {:c 3})))
    (is (= #{:a :b :c}
           (coll/merge #{:a}
                       #{:b}
                       #{:c}))))

  (testing "Nil values."
    (is (= {:a nil
            :b nil
            :c nil}
           (coll/merge {:a 1
                        :b nil}
                       {:a nil}
                       {:c nil})
           (core/merge {:a 1
                        :b nil}
                       {:a nil}
                       {:c nil}))))

  (testing "Vector values."
    (is (= {:a [1]
            :b [2]
            :c [3]}
           (coll/merge {:a [nil nil]
                        :b [2]}
                       {:a [1]}
                       {:c [3]})
           (core/merge {:a [nil nil]
                        :b [2]}
                       {:a [1]}
                       {:c [3]}))))

  (testing "Maps in RHS position merge into records in LHS position."
    (is (= (PairAB. 1 22)
           (coll/merge (PairAB. 1 2)
                       {:b 22})
           (core/merge (PairAB. 1 2)
                       {:b 22})))
    (is (instance? PairAB
                   (coll/merge (PairAB. 1 2)
                               {:b 22}))))

  (testing "Records in RHS position merge into maps in LHS position."
    (is (= {:a 11 :b 22 :c 3}
           (coll/merge {:a 1 :b 2 :c 3}
                       (PairAB. 11 22))
           (core/merge {:a 1 :b 2 :c 3}
                       (PairAB. 11 22)))))

  (testing "Returns nil when called with no maps."
    (is (nil? (coll/merge)))
    (is (nil? (core/merge))))

  (testing "Returns original when there is nothing to merge."
    (let [original-map {:a 1
                        :m {:a 1
                            :m {:a 1}}}]
      (is (identical? original-map (coll/merge original-map)))
      (is (identical? original-map (coll/merge original-map nil)))
      (is (identical? original-map (coll/merge original-map {})))
      (is (identical? original-map (coll/merge nil original-map)))
      (is (identical? original-map (coll/merge {} original-map))))
    (let [original-set #{:a}]
      (is (identical? original-set (coll/merge original-set)))
      (is (identical? original-set (coll/merge original-set nil)))
      (is (identical? original-set (coll/merge original-set #{})))
      (is (identical? original-set (coll/merge nil original-set)))
      (is (identical? original-set (coll/merge #{} original-set)))))

  (testing "Preserves metadata."
    (let [original-meta {:meta-key "meta-value"}]
      (doseq [map-fn [array-map hash-map sorted-map]]
        (let [original-map (with-meta (map-fn :a 1) original-meta)
              merged-map (coll/merge original-map {:a 2})]
          (is (= {:a 2} merged-map))
          (is (identical? original-meta (meta merged-map)))))
      (doseq [set-fn [hash-set sorted-set]]
        (let [original-set (with-meta (set-fn :a) original-meta)
              merged-set (coll/merge original-set #{:b})]
          (is (= #{:a :b} merged-set))
          (is (identical? original-meta (meta merged-set)))))))

  (testing "Returns same type as first non-empty collection."
    (let [result (coll/merge nil nil (hash-map) nil nil (sorted-map) nil nil (array-map :a 1) {:b 2})]
      (is (= (array-map :a 1 :b 2) result))
      (is (instance? PersistentArrayMap result)))
    (let [result (coll/merge nil nil (array-map) nil nil (sorted-map) nil nil (hash-map :a 1) {:b 2})]
      (is (= (hash-map :a 1 :b 2) result))
      (is (instance? PersistentHashMap result)))
    (let [result (coll/merge nil nil (array-map) nil nil (hash-map) nil nil (sorted-map :a 1) {:b 2})]
      (is (= (sorted-map :a 1 :b 2) result))
      (is (instance? PersistentTreeMap result)))
    (let [result (coll/merge nil nil (sorted-set) nil nil (sorted-set) nil nil (hash-set :a) #{:b})]
      (is (= (hash-set :a :b) result))
      (instance? PersistentHashSet result))
    (let [result (coll/merge nil nil (hash-set) nil nil (hash-set) nil nil (sorted-set :a) #{:b})]
      (is (= (sorted-set :a :b) result))
      (is (instance? PersistentTreeSet result)))))

(deftest merge-with-test
  (testing "Merges conflicting values using supplied function."
    (is (= {:a 11
            :b 22}
           (coll/merge-with +
                            {:a 1
                             :b 2}
                            {:a 10
                             :b 20})
           (core/merge-with +
                            {:a 1
                             :b 2}
                            {:a 10
                             :b 20}))))

  (testing "Adds entries."
    (is (= {:a 1
            :b 2
            :c 3}
           (coll/merge-with +
                            {:a 1}
                            {:b 2
                             :c 3})
           (core/merge-with +
                            {:a 1}
                            {:b 2
                             :c 3}))))

  (testing "Multiple collections."
    (is (= {:a 11
            :b 22
            :c 33}
           (coll/merge-with +
                            {:a 1}
                            {:b 2}
                            {:c 3}
                            {:a 10}
                            {:b 20
                             :c 30})
           (core/merge-with +
                            {:a 1}
                            {:b 2}
                            {:c 3}
                            {:a 10}
                            {:b 20
                             :c 30}))))

  (testing "Nil values."
    (is (= {:a nil
            :b nil
            :c nil}
           (coll/merge-with (fn [_a b] b)
                            {:a 1
                             :b nil}
                            {:a nil}
                            {:c nil})
           (core/merge-with (fn [_a b] b)
                            {:a 1
                             :b nil}
                            {:a nil}
                            {:c nil}))))

  (testing "Vector values."
    (is (= {:a [1]
            :b [2]
            :c [3]}
           (coll/merge-with (fn [_a b] b)
                            {:a [nil nil]
                             :b [2]}
                            {:a [1]}
                            {:c [3]})
           (core/merge-with (fn [_a b] b)
                            {:a [nil nil]
                             :b [2]}
                            {:a [1]}
                            {:c [3]}))))

  (testing "Maps in RHS position merge into records in LHS position."
    (is (= (PairAB. 1 22)
           (coll/merge-with +
                            (PairAB. 1 2)
                            {:b 20})
           (core/merge-with +
                            (PairAB. 1 2)
                            {:b 20})))
    (is (instance? PairAB
                   (coll/merge-with +
                                    (PairAB. 1 2)
                                    {:b 20}))))

  (testing "Records in RHS position merge into maps in LHS position."
    (is (= {:a 11 :b 22 :c 3}
           (coll/merge-with +
                            {:a 1 :b 2 :c 3}
                            (PairAB. 10 20))
           (core/merge-with +
                            {:a 1 :b 2 :c 3}
                            (PairAB. 10 20)))))

  (testing "Returns nil when called with no maps."
    (is (nil? (coll/merge-with +)))
    (is (nil? (core/merge-with +))))

  (testing "Returns original when there is nothing to merge."
    (let [original-map {:a 1
                        :m {:a 1
                            :m {:a 1}}}]
      (is (identical? original-map (coll/merge-with + original-map)))
      (is (identical? original-map (coll/merge-with + original-map nil)))
      (is (identical? original-map (coll/merge-with + original-map {})))
      (is (identical? original-map (coll/merge-with + nil original-map)))
      (is (identical? original-map (coll/merge-with + {} original-map)))))

  (testing "Preserves metadata."
    (let [original-meta {:meta-key "meta-value"}]
      (doseq [map-fn [array-map hash-map sorted-map]]
        (let [original-map (with-meta (map-fn :a 1) original-meta)
              merged-map (coll/merge-with + original-map {:a 2})]
          (is (= {:a 3} merged-map))
          (is (identical? original-meta (meta merged-map)))))))

  (testing "Returns same type as first non-empty collection."
    (let [result (coll/merge-with + nil nil (hash-map) nil nil (sorted-map) nil nil (array-map :a 1) {:b 2})]
      (is (= (array-map :a 1 :b 2) result))
      (is (instance? PersistentArrayMap result)))
    (let [result (coll/merge-with + nil nil (array-map) nil nil (sorted-map) nil nil (hash-map :a 1) {:b 2})]
      (is (= (hash-map :a 1 :b 2) result))
      (is (instance? PersistentHashMap result)))
    (let [result (coll/merge-with + nil nil (array-map) nil nil (hash-map) nil nil (sorted-map :a 1) {:b 2})]
      (is (= (sorted-map :a 1 :b 2) result))
      (is (instance? PersistentTreeMap result)))))

(deftest merge-with-kv-test
  (letfn [(throwing-merge-fn [key a b]
            (throw (ex-info (str "Unexpected conflict for key: " key)
                            {:key key
                             :value a
                             :conflicting-value b})))
          (make-sum-merge-fn [& allowed-conflicting-keys]
            (let [allowed-conflicting-key-set (set allowed-conflicting-keys)]
              (fn sum-merge-fn [key a b]
                (if (contains? allowed-conflicting-key-set key)
                  (+ (long a) (long b))
                  (throwing-merge-fn key a b)))))
          (make-return-b-merge-fn [& allowed-conflicting-keys]
            (let [allowed-conflicting-key-set (set allowed-conflicting-keys)]
              (fn return-b-merge-fn [key a b]
                (if (contains? allowed-conflicting-key-set key)
                  b
                  (throwing-merge-fn key a b)))))]
    (testing "Merges conflicting values using supplied function."
      (is (= {:a 11
              :b 22}
             (coll/merge-with-kv
               (make-sum-merge-fn :a :b)
               {:a 1
                :b 2}
               {:a 10
                :b 20}))))

    (testing "Adds entries."
      (is (= {:a 1
              :b 2
              :c 3}
             (coll/merge-with-kv
               throwing-merge-fn
               {:a 1}
               {:b 2
                :c 3}))))

    (testing "Multiple collections."
      (is (= {:a 11
              :b 22
              :c 33}
             (coll/merge-with-kv
               (make-sum-merge-fn :a :b :c)
               {:a 1}
               {:b 2}
               {:c 3}
               {:a 10}
               {:b 20
                :c 30}))))

    (testing "Nil values."
      (is (= {:a nil
              :b nil
              :c nil}
             (coll/merge-with-kv
               (make-return-b-merge-fn :a)
               {:a 1
                :b nil}
               {:a nil}
               {:c nil}))))

    (testing "Vector values."
      (is (= {:a [1]
              :b [2]
              :c [3]}
             (coll/merge-with-kv
               (make-return-b-merge-fn :a)
               {:a [nil nil]
                :b [2]}
               {:a [1]}
               {:c [3]}))))

    (testing "Maps in RHS position merge into records in LHS position."
      (let [merged-record (coll/merge-with-kv
                            (make-sum-merge-fn :b)
                            (PairAB. 1 2)
                            {:b 20})]
        (is (= (PairAB. 1 22) merged-record))
        (is (instance? PairAB merged-record))))

    (testing "Records in RHS position merge into maps in LHS position."
      (is (= {:a 11 :b 22 :c 3}
             (coll/merge-with-kv
               (make-sum-merge-fn :a :b)
               {:a 1 :b 2 :c 3}
               (PairAB. 10 20)))))

    (testing "Returns nil when called with no maps."
      (is (nil? (coll/merge-with-kv throwing-merge-fn))))

    (testing "Returns original when there is nothing to merge."
      (let [original-map {:a 1
                          :m {:a 1
                              :m {:a 1}}}]
        (is (identical? original-map (coll/merge-with-kv throwing-merge-fn original-map)))
        (is (identical? original-map (coll/merge-with-kv throwing-merge-fn original-map nil)))
        (is (identical? original-map (coll/merge-with-kv throwing-merge-fn original-map {})))
        (is (identical? original-map (coll/merge-with-kv throwing-merge-fn nil original-map)))
        (is (identical? original-map (coll/merge-with-kv throwing-merge-fn {} original-map)))))

    (testing "Preserves metadata."
      (let [original-meta {:meta-key "meta-value"}]
        (doseq [map-fn [array-map hash-map sorted-map]]
          (let [original-map (with-meta (map-fn :a 1) original-meta)
                merged-map (coll/merge-with-kv
                             (make-sum-merge-fn :a)
                             original-map
                             {:a 2})]
            (is (= {:a 3} merged-map))
            (is (identical? original-meta (meta merged-map)))))))

    (testing "Returns same type as first non-empty collection."
      (let [result (coll/merge-with-kv throwing-merge-fn nil nil (hash-map) nil nil (sorted-map) nil nil (array-map :a 1) {:b 2})]
        (is (= (array-map :a 1 :b 2) result))
        (is (instance? PersistentArrayMap result)))
      (let [result (coll/merge-with-kv throwing-merge-fn nil nil (array-map) nil nil (sorted-map) nil nil (hash-map :a 1) {:b 2})]
        (is (= (hash-map :a 1 :b 2) result))
        (is (instance? PersistentHashMap result)))
      (let [result (coll/merge-with-kv throwing-merge-fn nil nil (array-map) nil nil (hash-map) nil nil (sorted-map :a 1) {:b 2})]
        (is (= (sorted-map :a 1 :b 2) result))
        (is (instance? PersistentTreeMap result))))))

(deftest deep-merge-test
  (testing "Replaces entries."
    (is (= {:a 11
            :m {:a 11
                :m {:a 11}}}
           (coll/deep-merge {:a 1
                             :m {:a 1
                                 :m {:a 1}}}
                            {:a 11
                             :m {:a 11
                                 :m {:a 11}}}))))

  (testing "Adds entries."
    (is (= {:a 1
            :b 2
            :m {:a 1
                :b 2
                :m {:a 1
                    :b 2}}}
           (coll/deep-merge {:a 1
                             :m {:a 1
                                 :m {:a 1}}}
                            {:b 2
                             :m {:b 2
                                 :m {:b 2}}}))))

  (testing "Multiple maps."
    (is (= {:a 1
            :b 2
            :c 3
            :m {:a 1
                :b 2
                :c 3
                :m {:a 1
                    :b 2
                    :c 3}}}
           (coll/deep-merge {:a 1
                             :m {:a 1
                                 :m {:a 1}}}
                            {:b 2
                             :m {:b 2
                                 :m {:b 2}}}
                            {:c 3
                             :m {:c 3
                                 :m {:c 3}}}))))

  (testing "Nil values."
    (is (= {:a nil
            :b nil
            :c nil
            :m {:a nil
                :b nil
                :c nil
                :m {:a nil
                    :b nil
                    :c nil}}}
           (coll/deep-merge {:a 1
                             :b nil
                             :m {:a 1
                                 :b nil
                                 :m {:a 1
                                     :b nil}}}
                            {:a nil
                             :m {:a nil
                                 :m {:a nil}}}
                            {:c nil
                             :m {:c nil
                                 :m {:c nil}}}))))

  (testing "Vector values."
    (is (= {:a [1]
            :b [2]
            :c [3]
            :m {:a [1]
                :b [2]
                :c [3]
                :m {:a [1]
                    :b [2]
                    :c [3]}}}
           (coll/deep-merge {:a [nil nil]
                             :b [2]
                             :m {:a [nil nil]
                                 :b [2]
                                 :m {:a [nil nil]
                                     :b [2]}}}
                            {:a [1]
                             :m {:a [1]
                                 :m {:a [1]}}}
                            {:c [3]
                             :m {:c [3]
                                 :m {:c [3]}}}))))

  (testing "Maps in RHS position merge into records in LHS position."
    (is (= (PairAB. 1 22)
           (coll/deep-merge (PairAB. 1 2)
                            {:b 22})))
    (is (instance? PairAB
                   (coll/deep-merge (PairAB. 1 2)
                                    {:b 22})))
    (is (= {:a (PairAB. 1 22)}
           (coll/deep-merge {:a (PairAB. 1 2)}
                            {:a {:b 22}})))
    (is (instance? PairAB
                   (:a (coll/deep-merge {:a (PairAB. 1 2)}
                                        {:a {:b 22}})))))

  (testing "Records in RHS position merge into maps in LHS position."
    (is (= {:a 11 :b 22 :c 3}
           (coll/deep-merge {:a 1 :b 2 :c 3}
                            (PairAB. 11 22))))
    (is (= {:m {:a 11 :b 22 :c 3}}
           (coll/deep-merge {:m {:a 1 :b 2 :c 3}}
                            {:m (PairAB. 11 22)}))))

  (testing "Records in RHS position replace records in LHS position."
    (is (= (JustA. 11)
           (coll/deep-merge (PairAB. 1 2)
                            (JustA. 11))))
    (is (= {:m (JustA. 11)}
           (coll/deep-merge {:m (PairAB. 1 2)}
                            {:m (JustA. 11)}))))

  (testing "Returns nil when called with no maps."
    (is (nil? (coll/deep-merge))))

  (testing "Returns original when there is nothing to merge."
    (let [original-map {:a 1
                        :m {:a 1
                            :m {:a 1}}}]
      (is (identical? original-map (coll/deep-merge original-map)))
      (is (identical? original-map (coll/deep-merge original-map nil)))
      (is (identical? original-map (coll/deep-merge original-map {})))
      (is (identical? original-map (coll/deep-merge nil original-map)))
      (is (identical? original-map (coll/deep-merge {} original-map)))))

  (testing "Preserves metadata."
    (let [original-meta {:meta-key "meta-value"}]
      (doseq [map-fn [array-map hash-map sorted-map]]
        (let [original-map (with-meta (map-fn :a 1
                                              :m (with-meta (map-fn :a 11)
                                                            original-meta))
                                      original-meta)
              merged-map (coll/deep-merge original-map
                                          {:a 2 :m {:a 22}})]
          (is (= {:a 2
                  :m {:a 22}}
                 merged-map))
          (is (identical? original-meta (meta merged-map)))
          (is (identical? original-meta (meta (:m merged-map)))))))))

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
  (testing "Aggregates pairs into the specified associative."
    (is (= {\a 5
            \b 7
            \c 0}
           (coll/aggregate-into {\c 0}
                                +
                                (map (juxt first count)
                                     ["apple" "book" "box"])))))

  (testing "Uses supplied init values as the starting point."
    (is (= {\a #{:init :a1 :a2}
            \b #{:init :b1}
            \c #{:c1}}
           (coll/aggregate-into {\c #{:c1}}
                                conj
                                #{:init}
                                (map (juxt first keyword)
                                     ["a1" "b1" "a2"])))))

  (testing "Uses values produced by supplied init function as the starting point."
    (let [init-fn (fn/make-call-logger
                    (fn init-fn [unseen-key]
                      (is (contains? #{\a \b} unseen-key))
                      (transient [])))]
      (is (= {\a [:a1 :a2]
              \b [:b1]}
             (into {}
                   (map (fn [[key value]]
                          [key (persistent! value)]))
                   (coll/aggregate-into {}
                                        conj!
                                        init-fn
                                        (map (juxt first keyword)
                                             ["a1" "b1" "a2"])))))
      (is (= [[\a]
              [\b]]
             (fn/call-logger-calls init-fn)))))

  (testing "Does nothing and returns target coll when there are no pairs."
    (doseq [empty-pairs [nil '() [] #{} {} (sorted-set) (sorted-map) (vector-of :long)]]
      (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
        (let [result (coll/aggregate-into target-coll
                                          (fn accumulate-fn [_ _] (assert false))
                                          (fn init-fn [_] (assert false))
                                          empty-pairs)]
          (is (identical? target-coll result))))))

  (testing "Preserves metadata."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta target-coll original-meta)
            altered-map (coll/aggregate-into original-map
                                             conj
                                             (map (juxt first keyword)
                                                  ["a1" "b1" "a2"]))]
        (is (identical? original-meta (meta altered-map)))))))

(deftest update-vals-test
  (testing "Over nil."
    (is (nil? (coll/update-vals nil (fn [] :uncalled)))))

  (testing "Over collection."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta (into target-coll
                                          {:a 1
                                           :b 2})
                                    original-meta)
            altered-map (coll/update-vals original-map inc)]
        (is (= {:a 2 :b 3} altered-map))
        (is (identical? original-meta (meta altered-map))))))

  (testing "Over record."
    (let [original-meta {:meta-key "meta-value"}
          original-record (with-meta (->PairAB 1 2) original-meta)
          altered-record (coll/update-vals original-record inc)]
      (is (instance? PairAB altered-record))
      (is (= (->PairAB 2 3) altered-record))
      (is (identical? original-meta (meta altered-record)))))

  (testing "Supplies additional arguments to f."
    (let [original-map {:a 1 :b 2}
          altered-map (coll/update-vals original-map vector :arg1 :arg2)]
      (is (= {:a [1 :arg1 :arg2]
              :b [2 :arg1 :arg2]}
             altered-map)))))

(deftest update-vals-kv-test
  (testing "Over nil."
    (is (nil? (coll/update-vals-kv nil (fn [] :uncalled)))))

  (testing "Over collection."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta (into target-coll
                                          {:a 1
                                           :b 2})
                                    original-meta)
            altered-map (coll/update-vals-kv original-map
                                             (fn [k ^long v]
                                               (case k
                                                 :b (+ 10 v)
                                                 v)))]
        (is (= {:a 1 :b 12} altered-map))
        (is (identical? original-meta (meta altered-map))))))

  (testing "Over record."
    (let [original-meta {:meta-key "meta-value"}
          original-record (with-meta (->PairAB 1 2) original-meta)
          altered-record (coll/update-vals-kv original-record
                                              (fn [k ^long v]
                                                (case k
                                                  :b (+ 10 v)
                                                  v)))]
      (is (instance? PairAB altered-record))
      (is (= (->PairAB 1 12) altered-record))
      (is (identical? original-meta (meta altered-record)))))

  (testing "Supplies additional arguments to f."
    (let [original-map {:a 1 :b 2}
          altered-map (coll/update-vals-kv original-map vector :arg1 :arg2)]
      (is (= {:a [:a 1 :arg1 :arg2]
              :b [:b 2 :arg1 :arg2]}
             altered-map)))))

(deftest map-vals-test
  (testing "Over nil."
    (is (nil? (coll/map-vals (fn [] :uncalled) nil))))

  (testing "Over collection."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta (into target-coll
                                          {:a 1
                                           :b 2})
                                    original-meta)
            altered-map (coll/map-vals inc original-map)]
        (is (= {:a 2 :b 3} altered-map))
        (is (identical? original-meta (meta altered-map))))))

  (testing "Over record."
    (let [original-meta {:meta-key "meta-value"}
          original-record (with-meta (->PairAB 1 2) original-meta)
          altered-record (coll/map-vals inc original-record)]
      (is (instance? PairAB altered-record))
      (is (= (->PairAB 2 3) altered-record))
      (is (identical? original-meta (meta altered-record)))))

  (testing "As transducer."
    (is (= {:a 0
            :b 1}
           (into {}
                 (coll/map-vals dec)
                 {:a 1
                  :b 2})))))

(deftest map-vals-kv-test
  (testing "Over nil."
    (is (nil? (coll/map-vals-kv (fn [] :uncalled) nil))))

  (testing "Over collection."
    (doseq [target-coll [(array-map) (hash-map) (sorted-map)]]
      (let [original-meta {:meta-key "meta-value"}
            original-map (with-meta (into target-coll
                                          {:a 1
                                           :b 2})
                                    original-meta)
            altered-map (coll/map-vals-kv (fn [k ^long v]
                                            (case k
                                              :b (+ 10 v)
                                              v))
                                          original-map)]
        (is (= {:a 1 :b 12} altered-map))
        (is (identical? original-meta (meta altered-map))))))

  (testing "Over record."
    (let [original-meta {:meta-key "meta-value"}
          original-record (with-meta (->PairAB 1 2) original-meta)
          altered-record (coll/map-vals-kv (fn [k ^long v]
                                             (case k
                                               :b (+ 10 v)
                                               v))
                                           original-record)]
      (is (instance? PairAB altered-record))
      (is (= (->PairAB 1 12) altered-record))
      (is (identical? original-meta (meta altered-record)))))

  (testing "As transducer."
    (is (= {:a 1
            :b 12}
           (into {}
                 (coll/map-vals-kv (fn [k ^long v]
                                     (case k
                                       :b (+ 10 v)
                                       v)))
                 {:a 1
                  :b 2})))))

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

(deftest tree-xf-test
  (testing "tree behavior"
    (are [ids root] (and (= (tree-seq :children :children root)
                            (into [] (coll/tree-xf :children :children) [root]))
                         (= ids (mapv :id (tree-seq :children :children root))))
      (range 1) {:id 0}
      (range 1) {:id 0 :children []}
      (range 3) {:id 0 :children [{:id 1 :children nil}
                                  {:id 2 :children []}]}
      (range 4) {:id 0 :children [{:id 1 :children nil}
                                  {:id 2 :children [{:id 3}]}]}))
  (testing "reduced support"
    (is (= :stop
           (transduce
             (comp
               (coll/tree-xf :children :children)
               (halt-when #(= :stop %)))
             conj
             []
             [{:children [{:children []}
                          :stop
                          {:children (repeatedly #(throw (Exception. "Should not be reduced!")))}]}])))))

(deftest find-values-test
  (testing "Empty collections."
    (doseq [coll [nil
                  ""
                  []
                  (vector-of :long)
                  '()
                  {}
                  #{}
                  (sorted-map)
                  (sorted-set)
                  (double-array 0)
                  (object-array 0)
                  (range 0)
                  (repeatedly 0 rand)
                  (Nothing.)]]
      (is (= []
             (coll/find-values
               #(= :wanted (:type %))
               coll)))))

  (testing "Traverses collections."
    (let [make-coll-fns
          [vector
           list
           #(array-map :key %)
           #(hash-map :key %)
           hash-set
           #(object-array [%])
           #(repeatedly 1 (constantly %))
           ->JustA]]

      (testing "Top-level."
        (doseq [make-coll make-coll-fns]
          (is (= [{:type :wanted}]
                 (coll/find-values
                   #(= :wanted (:type %))
                   (make-coll {:type :wanted}))))))

      (testing "Nested."
        (doseq [make-inner-coll make-coll-fns
                make-middle-coll make-coll-fns
                make-outer-coll make-coll-fns]
          (is (= [{:type :wanted}]
                 (coll/find-values
                   #(= :wanted (:type %))
                   (make-outer-coll
                     (make-middle-coll
                       (make-inner-coll {:type :wanted}))))))))))

  (testing "Match is returned as-is."
    (is (= [{:type :wanted
             :vec [{:type :wanted
                    :map {:a 1}
                    :vec [0]}]
             :map {:type :wanted
                   :map {:a 1}
                   :vec [0]}}]
           (coll/find-values
             #(= :wanted (:type %))
             [{:type :wanted
               :vec [{:type :wanted
                      :map {:a 1}
                      :vec [0]}]
               :map {:type :wanted
                     :map {:a 1}
                     :vec [0]}}]))))

  (testing "Over sequence."
    (is (= [{:type :wanted :index 0}
            {:type :wanted :index 1}
            {:type :wanted :index 2}]
           (coll/find-values
             #(= :wanted (:type %))
             [{:type :wanted :index 0}
              [{:type :wanted :index 1}]
              {:key {:type :wanted :index 2}}]))))


  (testing "As transducer."
    (is (= [{:type :wanted :index 0}
            {:type :wanted :index 1}
            {:type :wanted :index 2}]
           (into []
                 (coll/find-values #(= :wanted (:type %)))
                 [{:type :wanted :index 0}
                  [{:type :wanted :index 1}]
                  {:key {:type :wanted :index 2}}])))))

(deftest first-where-test
  (is (= 2 (coll/first-where even? (range 1 4))))
  (is (nil? (coll/first-where nil? [:a :b nil :d])))
  (is (= [:d 4] (coll/first-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= :e (coll/first-where #(= :e %) (list :a nil :c nil :e))))
  (is (= "f" (coll/first-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (coll/first-where nil? nil)))
  (is (nil? (coll/first-where even? nil)))
  (is (nil? (coll/first-where even? [])))
  (is (nil? (coll/first-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (fn/make-call-logger fn/constantly-true)]
      (is (= 0 (coll/first-where pred (range 10))))
      (is (= 1 (count (fn/call-logger-calls pred)))))))

(deftest first-index-where-test
  (is (= 1 (coll/first-index-where even? (range 1 4))))
  (is (= 2 (coll/first-index-where nil? [:a :b nil :d])))
  (is (= 3 (coll/first-index-where ^[char] Character/isDigit "abc123def")))
  (is (= 3 (coll/first-index-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (= 4 (coll/first-index-where #(= :e %) (list :a nil :c nil :e))))
  (is (= 5 (coll/first-index-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (coll/first-index-where nil? nil)))
  (is (nil? (coll/first-index-where even? nil)))
  (is (nil? (coll/first-index-where even? [])))
  (is (nil? (coll/first-index-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (fn/make-call-logger fn/constantly-true)]
      (is (= 0 (coll/first-index-where pred (range 10))))
      (is (= 1 (count (fn/call-logger-calls pred)))))))

(deftest some-test
  (testing "some behavior"
    (are [pred coll ret] (= ret (some pred coll) (coll/some pred coll))
      #{100} (range 1000) 100
      #(= % 100) (range 1000) true
      #(= % 100) (range 50) nil
      #(= % 100) [] nil)))

(deftest any?-test
  (testing "any? behavior"
    (are [pred coll ret] (= ret (boolean (some pred coll)) (coll/any? pred coll))
      even? nil false
      even? [] false
      even? [1 3 5] false
      even? [1 2 3 5] true
      odd? [2 4 6] false
      odd? [2 4 5 6] true)))

(deftest not-any?-test
  (testing "not-any? behavior"
    (are [pred coll ret] (= ret (not-any? pred coll) (coll/not-any? pred coll))
      even? nil true
      even? [] true
      even? [1 3 5] true
      even? [1 2 3 5] false
      odd? [2 4 6] true
      odd? [2 4 5 6] false)))

(deftest every?-test
  (testing "every? behavior"
    (are [pred coll ret] (= ret (every? pred coll) (coll/every? pred coll))
      odd? nil true
      odd? [] true
      odd? [1 3 5] true
      odd? [1 2 3 5] false
      even? [2 4 6] true
      even? [2 4 5 6] false)))

(deftest not-every?-test
  (testing "not-every? behavior"
    (are [pred coll ret] (= ret (not-every? pred coll) (coll/not-every? pred coll))
      odd? nil false
      odd? [] false
      odd? [1 3 5] false
      odd? [1 2 3 5] true
      even? [2 4 6] false
      even? [2 4 5 6] true)))

(deftest join-to-string-test
  (is (= "" (coll/join-to-string ", " [])))
  (is (= "" (coll/join-to-string [])))
  (is (= "12" (coll/join-to-string [1 nil 2])))
  (is (= "1,,2" (coll/join-to-string "," [1 nil 2])))
  (is (= "0, 1, 2, 3, 4" (coll/join-to-string ", " (range 5)))))

(deftest unanimous-value-test
  (is (nil? (coll/unanimous-value nil)))
  (is (= ::no-unanimous-value (coll/unanimous-value nil ::no-unanimous-value)))
  (is (nil? (coll/unanimous-value [])))
  (is (= ::no-unanimous-value (coll/unanimous-value [] ::no-unanimous-value)))
  (is (= ::one-value (coll/unanimous-value [::one-value])))
  (is (= ::one-value (coll/unanimous-value [::one-value] ::no-unanimous-value)))
  (is (= ::equal-value (coll/unanimous-value [::equal-value ::equal-value])))
  (is (= ::equal-value (coll/unanimous-value [::equal-value ::equal-value] ::no-unanimous-value)))
  (is (nil? (coll/unanimous-value [::one-value ::conflicting-value])))
  (is (= ::no-unanimous-value (coll/unanimous-value [::one-value ::conflicting-value] ::no-unanimous-value))))

(deftest primitive-vector-type-test
  (is (nil? (coll/primitive-vector-type nil)))
  (is (nil? (coll/primitive-vector-type 0)))
  (is (nil? (coll/primitive-vector-type [])))
  (is (nil? (coll/primitive-vector-type "")))
  (is (nil? (coll/primitive-vector-type (Object.))))
  (doseq [primitive-type [:boolean :char :byte :short :int :long :float :double]]
    (is (= primitive-type (coll/primitive-vector-type (vector-of primitive-type))))))

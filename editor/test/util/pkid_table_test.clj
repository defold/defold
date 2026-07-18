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

(ns util.pkid-table-test
  (:require [clojure.data.int-map :as int-map]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [util.coll :as coll]
            [util.pkid-table :as pkid-table])
  (:import [util.pkid_table PkidTable]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- missing-pkids [^PkidTable table]
  (.-missing-pkids table))

(defn- check-state! [^PkidTable table expected-vals expected-missing-pkids ^long expected-next-pkid]
  (is (instance? PkidTable table))
  (let [vals (pkid-table/vals table)
        missing-pkids (missing-pkids table)
        next-pkid (pkid-table/next-pkid table)]
    (is (= expected-vals vals))
    (is (= expected-missing-pkids (vec missing-pkids)))
    (is (= expected-next-pkid next-pkid))
    (is (= expected-next-pkid
           (+ (count expected-vals)
              (count missing-pkids))))
    (is (coll/every?
          #(<= 0 % (dec expected-next-pkid))
          missing-pkids))
    table))

(deftest pkid-table-test
  (let [table (pkid-table/pkid-table)]
    (check-state! table [] [] 0)
    (is (= [] (pkid-table/vals table)))
    (is (= [[] []] (mapv pkid-table/vals [table table])))
    (is (= 0 (pkid-table/next-pkid table)))
    (is (= [] (vec (pkid-table/find-pkids table :not-found))))))

(deftest append-test
  (let [empty-table (pkid-table/pkid-table)
        first-table (pkid-table/append empty-table :a)
        second-table (pkid-table/append first-table nil)
        third-table (pkid-table/append second-table false)]
    (testing "Appends values at consecutive pkids."
      (check-state! first-table [:a] [] 1)
      (check-state! second-table [:a nil] [] 2)
      (check-state! third-table [:a nil false] [] 3))

    (testing "Earlier tables remain unchanged."
      (check-state! empty-table [] [] 0)
      (check-state! first-table [:a] [] 1)
      (check-state! second-table [:a nil] [] 2)))

  (testing "Append does not reuse deleted pkids."
    (let [table (-> (pkid-table/pkid-table)
                    (pkid-table/append :a)
                    (pkid-table/append :b)
                    (pkid-table/dissoc-pkids [0 1])
                    (pkid-table/append :c))]
      (check-state! table [:c] [0 1] 3)
      (is (= [2] (vec (pkid-table/find-pkids table :c)))))))

(deftest assoc-pkids-test
  (testing "Empty pkids leave the table unchanged."
    (let [table (pkid-table/pkid-table)]
      (is (identical? table (pkid-table/assoc-pkids table [] :a)))))

  (testing "Associating at the next pkid is equivalent to appending."
    (let [table (pkid-table/assoc-pkids (pkid-table/pkid-table) [0] :a)]
      (check-state! table [:a] [] 1)))

  (testing "Associating beyond the next pkid records the intervening holes."
    (let [table (-> (pkid-table/pkid-table)
                    (pkid-table/assoc-pkids [3] :d)
                    (check-state! [:d] [0 1 2] 4)
                    (pkid-table/assoc-pkids [1] :b)
                    (check-state! [:b :d] [0 2] 4)
                    (pkid-table/assoc-pkids [3] :D)
                    (check-state! [:b :D] [0 2] 4)
                    (pkid-table/assoc-pkids [7] :h))]
      (check-state! table [:b :D :h] [0 2 4 5 6] 8)))

  (testing "Missing pkids can be restored out of order."
    (let [table (-> (pkid-table/pkid-table)
                    (pkid-table/assoc-pkids [5] :f)
                    (pkid-table/assoc-pkids [4] :e)
                    (pkid-table/assoc-pkids [2] :c)
                    (pkid-table/assoc-pkids [0] :a)
                    (pkid-table/assoc-pkids [3] :d)
                    (pkid-table/assoc-pkids [1] :b))]
      (check-state! table [:a :b :c :d :e :f] [] 6)))

  (testing "A batch may replace, restore, append, and create gaps."
    (let [table (-> (pkid-table/pkid-table)
                    (pkid-table/append :a)
                    (pkid-table/append :b)
                    (pkid-table/append :c)
                    (pkid-table/dissoc-pkids [1])
                    (pkid-table/assoc-pkids #{5 1 2 3} :x))]
      (check-state! table [:a :x :x :x :x] [4] 6)
      (is (= [1 2 3 5]
             (vec (pkid-table/find-pkids table :x))))))

  (testing "Duplicate pkids and falsey values are supported."
    (let [nil-table (pkid-table/assoc-pkids (pkid-table/pkid-table) [2 2] nil)
          false-table (pkid-table/assoc-pkids nil-table [0 1] false)]
      (check-state! false-table [false false nil] [] 3))))

(deftest dissoc-pkids-test
  (let [initial-table (-> (pkid-table/pkid-table)
                          (pkid-table/append :a)
                          (pkid-table/append :b)
                          (pkid-table/append :c)
                          (pkid-table/append :d))]
    (testing "Values can be removed from the beginning, middle, and end."
      (let [table (-> initial-table
                      (pkid-table/dissoc-pkids [0])
                      (check-state! [:b :c :d] [0] 4)
                      (pkid-table/dissoc-pkids [2])
                      (check-state! [:b :d] [0 2] 4)
                      (pkid-table/dissoc-pkids [3]))]
        (check-state! table [:b] [0 2 3] 4)))

    (testing "Unordered and duplicate pkids are supported."
      (let [table (pkid-table/dissoc-pkids initial-table [2 0 2 1])]
        (check-state! table [:d] [0 1 2] 4)))

    (testing "Missing and out-of-range pkids leave the table unchanged."
      (let [table (pkid-table/dissoc-pkids initial-table [1])]
        (is (identical? table (pkid-table/dissoc-pkids table [1 4 100])))))

    (testing "Deleting all values preserves the next pkid."
      (let [table (pkid-table/dissoc-pkids initial-table [0 1 2 3])]
        (check-state! table [] [0 1 2 3] 4)
        (is (= [] (vec (pkid-table/find-pkids table :a))))))

    (testing "The original table remains unchanged."
      (check-state! initial-table [:a :b :c :d] [] 4)))

  (testing "Invalid pkids are rejected."
    (let [table (pkid-table/pkid-table)]
      (is (thrown? AssertionError (pkid-table/dissoc-pkids table [-1])))
      (is (thrown? AssertionError (pkid-table/dissoc-pkids table [1.0])))
      (is (thrown? AssertionError (pkid-table/assoc-pkids table [:invalid] :a))))))

(deftest find-pkids-test
  (let [equal-but-not-identical-a (String. "same")
        equal-but-not-identical-b (String. "same")
        table (-> (pkid-table/pkid-table)
                  (pkid-table/append :a)
                  (pkid-table/append nil)
                  (pkid-table/append equal-but-not-identical-a)
                  (pkid-table/append :a)
                  (pkid-table/append nil)
                  (pkid-table/append equal-but-not-identical-b)
                  (pkid-table/dissoc-pkids [0 4]))]
    (is (= [] (vec (pkid-table/find-pkids table :not-found))))
    (is (= [3] (vec (pkid-table/find-pkids table :a))))
    (is (= [1] (vec (pkid-table/find-pkids table nil))))
    (is (= [2 5] (vec (pkid-table/find-pkids table "same")))))

  (testing "Stable pkids survive deletion, append, and restoration."
    (let [table (-> (pkid-table/pkid-table)
                    (pkid-table/append :a)
                    (pkid-table/append :x)
                    (pkid-table/append :b)
                    (pkid-table/append :x)
                    (pkid-table/dissoc-pkids [1 3])
                    (pkid-table/append :c)
                    (pkid-table/assoc-pkids [3 1] :x))]
      (check-state! table [:a :x :b :x :c] [] 5)
      (is (= [1 3] (vec (pkid-table/find-pkids table :x)))))))

(deftest dense-int-set-boundary-test
  (let [table (-> (pkid-table/pkid-table)
                  (pkid-table/assoc-pkids [4096] :last)
                  (pkid-table/assoc-pkids [4095] :before-last)
                  (pkid-table/dissoc-pkids [4096])
                  (pkid-table/append :after-last)
                  (pkid-table/assoc-pkids [0] :first))]
    (is (= [:first :before-last :after-last]
           (pkid-table/vals table)))
    (is (= 4098 (pkid-table/next-pkid table)))
    (is (= [0] (vec (pkid-table/find-pkids table :first))))
    (is (= [4095] (vec (pkid-table/find-pkids table :before-last))))
    (is (= [4097] (vec (pkid-table/find-pkids table :after-last))))
    (is (= 4098
           (+ (count (pkid-table/vals table))
              (count (missing-pkids table)))))))

(def ^:private test-values
  [nil false :a :b])

(def ^:private operation-gen
  (gen/one-of
    [(gen/tuple (gen/return :append)
                (gen/elements test-values))
     (gen/tuple (gen/return :assoc-pkids)
                (gen/fmap set (gen/vector (gen/choose 0 20) 0 5))
                (gen/elements test-values))
     (gen/tuple (gen/return :dissoc-pkids)
                (gen/fmap set (gen/vector (gen/choose 0 20) 0 5)))]))

(defn- apply-model-operation [{:keys [entries next-pkid]} [operation arg value]]
  (let [next-pkid (long next-pkid)]
    (case operation
      :append
      {:entries (assoc entries next-pkid arg)
       :next-pkid (inc next-pkid)}

      :assoc-pkids
      {:entries (reduce (fn [entries pkid]
                          (assoc entries pkid value))
                        entries
                        arg)
       :next-pkid (reduce (fn [^long next-pkid pkid]
                            (max next-pkid (inc (long pkid))))
                          next-pkid
                          arg)}

      :dissoc-pkids
      {:entries (reduce dissoc entries arg)
       :next-pkid next-pkid})))

(defn- apply-table-operation [table [operation arg value]]
  (case operation
    :append (pkid-table/append table arg)
    :assoc-pkids (pkid-table/assoc-pkids table arg value)
    :dissoc-pkids (pkid-table/dissoc-pkids table arg)))

(defn- model-pkids [entries value]
  (into (int-map/int-set)
        (keep (fn [[pkid entry-value]]
                (when (= value entry-value)
                  pkid)))
        entries))

(defn- table-matches-model? [{:keys [entries next-pkid]} table]
  (and (= (vec (clojure.core/vals entries))
          (pkid-table/vals table))
       (= next-pkid
          (pkid-table/next-pkid table)
          (+ (count (pkid-table/vals table))
             (count (missing-pkids table))))
       (coll/every? (fn [value]
                      (= (model-pkids entries value)
                         (pkid-table/find-pkids table value)))
                    test-values)))

(defspec operations-match-sorted-map-model 500
  (prop/for-all [operations (gen/vector operation-gen 0 100)]
    (loop [operation-index 0
           model {:entries (sorted-map)
                  :next-pkid 0}
           table (pkid-table/pkid-table)]
      (if (= operation-index (count operations))
        true
        (let [operation (operations operation-index)
              model (apply-model-operation model operation)
              table (apply-table-operation table operation)]
          (if (table-matches-model? model table)
            (recur (inc operation-index) model table)
            false))))))

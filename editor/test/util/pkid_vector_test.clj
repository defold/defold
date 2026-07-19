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

(ns util.pkid-vector-test
  (:require [clojure.data.int-map :as int-map]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [util.coll :as coll]
            [util.pkid-vector :as pkid-vector])
  (:import [clojure.lang IReduceInit PkidVector]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- missing-pkids [^PkidVector vector]
  (let [^java.util.Iterator iterator (.missingPkidIterator vector)]
    (loop [result (transient [])]
      (if (.hasNext iterator)
        (recur (conj! result (.next iterator)))
        (persistent! result)))))

(defn- check-state! [^PkidVector vector expected-vals expected-missing-pkids ^long expected-next-pkid]
  (is (instance? PkidVector vector))
  (let [missing-pkids (missing-pkids vector)
        next-pkid (pkid-vector/next-pkid vector)]
    (is (= expected-vals vector))
    (is (= expected-missing-pkids missing-pkids))
    (is (= expected-next-pkid next-pkid))
    (is (= expected-next-pkid
           (+ (count expected-vals)
              (count missing-pkids))))
    (is (coll/every?
          #(<= 0 % (dec expected-next-pkid))
          missing-pkids))
    vector))

(deftest pkid-vector-test
  (let [table (pkid-vector/pkid-vector)]
    (check-state! table [] [] 0)
    (is (identical? PkidVector/EMPTY table))
    (is (= 0 (pkid-vector/next-pkid table)))
    (is (= [] (pkid-vector/find-pkids table :not-found)))))

(deftest vector-behavior-test
  (let [initial-vector (-> (pkid-vector/pkid-vector)
                           (conj :a :b :c)
                           (pkid-vector/dissoc-pkids [1]))]
    (testing "Read operations use realized vector indexes."
      (is (vector? initial-vector))
      (is (= [:a :c] initial-vector))
      (is (= initial-vector [:a :c]))
      (is (= (hash [:a :c]) (hash initial-vector)))
      (is (= 2 (count initial-vector)))
      (is (= :a (initial-vector 0) (get initial-vector 0) (nth initial-vector 0)))
      (is (= :c (peek initial-vector)))
      (is (= [:c :a] (vec (rseq initial-vector)))))

    (testing "Conj and indexed assoc preserve the pkid state."
      (check-state! (conj initial-vector :d) [:a :c :d] [1] 4)
      (check-state! (assoc initial-vector 1 :C) [:a :C] [1] 3)
      (check-state! (assoc initial-vector 2 :d) [:a :c :d] [1] 4))

    (testing "Pop removes the stable pkid of the final realized value."
      (check-state! (pop initial-vector) [:a] [1 2] 3)
      (let [trailing-holes-vector (-> (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [5] :f)
                                      (pkid-vector/assoc-pkids [2] :c)
                                      (pkid-vector/dissoc-pkids [5]))]
        (check-state! (pop trailing-holes-vector) [] [0 1 2 3 4 5] 6)))

    (testing "Empty resets the stable-pkid history and preserves metadata."
      (let [metadata {:test true}
            metadata-vector (with-meta initial-vector metadata)]
        (check-state! metadata-vector [:a :c] [1] 3)
        (is (= metadata (meta metadata-vector)))
        (let [empty-vector (empty metadata-vector)]
          (check-state! empty-vector [] [] 0)
          (is (= metadata (meta empty-vector))))))

    (testing "Subvectors retain normal realized-index association semantics."
      (let [subvector (subvec (conj initial-vector :d) 1 3)]
        (is (= [:c :d] subvector))
        (is (= [:C :d] (assoc subvector 0 :C)))))

    (testing "Generic transients are rejected because they cannot retain pkid state."
      (is (thrown-with-msg? UnsupportedOperationException
                            #"does not support generic transient conversion"
                            (transient initial-vector)))
      (is (thrown-with-msg? UnsupportedOperationException
                            #"does not support generic transient conversion"
                            (into initial-vector [:d])))))

  (testing "Vector equality reflects realized values rather than stable-pkid history."
    (let [first-vector (conj (pkid-vector/pkid-vector) :a)
          second-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [1] :a)]
      (is (= first-vector second-vector [:a]))
      (is (not= (pkid-vector/next-pkid first-vector)
                (pkid-vector/next-pkid second-vector))))))

(deftest persistent-vector-boundaries-test
  (testing "Vector operations preserve the subtype across trie boundaries."
    (doseq [^long size [31 32 33 1023 1024 1025 32767 32768 32769]]
      (let [initial-vector (reduce conj (pkid-vector/pkid-vector) (range size))
            last-index (dec size)
            associated-vector (assoc initial-vector last-index :last)
            appended-vector (conj associated-vector :appended)
            popped-vector (pop appended-vector)]
        (check-state! initial-vector (vec (range size)) [] size)
        (check-state! associated-vector
                      (assoc (vec (range size)) last-index :last)
                      []
                      size)
        (check-state! appended-vector
                      (conj (assoc (vec (range size)) last-index :last)
                            :appended)
                      []
                      (inc size))
        (check-state! popped-vector
                      (assoc (vec (range size)) last-index :last)
                      [size]
                      (inc size)))))

  (testing "Batched transient finalization preserves metadata."
    (let [metadata {:test true}
          initial-vector (with-meta (reduce conj (pkid-vector/pkid-vector) (range 65))
                            metadata)
          associated-vector (pkid-vector/assoc-pkids initial-vector [0 32 64] :x)
          dissociated-vector (pkid-vector/dissoc-pkids associated-vector [1 33])]
      (is (= metadata (meta associated-vector)))
      (is (= metadata (meta dissociated-vector)))
      (check-state! associated-vector
                    (-> (vec (range 65))
                        (assoc 0 :x)
                        (assoc 32 :x)
                        (assoc 64 :x))
                    []
                    65)
      (check-state! dissociated-vector
                    (into [:x] (concat (range 2 32) [:x] (range 34 64) [:x]))
                    [1 33]
                    65))))

(deftest java-method-api-test
  (let [^PkidVector initial-vector (-> (pkid-vector/pkid-vector)
                                      (conj :a)
                                      (conj 1))]
    (testing "Direct Java methods match the Clojure wrappers."
      (let [direct-vector (.assocPkids initial-vector [5 1 3] :x)
            wrapped-vector (pkid-vector/assoc-pkids initial-vector [5 1 3] :x)]
        (check-state! direct-vector [:a :x :x :x] [2 4] 6)
        (check-state! wrapped-vector [:a :x :x :x] [2 4] 6)
        (is (= (missing-pkids direct-vector)
               (missing-pkids wrapped-vector)))
        (is (= (pkid-vector/next-pkid direct-vector)
               (pkid-vector/next-pkid wrapped-vector)))
        (check-state! (.dissocPkids direct-vector [0 5])
                      [:x :x]
                      [0 2 4 5]
                      6)))

    (testing "Direct no-op batches preserve identity."
      (is (identical? initial-vector (.assocPkids initial-vector nil :x)))
      (is (identical? initial-vector (.dissocPkids initial-vector [10]))))

    (testing "Find uses Clojure equality and returns a sorted vector."
      (let [matching-pkids (.findPkids initial-vector (int 1))]
        (is (= [1] matching-pkids))
        (is (vector? matching-pkids))))

  (testing "Reducible-only pkid batches are traversed once."
    (let [visit-count (atom 0)
          pkids (reify IReduceInit
                  (reduce [_ reducing-fn init]
                    (clojure.core/reduce
                      (fn [result pkid]
                        (swap! visit-count inc)
                        (reducing-fn result pkid))
                      init
                      [3 1 3 0])))
          vector (.assocPkids (pkid-vector/pkid-vector) pkids :x)]
      (is (= 4 @visit-count))
      (check-state! vector [:x :x :x] [2] 4)))

  (testing "The empty singleton is canonical."
    (is (identical? PkidVector/EMPTY (pkid-vector/pkid-vector)))
    (is (identical? PkidVector/EMPTY (empty (conj PkidVector/EMPTY :a)))))))

(deftest missing-pkid-persistence-test
  (let [initial-vector (reduce conj PkidVector/EMPTY [:a :b :c :d])
        base-vector (pkid-vector/dissoc-pkids initial-vector [1])
        first-branch (pkid-vector/dissoc-pkids base-vector [2])
        second-branch (pkid-vector/dissoc-pkids base-vector [3])
        restored-branch (pkid-vector/assoc-pkids first-branch [1] :B)]
    (testing "Updates cannot mutate their source or sibling branches."
      (check-state! initial-vector [:a :b :c :d] [] 4)
      (check-state! base-vector [:a :c :d] [1] 4)
      (check-state! first-branch [:a :d] [1 2] 4)
      (check-state! second-branch [:a :c] [1 3] 4)
      (check-state! restored-branch [:a :B :d] [2] 4)))

  (let [initial-vector (reduce conj PkidVector/EMPTY (range 260))
        base-vector (pkid-vector/dissoc-pkids initial-vector [1 129 257])
        first-branch (pkid-vector/dissoc-pkids base-vector [2 130 258])
        second-branch (pkid-vector/dissoc-pkids base-vector [3 131 259])
        restored-branch (pkid-vector/assoc-pkids first-branch [1 129] :restored)]
    (testing "Sibling updates remain isolated across int-set leaves."
      (is (= [] (missing-pkids initial-vector)))
      (is (= [1 129 257] (missing-pkids base-vector)))
      (is (= [1 2 129 130 257 258] (missing-pkids first-branch)))
      (is (= [1 3 129 131 257 259] (missing-pkids second-branch)))
      (is (= [2 130 257 258] (missing-pkids restored-branch)))
      (is (= [1 129] (pkid-vector/find-pkids restored-branch :restored))))))

(deftest conj-test
  (let [empty-table (pkid-vector/pkid-vector)
        first-table (conj empty-table :a)
        second-table (conj first-table nil)
        third-table (conj second-table false)]
    (testing "Conjoins values at consecutive pkids."
      (check-state! first-table [:a] [] 1)
      (check-state! second-table [:a nil] [] 2)
      (check-state! third-table [:a nil false] [] 3))

    (testing "Earlier tables remain unchanged."
      (check-state! empty-table [] [] 0)
      (check-state! first-table [:a] [] 1)
      (check-state! second-table [:a nil] [] 2)))

  (testing "Conj does not reuse deleted pkids."
    (let [table (-> (pkid-vector/pkid-vector)
                    (conj :a)
                    (conj :b)
                    (pkid-vector/dissoc-pkids [0 1])
                    (conj :c))]
      (check-state! table [:c] [0 1] 3)
      (is (= [2] (pkid-vector/find-pkids table :c))))))

(deftest assoc-pkids-test
  (testing "Empty pkids leave the table unchanged."
    (let [table (pkid-vector/pkid-vector)]
      (is (identical? table (pkid-vector/assoc-pkids table [] :a)))
      (is (identical? table (pkid-vector/assoc-pkids table nil :a)))))

  (testing "Associating at the next pkid is equivalent to appending."
    (let [table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [0] :a)]
      (check-state! table [:a] [] 1)))

  (testing "Associating beyond the next pkid records the intervening holes."
    (let [table (-> (pkid-vector/pkid-vector)
                    (pkid-vector/assoc-pkids [3] :d)
                    (check-state! [:d] [0 1 2] 4)
                    (pkid-vector/assoc-pkids [1] :b)
                    (check-state! [:b :d] [0 2] 4)
                    (pkid-vector/assoc-pkids [3] :D)
                    (check-state! [:b :D] [0 2] 4)
                    (pkid-vector/assoc-pkids [7] :h))]
      (check-state! table [:b :D :h] [0 2 4 5 6] 8)))

  (testing "Missing pkids can be restored out of order."
    (let [table (-> (pkid-vector/pkid-vector)
                    (pkid-vector/assoc-pkids [5] :f)
                    (pkid-vector/assoc-pkids [4] :e)
                    (pkid-vector/assoc-pkids [2] :c)
                    (pkid-vector/assoc-pkids [0] :a)
                    (pkid-vector/assoc-pkids [3] :d)
                    (pkid-vector/assoc-pkids [1] :b))]
      (check-state! table [:a :b :c :d :e :f] [] 6)))

  (testing "A batch may replace, restore, append, and create gaps."
    (let [table (-> (pkid-vector/pkid-vector)
                    (conj :a)
                    (conj :b)
                    (conj :c)
                    (pkid-vector/dissoc-pkids [1])
                    (pkid-vector/assoc-pkids #{5 1 2 3} :x))]
      (check-state! table [:a :x :x :x :x] [4] 6)
      (is (= [1 2 3 5]
             (pkid-vector/find-pkids table :x)))))

  (testing "Duplicate pkids and falsey values are supported."
    (let [nil-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [2 2] nil)
          false-table (pkid-vector/assoc-pkids nil-table [0 1] false)]
      (check-state! false-table [false false nil] [] 3))))

(deftest dissoc-pkids-test
  (let [initial-table (-> (pkid-vector/pkid-vector)
                          (conj :a)
                          (conj :b)
                          (conj :c)
                          (conj :d))]
    (testing "Values can be removed from the beginning, middle, and end."
      (let [table (-> initial-table
                      (pkid-vector/dissoc-pkids [0])
                      (check-state! [:b :c :d] [0] 4)
                      (pkid-vector/dissoc-pkids [2])
                      (check-state! [:b :d] [0 2] 4)
                      (pkid-vector/dissoc-pkids [3]))]
        (check-state! table [:b] [0 2 3] 4)))

    (testing "Unordered and duplicate pkids are supported."
      (let [table (pkid-vector/dissoc-pkids initial-table [2 0 2 1])]
        (check-state! table [:d] [0 1 2] 4)))

    (testing "Missing and out-of-range pkids leave the table unchanged."
      (let [table (pkid-vector/dissoc-pkids initial-table [1])]
        (is (identical? table (pkid-vector/dissoc-pkids table [1 4 100])))
        (is (identical? table (pkid-vector/dissoc-pkids table nil)))))

    (testing "Deleting all values preserves the next pkid."
      (let [table (pkid-vector/dissoc-pkids initial-table [0 1 2 3])]
        (check-state! table [] [0 1 2 3] 4)
        (is (= [] (pkid-vector/find-pkids table :a)))))

    (testing "The original table remains unchanged."
      (check-state! initial-table [:a :b :c :d] [] 4)))

  (testing "Invalid pkids are rejected."
    (let [table (pkid-vector/pkid-vector)]
      (is (thrown-with-msg? IllegalArgumentException
                            #"pkid must not be negative: -1"
                            (pkid-vector/dissoc-pkids table [-1])))
      (is (thrown-with-msg? IllegalArgumentException
                            #"pkid must not be negative: -1"
                            (pkid-vector/assoc-pkids table [-1] :a)))
      (is (thrown? NullPointerException (pkid-vector/dissoc-pkids table [nil])))
      (is (thrown? ClassCastException (pkid-vector/dissoc-pkids table [(int 1)])))
      (is (thrown? ClassCastException (pkid-vector/dissoc-pkids table [1.0])))
      (is (thrown? ClassCastException (pkid-vector/assoc-pkids table [:invalid] :a))))))

(deftest batched-updates-test
  (testing "A mixed batch is normalized before it is applied."
    (let [initial-table (-> (reduce conj
                                    (pkid-vector/pkid-vector)
                                    [:v0 :v1 :v2 :v3 :v4 :v5 :v6 :v7])
                            (pkid-vector/dissoc-pkids [1 3 6]))
          unique-pkids [0 1 3 8 10 12]
          input-variants [[12 3 8 1 10 3 0 12]
                          unique-pkids
                          (int-map/dense-int-set unique-pkids)]
          updated-tables (mapv #(pkid-vector/assoc-pkids initial-table % :x)
                               input-variants)]
      (doseq [updated-table updated-tables]
        (check-state! updated-table
                      [:x :x :v2 :x :v4 :v5 :v7 :x :x :x]
                      [6 9 11]
                      13))

      (is (apply = updated-tables))
      (check-state! initial-table [:v0 :v2 :v4 :v5 :v7] [1 3 6] 8)))

  (testing "A single batch can restore every missing pkid."
    (let [initial-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [6] :last)
          restored-table (pkid-vector/assoc-pkids initial-table
                                                 [5 4 3 2 1 0 5 0]
                                                 :restored)]
      (check-state! restored-table
                    [:restored :restored :restored :restored :restored :restored :last]
                    []
                    7)))

  (testing "Replacement-only batches account for unrelated missing pkids."
    (let [table (-> (reduce conj
                            (pkid-vector/pkid-vector)
                            [:a :b :c :d :e])
                    (pkid-vector/dissoc-pkids [1 3])
                    (pkid-vector/assoc-pkids [4 2 0 2] :x))]
      (check-state! table [:x :x :x] [1 3] 5)))

  (testing "Each supplied batch is traversed once."
    (let [visit-count (atom 0)
          pkids (eduction (map (fn [pkid]
                                 (swap! visit-count inc)
                                 pkid))
                          [3 1 3 0])
          table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) pkids :x)]
      (is (= 4 @visit-count))
      (check-state! table [:x :x :x] [2] 4))

    (let [visit-count (atom 0)
          initial-table (reduce conj
                                (pkid-vector/pkid-vector)
                                [:a :b :c :d :e])
          pkids (eduction (map (fn [pkid]
                                 (swap! visit-count inc)
                                 pkid))
                          [3 1 3 10])
          table (pkid-vector/dissoc-pkids initial-table pkids)]
      (is (= 4 @visit-count))
      (check-state! table [:a :c :e] [1 3] 5)))

  (testing "A large restoration batch preserves stable order."
    (let [initial-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [2048] :last)
          restored-pkids (vec (range 0 2048 2))
          descending-pkids (into [] (rseq restored-pkids))
          table (pkid-vector/assoc-pkids initial-table descending-pkids :restored)]
      (is (= restored-pkids
             (pkid-vector/find-pkids table :restored)))
      (is (= [2048] (pkid-vector/find-pkids table :last)))
      (is (= (vec (range 1 2048 2))
             (missing-pkids table)))
      (check-state! initial-table [:last] (vec (range 2048)) 2049)))

  (testing "A large deletion batch rebuilds the values once."
    (let [initial-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                                (range 2049)
                                                :x)
          deleted-pkids (vec (range 0 2049 2))
          descending-pkids (into [] (rseq deleted-pkids))
          table (pkid-vector/dissoc-pkids initial-table descending-pkids)]
      (is (= 1024 (count table)))
      (is (coll/every? #{:x} table))
      (is (= deleted-pkids (missing-pkids table)))
      (is (= (vec (range 1 2049 2))
             (pkid-vector/find-pkids table :x)))
      (check-state! initial-table (vec (repeat 2049 :x)) [] 2049))))

(deftest find-pkids-test
  (let [equal-but-not-identical-a (String. "same")
        equal-but-not-identical-b (String. "same")
        table (-> (pkid-vector/pkid-vector)
                  (conj :a)
                  (conj nil)
                  (conj equal-but-not-identical-a)
                  (conj :a)
                  (conj nil)
                  (conj equal-but-not-identical-b)
                  (pkid-vector/dissoc-pkids [0 4]))]
    (is (= [] (pkid-vector/find-pkids table :not-found)))
    (is (= [3] (pkid-vector/find-pkids table :a)))
    (is (= [1] (pkid-vector/find-pkids table nil)))
    (is (= [2 5] (pkid-vector/find-pkids table "same"))))

  (testing "Stable pkids survive deletion, conj, and restoration."
    (let [table (-> (pkid-vector/pkid-vector)
                    (conj :a)
                    (conj :x)
                    (conj :b)
                    (conj :x)
                    (pkid-vector/dissoc-pkids [1 3])
                    (conj :c)
                    (pkid-vector/assoc-pkids [3 1] :x))]
      (check-state! table [:a :x :b :x :c] [] 5)
      (is (= [1 3] (pkid-vector/find-pkids table :x)))))

  (testing "Many matching values are mapped through holes as a batch."
    (let [matching-pkids (vec (range 64 1024 2))
          table (-> (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                            matching-pkids
                                            :match)
                    (conj :trailing)
                    (pkid-vector/dissoc-pkids [1023]))]
      (is (= matching-pkids
             (pkid-vector/find-pkids table :match))))))

(deftest int-set-leaf-boundary-test
  (let [table (-> (pkid-vector/pkid-vector)
                  (pkid-vector/assoc-pkids [128] :last)
                  (pkid-vector/assoc-pkids [127] :before-last)
                  (pkid-vector/dissoc-pkids [128])
                  (conj :after-last)
                  (pkid-vector/assoc-pkids [0] :first))]
    (is (= [:first :before-last :after-last]
           table))
    (is (= 130 (pkid-vector/next-pkid table)))
    (is (= [0] (pkid-vector/find-pkids table :first)))
    (is (= [127] (pkid-vector/find-pkids table :before-last)))
    (is (= [129] (pkid-vector/find-pkids table :after-last)))
    (is (= 130
           (+ (count table)
              (count (missing-pkids table)))))))

(deftest emptied-int-set-leaf-test
  (let [initial-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [130] :anchor)
        partially-restored-table (pkid-vector/assoc-pkids initial-table (range 128) :restored)
        fully-restored-table (pkid-vector/assoc-pkids partially-restored-table [128 129] :filled)]
    (testing "Iteration skips an emptied int-set leaf."
      (is (= [128 129] (missing-pkids partially-restored-table)))
      (is (= (vec (range 128)) (pkid-vector/find-pkids partially-restored-table :restored)))
      (is (= [130] (pkid-vector/find-pkids partially-restored-table :anchor))))

    (testing "Restoring every missing pkid canonicalizes the empty set."
      (is (= [] (missing-pkids fully-restored-table)))
      (is (= [128 129] (pkid-vector/find-pkids fully-restored-table :filled)))
      (is (= 131 (count fully-restored-table) (pkid-vector/next-pkid fully-restored-table))))))

(deftest batched-int-set-leaf-boundary-test
  (let [boundary-pkids [127 128 129 255 256 257]
        initial-table (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                              [126 130 254 258]
                                              :anchor)
        restored-table (pkid-vector/assoc-pkids initial-table
                                               [257 129 127 255 128 256 129]
                                               :boundary)
        deletion-pkids (int-map/int-set boundary-pkids)
        deleted-table (pkid-vector/dissoc-pkids restored-table deletion-pkids)]
    (is (= boundary-pkids
           (pkid-vector/find-pkids restored-table :boundary)))
    (is (= [126 130 254 258]
           (pkid-vector/find-pkids restored-table :anchor)))
    (is (= 259
           (+ (count restored-table)
              (count (missing-pkids restored-table)))))
    (is (= [126 130 254 258]
           (pkid-vector/find-pkids deleted-table :anchor)))
    (is (= [] (pkid-vector/find-pkids deleted-table :boundary)))
    (is (= boundary-pkids (vec deletion-pkids)))))

(def ^:private test-values
  [nil false :a :b])

(def ^:private operation-gen
  (gen/one-of
    [(gen/tuple (gen/return :conj)
                (gen/elements test-values))
     (gen/tuple (gen/return :assoc-pkids)
                (gen/vector (gen/choose 0 50) 0 15)
                (gen/elements test-values))
     (gen/tuple (gen/return :dissoc-pkids)
                (gen/vector (gen/choose 0 50) 0 15))]))

(defn- apply-model-operation [{:keys [entries next-pkid]} [operation arg value]]
  (let [next-pkid (long next-pkid)]
    (case operation
      :conj
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
    :conj (conj table arg)
    :assoc-pkids (pkid-vector/assoc-pkids table arg value)
    :dissoc-pkids (pkid-vector/dissoc-pkids table arg)))

(defn- model-pkids [entries value]
  (into (int-map/int-set)
        (keep (fn [[pkid entry-value]]
                (when (= value entry-value)
                  pkid)))
        entries))

(defn- table-matches-model? [{:keys [entries next-pkid]} table]
  (and (= (vec (clojure.core/vals entries))
          table)
       (= next-pkid
          (pkid-vector/next-pkid table)
          (+ (count table)
             (count (missing-pkids table))))
       (coll/every? (fn [value]
                      (= (vec (model-pkids entries value))
                         (pkid-vector/find-pkids table value)))
                    test-values)))

(defspec operations-match-sorted-map-model 500
  (prop/for-all [operations (gen/vector operation-gen 0 100)]
    (loop [operation-index 0
           model {:entries (sorted-map)
                  :next-pkid 0}
           table (pkid-vector/pkid-vector)]
      (if (= operation-index (count operations))
        true
        (let [operation (operations operation-index)
              model (apply-model-operation model operation)
              table (apply-table-operation table operation)]
          (if (table-matches-model? model table)
            (recur (inc operation-index) model table)
            false))))))

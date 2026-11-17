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
            [clojure.test :refer [deftest is testing]]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [util.coll :as coll]
            [util.pkid-vector :as pkid-vector])
  (:import [clojure.lang IReduceInit PersistentQueue RT]
           [com.defold.util PkidVector]
           [java.util ArrayList Collection HashMap List NoSuchElementException]
           [java.util.concurrent Callable CountDownLatch Executors]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- missing-pkids [table]
  (let [matches (pkid-vector/find-pkids-by-value table (into #{} table))
        live-pkids (coll/into-> (coll/vals matches) #{} cat)]
    (coll/into-> (range (pkid-vector/next-pkid table)) [] (remove live-pkids))))

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
  (let [pkid-vector (pkid-vector/pkid-vector)]
    (check-state! pkid-vector [] [] 0)
    (is (identical? PkidVector/EMPTY pkid-vector))
    (is (= 0 (pkid-vector/next-pkid pkid-vector)))
    (is (= [] (pkid-vector/find-pkids pkid-vector :not-found)))))

;; Checks reads, stack operations, equality, and resetting slot history.
(deftest vector-behavior-test
  (let [initial-vector (-> (pkid-vector/pkid-vector)
                           (conj :a :b :c)
                           (pkid-vector/dissoc-pkids [1]))]
    (testing "Read operations use realized vector indexes."
      (is (vector? initial-vector))
      (is (= [:a :c] initial-vector))
      (is (= initial-vector [:a :c]))
      (is (= "[:a :c]" (str initial-vector)))
      (is (= (hash [:a :c]) (hash initial-vector)))
      (is (= 2 (count initial-vector)))
      (is (= :a (initial-vector 0) (get initial-vector 0) (nth initial-vector 0)))
      (is (= :c (nth initial-vector 1) (initial-vector 1)))
      (is (= :c (peek initial-vector)))
      (is (= [:c :a] (vec (rseq initial-vector))))
      (is (thrown? IndexOutOfBoundsException (initial-vector 2)))
      (is (thrown? IllegalArgumentException (initial-vector :invalid))))

    (testing "Vector comparison orders by count, then by live values."
      (is (pos? (compare initial-vector [:z])))
      (is (neg? (compare initial-vector [:a :d])))
      (is (pos? (compare [:a :d] initial-vector)))
      (is (zero? (compare initial-vector [:a :c]))))

    (testing "Key/value reduction uses live indexes and supports early termination."
      (is (= [:a :c] (reduce-kv assoc [] initial-vector)))
      (is (= [0 :a] (reduce-kv (fn [_ index value] (reduced [index value]))
                               nil initial-vector))))

    (testing "Conj and indexed assoc preserve the pkid state."
      (check-state! (conj initial-vector :d) [:a :c :d] [1] 4)
      (check-state! (assoc initial-vector 1 :C) [:a :C] [1] 3)
      (check-state! (assoc initial-vector 2 :d) [:a :c :d] [1] 4))

    (testing "Pop removes the stable pkid of the final realized value."
      (check-state! (pop initial-vector) [:a] [1 2] 3)
      (let [trailing-holes-vector (-> (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [5] :f)
                                      (pkid-vector/assoc-pkids [2] :c)
                                      (pkid-vector/dissoc-pkids [5]))]
        (check-state! (pop trailing-holes-vector) [] [0 1 2 3 4 5] 6))
      (let [trailing-holes-vector (-> (pkid-vector/pkid-vector)
                                      (pkid-vector/assoc-pkids [2] :c)
                                      (pkid-vector/assoc-pkids [32] :last)
                                      (pkid-vector/dissoc-pkids [32]))]
        (is (= :c (peek trailing-holes-vector)))
        (is (= [:c] (vec (rseq trailing-holes-vector))))
        (check-state! (pop trailing-holes-vector) [] (vec (range 33)) 33)))

    (testing "Empty resets the stable-pkid history and preserves metadata."
      (let [metadata {:test true}
            metadata-vector (with-meta initial-vector metadata)
            empty-vector (check-state! (empty metadata-vector) [] [] 0)]
        (check-state! metadata-vector [:a :c] [1] 3)
        (is (= metadata (meta metadata-vector) (meta empty-vector)))
        (is (= "[]" (str empty-vector)))
        (is (nil? (peek empty-vector)))
        (is (nil? (rseq empty-vector)))
        (is (thrown? IllegalStateException (pop empty-vector)))))

    (testing "Subvectors retain normal realized-index association semantics."
      (let [subvector (subvec (conj initial-vector :d) 1 3)]
        (is (= [:c :d] subvector))
        (is (= [:C :d] (assoc subvector 0 :C)))))

    (testing "Java streams use the live values and respect subvector bounds."
      (is (= [:a :c] (.toList (.stream ^List initial-vector))))
      (let [subvector (subvec (conj initial-vector :d) 1 3)]
        (is (= [:c :d] (.toList (.stream ^List subvector))))
        (is (= [:c :d] (.toList (.parallelStream ^List subvector))))))

    (testing "Generic transient conversion is unsupported."
      (is (thrown? ClassCastException (transient initial-vector)))
      (check-state! (into initial-vector [:d]) [:a :c :d] [1] 4)))

  (testing "Vector equality reflects realized values rather than stable-pkid history."
    (let [first-vector (conj (pkid-vector/pkid-vector) :a)
          second-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [1] :a)]
      (is (= first-vector second-vector [:a]))
      (is (zero? (compare first-vector second-vector)))
      (is (not= (pkid-vector/next-pkid first-vector)
                (pkid-vector/next-pkid second-vector))))))

;; Checks Java equality and map lookups with lists and Clojure queues.
(deftest java-equality-test
  (let [table (-> (pkid-vector/pkid-vector)
                  (conj :a :removed nil)
                  (pkid-vector/dissoc-pkids [1]))
        java-list (doto (ArrayList.)
                    (.add :a)
                    (.add nil))
        queue (conj PersistentQueue/EMPTY :a nil)]
    (doseq [^Object other [java-list queue]]
      (is (.equals table other))
      (is (.equals other table))
      (is (= (.hashCode table) (.hashCode other)))
      (is (= :found (.get (doto (HashMap.) (.put table :found)) other)))
      (is (= :found (.get (doto (HashMap.) (.put other :found)) table))))
    (is (thrown? UnsupportedOperationException (.add table :b)))))

;; Checks that append, restore, and delete preserve slot IDs across tree levels.
(deftest persistent-vector-boundaries-test
  (testing "Vector operations preserve the subtype across trie boundaries."
    (doseq [^long size [31 32 33 1023 1024 1025 32767 32768 32769]]
      (let [values (vec (range size))
            initial-vector (reduce conj (pkid-vector/pkid-vector) values)
            last-index (dec size)
            associated-vector (assoc initial-vector last-index :last)
            appended-vector (conj associated-vector :appended)
            popped-vector (pop appended-vector)
            associated-values (assoc values last-index :last)]
        (check-state! initial-vector values [] size)
        (check-state! associated-vector associated-values [] size)
        (check-state! appended-vector (conj associated-values :appended) [] (inc size))
        (check-state! popped-vector associated-values [size] (inc size)))))

  (testing "Batch edits preserve past slot IDs and metadata."
    (let [metadata {:test true}
          values (vec (range 65))
          initial-vector (with-meta (reduce conj (pkid-vector/pkid-vector) values)
                           metadata)
          updated-vector (pkid-vector/assoc-pkids initial-vector [0 32 64] :x)
          dissociated-vector (pkid-vector/dissoc-pkids updated-vector [1 33])]
      (is (= metadata (meta updated-vector) (meta dissociated-vector)))
      (check-state! updated-vector
                    (assoc values 0 :x 32 :x 64 :x)
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

    (testing "Vector searches match equivalent numeric values and use live indexes."
      (let [^PkidVector vector (-> (conj initial-vector :removed 1)
                                   (pkid-vector/dissoc-pkids [2]))
            query (int 1)]
        (is (= 1 (.indexOf vector query)))
        (is (= 2 (.lastIndexOf vector query)))
        (is (.contains vector query))))

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
    (testing "Sibling updates remain isolated across slot ranges."
      (is (= [] (missing-pkids initial-vector)))
      (is (= [1 129 257] (missing-pkids base-vector)))
      (is (= [1 2 129 130 257 258] (missing-pkids first-branch)))
      (is (= [1 3 129 131 257 259] (missing-pkids second-branch)))
      (is (= [2 130 257 258] (missing-pkids restored-branch)))
      (is (= [1 129] (pkid-vector/find-pkids restored-branch :restored))))))

(deftest conj-test
  (let [empty-pkid-vector (pkid-vector/pkid-vector)
        first-pkid-vector (conj empty-pkid-vector :a)
        second-pkid-vector (conj first-pkid-vector nil)
        third-pkid-vector (conj second-pkid-vector false)]
    (testing "Conjoins values at consecutive pkids."
      (check-state! first-pkid-vector [:a] [] 1)
      (check-state! second-pkid-vector [:a nil] [] 2)
      (check-state! third-pkid-vector [:a nil false] [] 3))

    (testing "Earlier pkid-vectors remain unchanged."
      (check-state! empty-pkid-vector [] [] 0)
      (check-state! first-pkid-vector [:a] [] 1)
      (check-state! second-pkid-vector [:a nil] [] 2)))

  (testing "Conj does not reuse deleted pkids."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (conj :a)
                          (conj :b)
                          (pkid-vector/dissoc-pkids [0 1])
                          (conj :c))]
      (check-state! pkid-vector [:c] [0 1] 3)
      (is (= [2] (pkid-vector/find-pkids pkid-vector :c))))))

(deftest assoc-pkids-test
  (testing "Empty pkids leave the pkid-vector unchanged."
    (let [pkid-vector (pkid-vector/pkid-vector)]
      (is (identical? pkid-vector (pkid-vector/assoc-pkids pkid-vector [] :a)))
      (is (identical? pkid-vector (pkid-vector/assoc-pkids pkid-vector nil :a)))))

  (testing "Associating at the next pkid is equivalent to appending."
    (doseq [^long size [0 1 31 32 33 1023 1024 1025]]
      (let [metadata {:size size}
            initial-pkid-vector (with-meta (reduce conj (pkid-vector/pkid-vector) (range size))
                                  metadata)
            pkid-vector (pkid-vector/assoc-pkids initial-pkid-vector [size] :appended)]
        (check-state! pkid-vector (conj (vec (range size)) :appended) [] (inc size))
        (is (= metadata (meta pkid-vector))))))

  (testing "Associating at the next pkid preserves earlier holes."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (conj :a :b)
                          (pkid-vector/dissoc-pkids [0])
                          (pkid-vector/assoc-pkids [2] :c))]
      (check-state! pkid-vector [:b :c] [0] 3)))

  (testing "Associating beyond the next pkid records the intervening holes."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (pkid-vector/assoc-pkids [3] :d)
                          (check-state! [:d] [0 1 2] 4)
                          (pkid-vector/assoc-pkids [1] :b)
                          (check-state! [:b :d] [0 2] 4)
                          (pkid-vector/assoc-pkids [3] :D)
                          (check-state! [:b :D] [0 2] 4)
                          (pkid-vector/assoc-pkids [7] :h))]
      (check-state! pkid-vector [:b :D :h] [0 2 4 5 6] 8)))

  (testing "Missing pkids can be restored out of order."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (pkid-vector/assoc-pkids [5] :f)
                          (pkid-vector/assoc-pkids [4] :e)
                          (pkid-vector/assoc-pkids [2] :c)
                          (pkid-vector/assoc-pkids [0] :a)
                          (pkid-vector/assoc-pkids [3] :d)
                          (pkid-vector/assoc-pkids [1] :b))]
      (check-state! pkid-vector [:a :b :c :d :e :f] [] 6)))

  (testing "A batch may replace, restore, append, and create gaps."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (conj :a)
                          (conj :b)
                          (conj :c)
                          (pkid-vector/dissoc-pkids [1])
                          (pkid-vector/assoc-pkids #{5 1 2 3} :x))]
      (check-state! pkid-vector [:a :x :x :x :x] [4] 6)
      (is (= [1 2 3 5]
             (pkid-vector/find-pkids pkid-vector :x)))))

  (testing "Duplicate pkids and falsey values are supported."
    (let [nil-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [2 2] nil)
          false-pkid-vector (pkid-vector/assoc-pkids nil-pkid-vector [0 1] false)]
      (check-state! false-pkid-vector [false false nil] [] 3))))

(deftest dissoc-pkids-test
  (let [initial-pkid-vector (-> (pkid-vector/pkid-vector)
                                (conj :a)
                                (conj :b)
                                (conj :c)
                                (conj :d))]
    (testing "Values can be removed from the beginning, middle, and end."
      (let [pkid-vector (-> initial-pkid-vector
                            (pkid-vector/dissoc-pkids [0])
                            (check-state! [:b :c :d] [0] 4)
                            (pkid-vector/dissoc-pkids [2])
                            (check-state! [:b :d] [0 2] 4)
                            (pkid-vector/dissoc-pkids [3]))]
        (check-state! pkid-vector [:b] [0 2 3] 4)))

    (testing "Unordered and duplicate pkids are supported."
      (let [pkid-vector (pkid-vector/dissoc-pkids initial-pkid-vector [2 0 2 1])]
        (check-state! pkid-vector [:d] [0 1 2] 4)))

    (testing "Missing and out-of-range pkids leave the pkid-vector unchanged."
      (let [pkid-vector (pkid-vector/dissoc-pkids initial-pkid-vector [1])]
        (is (identical? pkid-vector (pkid-vector/dissoc-pkids pkid-vector [1 4 100])))
        (is (identical? pkid-vector (pkid-vector/dissoc-pkids pkid-vector nil)))))

    (testing "Deleting all values preserves the next pkid."
      (let [pkid-vector (pkid-vector/dissoc-pkids initial-pkid-vector [0 1 2 3])]
        (check-state! pkid-vector [] [0 1 2 3] 4)
        (is (= [] (pkid-vector/find-pkids pkid-vector :a)))))

    (testing "The original pkid-vector remains unchanged."
      (check-state! initial-pkid-vector [:a :b :c :d] [] 4)))

  (testing "Invalid pkids are rejected."
    (let [pkid-vector (pkid-vector/pkid-vector)]
      (is (thrown-with-msg? IllegalArgumentException
                            #"pkid must not be negative: -1"
                            (pkid-vector/dissoc-pkids pkid-vector [-1])))
      (is (thrown-with-msg? IllegalArgumentException
                            #"pkid must not be negative: -1"
                            (pkid-vector/assoc-pkids pkid-vector [-1] :a)))
      (is (thrown? NullPointerException (pkid-vector/dissoc-pkids pkid-vector [nil])))
      (is (thrown? ClassCastException (pkid-vector/dissoc-pkids pkid-vector [(int 1)])))
      (is (thrown? ClassCastException (pkid-vector/dissoc-pkids pkid-vector [1.0])))
      (is (thrown? ClassCastException (pkid-vector/assoc-pkids pkid-vector [:invalid] :a))))))

(deftest batched-updates-test
  (testing "A mixed batch is normalized before it is applied."
    (let [initial-pkid-vector (-> (reduce conj
                                          (pkid-vector/pkid-vector)
                                          [:v0 :v1 :v2 :v3 :v4 :v5 :v6 :v7])
                                  (pkid-vector/dissoc-pkids [1 3 6]))
          unique-pkids [0 1 3 8 10 12]
          input-variants [[12 3 8 1 10 3 0 12]
                          unique-pkids
                          (int-map/dense-int-set unique-pkids)]
          updated-pkid-vectors (mapv #(pkid-vector/assoc-pkids initial-pkid-vector % :x)
                                     input-variants)]
      (doseq [updated-pkid-vector updated-pkid-vectors]
        (check-state! updated-pkid-vector
                      [:x :x :v2 :x :v4 :v5 :v7 :x :x :x]
                      [6 9 11]
                      13))

      (is (apply = updated-pkid-vectors))
      (check-state! initial-pkid-vector [:v0 :v2 :v4 :v5 :v7] [1 3 6] 8)))

  (testing "A single batch can restore every missing pkid."
    (let [initial-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [6] :last)
          restored-pkid-vector (pkid-vector/assoc-pkids initial-pkid-vector
                                                        [5 4 3 2 1 0 5 0]
                                                        :restored)]
      (check-state! restored-pkid-vector
                    [:restored :restored :restored :restored :restored :restored :last]
                    []
                    7)))

  (testing "Replacement-only batches account for unrelated missing pkids."
    (let [pkid-vector (-> (reduce conj
                                  (pkid-vector/pkid-vector)
                                  [:a :b :c :d :e])
                          (pkid-vector/dissoc-pkids [1 3])
                          (pkid-vector/assoc-pkids [4 2 0 2] :x))]
      (check-state! pkid-vector [:x :x :x] [1 3] 5)))

  (testing "Each supplied batch is traversed once."
    (let [visit-count (atom 0)
          pkids (eduction (map (fn [pkid]
                                 (swap! visit-count inc)
                                 pkid))
                          [3 1 3 0])
          pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) pkids :x)]
      (is (= 4 @visit-count))
      (check-state! pkid-vector [:x :x :x] [2] 4))

    (let [visit-count (atom 0)
          initial-pkid-vector (reduce conj
                                      (pkid-vector/pkid-vector)
                                      [:a :b :c :d :e])
          pkids (eduction (map (fn [pkid]
                                 (swap! visit-count inc)
                                 pkid))
                          [3 1 3 10])
          pkid-vector (pkid-vector/dissoc-pkids initial-pkid-vector pkids)]
      (is (= 4 @visit-count))
      (check-state! pkid-vector [:a :c :e] [1 3] 5)))

  (testing "A large restoration batch preserves stable order."
    (let [initial-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [2048] :last)
          restored-pkids (vec (range 0 2048 2))
          descending-pkids (into [] (rseq restored-pkids))
          pkid-vector (pkid-vector/assoc-pkids initial-pkid-vector descending-pkids :restored)]
      (is (= restored-pkids
             (pkid-vector/find-pkids pkid-vector :restored)))
      (is (= [2048] (pkid-vector/find-pkids pkid-vector :last)))
      (is (= (vec (range 1 2048 2))
             (missing-pkids pkid-vector)))
      (check-state! initial-pkid-vector [:last] (vec (range 2048)) 2049)))

  (testing "A large deletion batch preserves slot order."
    (let [initial-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                                       (range 2049)
                                                       :x)
          deleted-pkids (vec (range 0 2049 2))
          descending-pkids (into [] (rseq deleted-pkids))
          pkid-vector (pkid-vector/dissoc-pkids initial-pkid-vector descending-pkids)]
      (is (= 1024 (count pkid-vector)))
      (is (coll/every? #{:x} pkid-vector))
      (is (= deleted-pkids (missing-pkids pkid-vector)))
      (is (= (vec (range 1 2049 2))
             (pkid-vector/find-pkids pkid-vector :x)))
      (check-state! initial-pkid-vector (vec (repeat 2049 :x)) [] 2049))))

(deftest find-pkids-test
  (let [equal-but-not-identical-a (String. "same")
        equal-but-not-identical-b (String. "same")
        pkid-vector (-> (pkid-vector/pkid-vector)
                        (conj :a)
                        (conj nil)
                        (conj equal-but-not-identical-a)
                        (conj :a)
                        (conj nil)
                        (conj equal-but-not-identical-b)
                        (pkid-vector/dissoc-pkids [0 4]))]
    (is (= [] (pkid-vector/find-pkids pkid-vector :not-found)))
    (is (= [3] (pkid-vector/find-pkids pkid-vector :a)))
    (is (= [1] (pkid-vector/find-pkids pkid-vector nil)))
    (is (= [2 5] (pkid-vector/find-pkids pkid-vector "same"))))

  (testing "Stable pkids survive deletion, conj, and restoration."
    (let [pkid-vector (-> (pkid-vector/pkid-vector)
                          (conj :a)
                          (conj :x)
                          (conj :b)
                          (conj :x)
                          (pkid-vector/dissoc-pkids [1 3])
                          (conj :c)
                          (pkid-vector/assoc-pkids [3 1] :x))]
      (check-state! pkid-vector [:a :x :b :x :c] [] 5)
      (is (= [1 3] (pkid-vector/find-pkids pkid-vector :x)))))

  (testing "Many matching values are mapped through holes as a batch."
    (let [matching-pkids (vec (range 64 1024 2))
          pkid-vector (-> (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                                   matching-pkids
                                                   :match)
                          (conj :trailing)
                          (pkid-vector/dissoc-pkids [1023]))]
      (is (= matching-pkids
             (pkid-vector/find-pkids pkid-vector :match))))))

(deftest find-pkids-by-value-test
  (let [equal-but-not-identical-a (String. "same")
        equal-but-not-identical-b (String. "same")
        pkid-vector (-> (pkid-vector/pkid-vector)
                        (conj :a)
                        (conj nil)
                        (conj equal-but-not-identical-a)
                        (conj :a)
                        (conj nil)
                        (conj equal-but-not-identical-b)
                        (pkid-vector/dissoc-pkids [0 4]))]
    (is (= {:a [3]
            nil [1]
            "same" [2 5]}
           (pkid-vector/find-pkids-by-value pkid-vector #{:a nil "same" :not-found})))
    (is (= {}
           (pkid-vector/find-pkids-by-value pkid-vector #{})))))

;; Checks append and restoration around a gap between distant slots.
(deftest sparse-slot-boundary-test
  (let [pkid-vector (-> (pkid-vector/pkid-vector)
                        (pkid-vector/assoc-pkids [128] :last)
                        (pkid-vector/assoc-pkids [127] :before-last)
                        (pkid-vector/dissoc-pkids [128])
                        (conj :after-last)
                        (pkid-vector/assoc-pkids [0] :first))]
    (is (= [:first :before-last :after-last]
           pkid-vector))
    (is (= 130 (pkid-vector/next-pkid pkid-vector)))
    (is (= [0] (pkid-vector/find-pkids pkid-vector :first)))
    (is (= [127] (pkid-vector/find-pkids pkid-vector :before-last)))
    (is (= [129] (pkid-vector/find-pkids pkid-vector :after-last)))
    (is (= 130
           (+ (count pkid-vector)
              (count (missing-pkids pkid-vector)))))))

;; Checks lookup while filling gaps in multiple batches.
(deftest restore-missing-slots-test
  (let [initial-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector) [130] :anchor)
        partially-restored-pkid-vector (pkid-vector/assoc-pkids initial-pkid-vector (range 128) :restored)
        fully-restored-pkid-vector (pkid-vector/assoc-pkids partially-restored-pkid-vector [128 129] :filled)]
    (testing "Iteration includes restored slots in order."
      (is (= [128 129] (missing-pkids partially-restored-pkid-vector)))
      (is (= (vec (range 128)) (pkid-vector/find-pkids partially-restored-pkid-vector :restored)))
      (is (= [130] (pkid-vector/find-pkids partially-restored-pkid-vector :anchor))))

    (testing "Restoring every missing pkid fills all gaps."
      (is (= [] (missing-pkids fully-restored-pkid-vector)))
      (is (= [128 129] (pkid-vector/find-pkids fully-restored-pkid-vector :filled)))
      (is (= 131 (count fully-restored-pkid-vector) (pkid-vector/next-pkid fully-restored-pkid-vector))))))

;; Checks out-of-order insertions and deletions across disjoint slot ranges.
(deftest batched-sparse-slot-boundary-test
  (let [anchor-pkids [126 130 254 258]
        boundary-pkids [127 128 129 255 256 257]
        jumbled-non-distinct-boundary-pkids [257 129 127 255 128 256 129]
        initial-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                                     anchor-pkids
                                                     :anchor)
        restored-pkid-vector (pkid-vector/assoc-pkids initial-pkid-vector
                                                      jumbled-non-distinct-boundary-pkids
                                                      :boundary)
        deletion-pkids (int-map/int-set boundary-pkids)
        deleted-pkid-vector (pkid-vector/dissoc-pkids restored-pkid-vector deletion-pkids)]
    (is (= boundary-pkids
           (pkid-vector/find-pkids restored-pkid-vector :boundary)))
    (is (= anchor-pkids
           (pkid-vector/find-pkids restored-pkid-vector :anchor)))
    (is (= {:anchor anchor-pkids
            :boundary boundary-pkids}
           (pkid-vector/find-pkids-by-value restored-pkid-vector #{:anchor :boundary :not-found})))
    (is (= 259
           (+ (count restored-pkid-vector)
              (count (missing-pkids restored-pkid-vector)))))
    (is (= anchor-pkids
           (pkid-vector/find-pkids deleted-pkid-vector :anchor)))
    (is (= [] (pkid-vector/find-pkids deleted-pkid-vector :boundary)))
    (is (= boundary-pkids (vec deletion-pkids)))))

(def ^:private test-values
  [nil false :a :b])

(def ^:private test-metadata
  [nil {:branch :first} {:branch :second}])

(def ^:private pkid-batch-representations
  [:nil :vector :list :set :int-set :reducible :iterable])

(def ^:private pkid-gen
  (gen/frequency
    [[8 (gen/choose 0 75)]
     [2 (gen/elements [127 128 129 255 256 257 1023 1024 1025])]]))

(def ^:private pkid-batch-gen
  (gen/vector pkid-gen 0 15))

(def ^:private operation-gen
  (gen/one-of
    [(gen/tuple (gen/return :conj)
                (gen/elements test-values))
     (gen/tuple (gen/return :assoc-index)
                gen/nat
                (gen/elements test-values))
     (gen/tuple (gen/return :assoc-pkids)
                pkid-batch-gen
                (gen/elements pkid-batch-representations)
                (gen/elements test-values))
     (gen/tuple (gen/return :dissoc-pkids)
                pkid-batch-gen
                (gen/elements pkid-batch-representations))
     (gen/tuple (gen/return :pop))
     (gen/tuple (gen/return :empty))
     (gen/tuple (gen/return :with-meta)
                (gen/elements test-metadata))]))

(defn- represented-pkids [pkids representation]
  (case representation
    :nil nil
    :vector pkids
    :list (apply list pkids)
    :set (set pkids)
    :int-set (int-map/int-set pkids)
    :reducible (eduction (map identity) pkids)
    :iterable (ArrayList. ^Collection pkids)))

(defn- effective-pkids [[_ pkids representation]]
  (if (= :nil representation)
    []
    pkids))

(defn- apply-model-operation [{:keys [entries next-pkid] :as model} [operation arg representation value :as operation-form]]
  (let [next-pkid (long next-pkid)]
    (case operation
      :conj
      {:entries (assoc entries next-pkid arg)
       :next-pkid (inc next-pkid)
       :meta (:meta model)}

      :assoc-index
      (let [index (mod (long arg) (inc (count entries)))]
        (if (= index (count entries))
          {:entries (assoc entries next-pkid representation)
           :next-pkid (inc next-pkid)
           :meta (:meta model)}
          (let [pkid (key (nth (vec entries) index))]
            (assoc model :entries (assoc entries pkid representation)))))

      :assoc-pkids
      (let [pkids (effective-pkids operation-form)]
        {:entries (reduce (fn [entries pkid]
                            (assoc entries pkid value))
                          entries
                          pkids)
         :next-pkid (reduce (fn [^long next-pkid pkid]
                              (max next-pkid (inc (long pkid))))
                            next-pkid
                            pkids)
         :meta (:meta model)})

      :dissoc-pkids
      (assoc model :entries (reduce dissoc entries (effective-pkids operation-form)))

      :pop
      (if (coll/empty? entries)
        model
        (assoc model :entries (dissoc entries (key (first (rseq entries))))))

      :empty
      {:entries (sorted-map)
       :next-pkid 0
       :meta (:meta model)}

      :with-meta
      (assoc model :meta arg))))

(defn- apply-pkid-vector-operation [pkid-vector [operation arg representation value]]
  (case operation
    :conj (conj pkid-vector arg)
    :assoc-index (assoc pkid-vector (mod (long arg) (inc (count pkid-vector))) representation)
    :assoc-pkids (pkid-vector/assoc-pkids pkid-vector
                                          (represented-pkids arg representation)
                                          value)
    :dissoc-pkids (pkid-vector/dissoc-pkids pkid-vector
                                            (represented-pkids arg representation))
    :pop (if (coll/empty? pkid-vector) pkid-vector (pop pkid-vector))
    :empty (empty pkid-vector)
    :with-meta (with-meta pkid-vector arg)))

(defn- model-pkids [entries value]
  (coll/into-> entries (int-map/int-set)
    (keep (fn [[pkid entry-value]]
            (when (= value entry-value)
              pkid)))))

(defn- model-missing-pkids [{:keys [entries next-pkid]}]
  (coll/into-> (range next-pkid) []
    (remove #(contains? entries %))))

(defn- pkid-vector-matches-model? [{:keys [entries next-pkid] :as model} pkid-vector]
  (let [missing (missing-pkids pkid-vector)]
    (and (instance? PkidVector pkid-vector)
         (= (coll/into-> entries [] (map val))
            pkid-vector)
         (= (:meta model) (meta pkid-vector))
         (= next-pkid
            (pkid-vector/next-pkid pkid-vector)
            (+ (count pkid-vector)
               (count missing)))
         (= (model-missing-pkids model) missing)
         (coll/every? (fn [value]
                        (= (vec (model-pkids entries value))
                           (pkid-vector/find-pkids pkid-vector value)))
                      (conj test-values :not-found)))))

(defn- operation-requires-identical-result? [{:keys [entries meta]} [operation arg :as operation-form]]
  (case operation
    :assoc-pkids (coll/empty? (effective-pkids operation-form))
    :dissoc-pkids (coll/every? #(not (contains? entries %))
                               (effective-pkids operation-form))
    :pop (coll/empty? entries)
    :with-meta (identical? meta arg)
    false))

(defn- throws? [exception-class f]
  (try
    (f)
    false
    (catch Throwable error
      (instance? exception-class error))))

(defn- vector-contract-matches-model? [{:keys [entries] :as model} pkid-vector]
  (let [values (coll/into-> entries [] (map val))
        value-count (count values)
        middle-index (quot value-count 2)
        subvector-start (quot value-count 4)
        subvector-end (- value-count subvector-start)
        subvector (subvec pkid-vector subvector-start subvector-end)
        expected-subvector (subvec values subvector-start subvector-end)]
    (and (vector? pkid-vector)
         (= pkid-vector values)
         (= values pkid-vector)
         (= (hash values) (hash pkid-vector))
         (= (peek values) (peek pkid-vector))
         (= (vec (rseq values)) (vec (rseq pkid-vector)))
         (= expected-subvector subvector)
         (= (:meta model) (meta pkid-vector))
         (identical? PkidVector/EMPTY (empty (with-meta pkid-vector nil)))
         (if (zero? value-count)
           (throws? IllegalStateException #(pop pkid-vector))
           (and (= (values middle-index)
                   (pkid-vector middle-index)
                   (get pkid-vector middle-index)
                   (nth pkid-vector middle-index))
                (= (assoc expected-subvector 0 :subvector-value)
                   (assoc subvector 0 :subvector-value)))))))

(defn- apply-operations [initial-model initial-pkid-vector operations]
  (reduce (fn [[model pkid-vector] operation]
            [(apply-model-operation model operation)
             (apply-pkid-vector-operation pkid-vector operation)])
          [initial-model initial-pkid-vector]
          operations))

;; Checks each edit and the resulting vector operations against a slot model.
(defspec operations-match-sorted-map-model 500
  (prop/for-all [operations (gen/vector operation-gen 0 100)]
    (loop [operation-index 0
           model {:entries (sorted-map)
                  :next-pkid 0}
           pkid-vector (pkid-vector/pkid-vector)]
      (if (= operation-index (count operations))
        (and (pkid-vector-matches-model? model pkid-vector)
             (vector-contract-matches-model? model pkid-vector))
        (let [operation (operations operation-index)
              new-model (apply-model-operation model operation)
              new-pkid-vector (apply-pkid-vector-operation pkid-vector operation)]
          (if (and (pkid-vector-matches-model? model pkid-vector)
                   (pkid-vector-matches-model? new-model new-pkid-vector)
                   (or (not (operation-requires-identical-result? model operation))
                       (identical? pkid-vector new-pkid-vector)))
            (recur (inc operation-index) new-model new-pkid-vector)
            false))))))

(defspec sibling-branches-remain-persistent 200
  (prop/for-all [prefix-operations (gen/vector operation-gen 0 50)
                 first-branch-operations (gen/vector operation-gen 0 25)
                 second-branch-operations (gen/vector operation-gen 0 25)]
    (let [initial-model {:entries (sorted-map)
                         :next-pkid 0}
          [base-model base-pkid-vector] (apply-operations initial-model
                                                          (pkid-vector/pkid-vector)
                                                          prefix-operations)
          [first-branch-model first-branch-pkid-vector] (apply-operations base-model
                                                                          base-pkid-vector
                                                                          first-branch-operations)
          [second-branch-model second-branch-pkid-vector] (apply-operations base-model
                                                                            base-pkid-vector
                                                                            second-branch-operations)]
      (and (pkid-vector-matches-model? base-model base-pkid-vector)
           (pkid-vector-matches-model? first-branch-model first-branch-pkid-vector)
           (pkid-vector-matches-model? second-branch-model second-branch-pkid-vector)))))

(defspec reducible-batches-are-traversed-once 200
  (prop/for-all [pkids pkid-batch-gen
                 value (gen/elements test-values)]
    (let [assoc-visit-count (atom 0)
          assoc-pkids (eduction (map (fn [pkid]
                                       (swap! assoc-visit-count inc)
                                       pkid))
                                pkids)
          associated-pkid-vector (pkid-vector/assoc-pkids (pkid-vector/pkid-vector)
                                                          assoc-pkids
                                                          value)
          dissoc-visit-count (atom 0)
          dissoc-pkids (eduction (map (fn [pkid]
                                        (swap! dissoc-visit-count inc)
                                        pkid))
                                 pkids)
          dissociated-pkid-vector (pkid-vector/dissoc-pkids associated-pkid-vector dissoc-pkids)]
      (and (= (count pkids) @assoc-visit-count @dissoc-visit-count)
           (= [] dissociated-pkid-vector)
           (= (reduce (fn [^long next-pkid pkid]
                        (max next-pkid (inc (long pkid))))
                      0
                      pkids)
              (pkid-vector/next-pkid dissociated-pkid-vector))))))

(defspec invalid-pkids-are-rejected 200
  (prop/for-all [negative-pkid (gen/choose -100000 -1)
                 invalid-pkid (gen/elements [nil :invalid (int 1) 1.0])]
    (let [pkid-vector (pkid-vector/pkid-vector)
          invalid-pkid-exception-class (if (nil? invalid-pkid)
                                         NullPointerException
                                         ClassCastException)]
      (and (throws? IllegalArgumentException
                    #(pkid-vector/assoc-pkids pkid-vector [negative-pkid] :value))
           (throws? IllegalArgumentException
                    #(pkid-vector/dissoc-pkids pkid-vector [negative-pkid]))
           (throws? invalid-pkid-exception-class
                    #(pkid-vector/assoc-pkids pkid-vector [invalid-pkid] :value))
           (throws? invalid-pkid-exception-class
                    #(pkid-vector/dissoc-pkids pkid-vector [invalid-pkid]))))))

;; Checks that slots restored near a distant anchor keep their IDs.
(defspec restored-slots-remain-readable 50
  (prop/for-all [leaf-index (gen/choose 1 8)
                 value (gen/elements test-values)]
    (let [leaf-index (long leaf-index)
          anchor-pkid (inc (* 128 leaf-index))
          restored-pkid (dec anchor-pkid)
          pkid-vector (-> (pkid-vector/pkid-vector)
                          (pkid-vector/assoc-pkids (int-map/int-set [anchor-pkid]) :anchor)
                          (pkid-vector/assoc-pkids (int-map/int-set [restored-pkid]) value))
          model {:entries (sorted-map restored-pkid value anchor-pkid :anchor)
                 :next-pkid (inc anchor-pkid)}]
      (pkid-vector-matches-model? model pkid-vector))))

;; Checks that edits around tree boundaries preserve slot order and the next ID.
(defspec persistent-vector-boundaries-preserve-pkid-state 50
  (prop/for-all [size (gen/elements [0 1 31 32 33 1023 1024 1025 32767 32768 32769])
                 raw-index gen/nat
                 metadata (gen/elements test-metadata)]
    (let [size (long size)
          values (vec (range size))
          initial-pkid-vector (with-meta (reduce conj (pkid-vector/pkid-vector) values)
                                metadata)
          index (mod (long raw-index) (inc size))
          associated-pkid-vector (assoc initial-pkid-vector index :associated)
          expected-associated-values (assoc values index :associated)
          appended-pkid-vector (conj associated-pkid-vector :appended)
          appended-pkid (pkid-vector/next-pkid associated-pkid-vector)
          popped-pkid-vector (pop appended-pkid-vector)]
      (and (= expected-associated-values associated-pkid-vector popped-pkid-vector)
           (= (conj expected-associated-values :appended) appended-pkid-vector)
           (= metadata
              (meta initial-pkid-vector)
              (meta associated-pkid-vector)
              (meta appended-pkid-vector)
              (meta popped-pkid-vector))
           (= (if (= index size) (inc size) size) appended-pkid)
           (= (inc appended-pkid)
              (pkid-vector/next-pkid appended-pkid-vector)
              (pkid-vector/next-pkid popped-pkid-vector))
           (= [appended-pkid] (missing-pkids popped-pkid-vector))))))

(def ^:private values [nil false :a :b 0 1])
(def ^:private value-gen (gen/elements values))

(def ^:private sparse-pkid-gen
  (gen/frequency [[6 (gen/choose 0 160)]
                  [3 (gen/choose 0 50000)]
                  [1 (gen/choose 0 (dec Long/MAX_VALUE))]]))

(def ^:private sparse-operation-gen
  (gen/one-of
    [(gen/tuple (gen/return :append) value-gen)
     (gen/tuple (gen/return :assoc) (gen/vector sparse-pkid-gen 0 16) value-gen)
     (gen/tuple (gen/return :remove) (gen/vector sparse-pkid-gen 0 16))
     (gen/tuple (gen/return :remove-live) gen/nat)
     (gen/tuple (gen/return :replace-live) gen/nat value-gen)
     (gen/tuple (gen/return :restore) gen/nat value-gen)
     (gen/tuple (gen/return :clear))]))

(def ^:private empty-model {:slots {} :deleted [] :next-pkid (long 0)})

(defn- model-entries [model]
  (coll/sort-by key (:slots model)))

(defn- matching-slots [entries value]
  (coll/into-> entries [] (keep (fn [[id stored]] (when (= value stored) id)))))

(defn- selected-slots [entries selected-values]
  (reduce (fn [result [id value]]
            (if (contains? selected-values value)
              (update result value coll/conj-vector id)
              result))
          {}
          entries))

(defn- iterator-matches? [table expected]
  (let [iterator (RT/iter table)]
    (and (coll/every? (fn [value]
                        (and (.hasNext iterator) (= value (.next iterator))))
                      expected)
         (not (.hasNext iterator))
         (try
           (.next iterator)
           false
           (catch NoSuchElementException _
             true)))))

(defn- matches-model? [model ^PkidVector table selected-values]
  (let [entries (model-entries model)
        expected (coll/into-> entries [] (map val))
        size (count expected)
        missing (Object.)]
    (and (= (:next-pkid model) (.-nextPkid table))
         (= size (count table))
         (= expected table)
         (= table expected)
         (= (hash expected) (hash table))
         (= (.hashCode ^Object expected) (.hashCode table))
         (= expected (into [] table))
         (= expected (into [] (seq table)))
         (iterator-matches? table expected)
         (= (first expected) (first table))
         (identical? missing (nth table size missing))
         (coll/every? (fn [index] (= (nth expected index) (nth table index)))
                      (range size))
         (= (selected-slots entries selected-values) (.findPkidsByValue table selected-values))
         (coll/every? (fn [value] (= (matching-slots entries value) (.findPkids table value)))
                      values)
         (= (first expected)
            (reduce (fn [_ value] (reduced value)) nil table)))))

(defn- next-state [[{:keys [slots next-pkid deleted] :as model} ^PkidVector table]
                   [operation arg value]]
  (case operation
    :append
    [(assoc model :slots (assoc slots next-pkid arg) :next-pkid (inc (long next-pkid)))
     (conj table arg)]

    :assoc
    [(assoc model
       :slots (reduce #(assoc %1 %2 value) slots arg)
       :next-pkid (reduce (fn [^long next-pkid id] (max next-pkid (inc (long id))))
                          next-pkid arg))
     (.assocPkids table arg value)]

    :remove
    [(assoc model
       :slots (reduce dissoc slots arg)
       :deleted (coll/into-> arg deleted (filter #(contains? slots %))))
     (.dissocPkids table arg)]

    :remove-live
    (if (coll/empty? slots)
      [model table]
      (let [id (key (nth (model-entries model) (mod (long arg) (count slots))))]
        [(assoc model :slots (dissoc slots id) :deleted (conj deleted id))
         (.dissocPkids table [(long id)])]))

    :replace-live
    (if (coll/empty? slots)
      [model table]
      (let [id (key (nth (model-entries model) (mod (long arg) (count slots))))]
        [(assoc model :slots (assoc slots id value))
         (.assocPkids table [(long id)] value)]))

    :restore
    (let [missing (coll/into-> deleted [] (remove #(contains? slots %)))]
      (if (coll/empty? missing)
        [model table]
        (let [id (nth missing (mod (long arg) (count missing)))]
          [(assoc model :slots (assoc slots id value))
           (.assocPkids table [(long id)] value)])))

    :clear
    [empty-model (empty table)]))

(defn- states-after [initial-state operations]
  (reduce (fn [states operation]
            (conj states (next-state (peek states) operation)))
          [initial-state]
          operations))

;; Checks every edit and both forks against an independent slot model.
(defspec updates-and-snapshots-match-slot-model 250
  (prop/for-all [initial (gen/vector value-gen 0 260)
                 prefix (gen/vector sparse-operation-gen 0 40)
                 left (gen/vector sparse-operation-gen 0 30)
                 right (gen/vector sparse-operation-gen 0 30)
                 selected (gen/set value-gen)]
    (let [initial-state [{:slots (coll/into-> initial {} (map-indexed vector))
                          :deleted []
                          :next-pkid (long (count initial))}
                         (reduce conj PkidVector/EMPTY initial)]
          prefix-states (states-after initial-state prefix)
          fork (peek prefix-states)
          states (coll/into-> [(states-after fork left) (states-after fork right)] prefix-states cat)]
      (coll/every? (fn [[model table]] (matches-model? model table selected)) states))))

;; Exercises batch ingestion through counted, iterable, and reducible inputs.
(defspec batches-consume-each-input-once 150
  (prop/for-all [ids (gen/vector sparse-pkid-gen 0 160)
                 value value-gen
                 representation (gen/elements [:vector :iterable :int-set :reducible])]
    (let [seen (volatile! 0)
          batch (case representation
                  :vector ids
                  :iterable (ArrayList. ^Collection ids)
                  :int-set (int-map/int-set ids)
                  :reducible (eduction (map (fn record-id [id] (vswap! seen #(inc (long %))) id)) ids))
          expected-ids (coll/into-> (coll/sort ids) [] (distinct))
          inserted (.assocPkids PkidVector/EMPTY batch value)
          after (.dissocPkids inserted batch)]
      (and (or (not= :reducible representation) (= (* 2 (count ids)) @seen))
           (= expected-ids (.findPkids inserted value))
           (= (count expected-ids) (count inserted))
           (zero? (count after))
           (= (.-nextPkid inserted) (.-nextPkid after))
           (identical? after (.dissocPkids after batch))))))

;; Checks lookup with a set whose comparator equates different stored values.
(defspec batch-lookup-uses-the-supplied-set 150
  (prop/for-all [stored (gen/vector gen/string-alphanumeric 0 200)
                 requested (gen/vector gen/string-alphanumeric 0 40)]
    (let [selected (into (sorted-set-by (fn [a b] (compare (count a) (count b)))) requested)
          entries (coll/into-> stored [] (map-indexed vector))
          ^PkidVector table (reduce conj PkidVector/EMPTY stored)]
      (= (selected-slots entries selected)
         (.findPkidsByValue table selected)))))

;; Checks ordered reads and value lookup before and after edits to a wide table.
(defspec wide-lookups-match-the-slot-model 100
  (prop/for-all [size (gen/choose 200 6000)
                 raw-ids (gen/vector gen/nat 0 64)
                 step (gen/choose 11 97)
                 raw-request (gen/vector gen/nat 0 48)
                 repeats (gen/choose 2 16)]
    (let [size (long size)
          step (long step)
          value-count (quot size (long repeats))
          stored (mapv #(mod (long %) value-count) (range size))
          removed-ids (coll/into-> raw-ids #{} (map #(mod (* step (long %)) size)))
          entries (coll/into-> stored {} (map-indexed vector))
          survivors (reduce dissoc entries removed-ids)
          ^PkidVector before (reduce conj PkidVector/EMPTY stored)
          requested (coll/into-> raw-request #{0 value-count}
                      (map #(mod (* step (long %)) value-count)))
          requested-indexes (coll/into-> raw-request []
                              (map #(mod (* step (long %)) (count survivors))))

          check-lookups
          (fn check-lookups [model ^PkidVector table]
            (let [entries (coll/sort-by key model)
                  expected (selected-slots entries requested)]
              (and (= expected (.findPkidsByValue table requested))
                   (coll/every? (fn [value]
                                  (let [selected #{value}]
                                    (and (= (matching-slots entries value)
                                            (.findPkids table value))
                                         (= (select-keys expected selected)
                                            (.findPkidsByValue table selected)))))
                                requested)
                   (= expected (.findPkidsByValue table requested))
                   (= expected (.findPkidsByValue table (conj requested :absent)))
                   (coll/every? (fn [index]
                                  (= (val (nth entries index))
                                     (nth table index)))
                                requested-indexes))))]
      (and (check-lookups entries before)
           (let [^PkidVector after (.dissocPkids before removed-ids)]
             (and (check-lookups survivors after)
                  (check-lookups entries before)))))))

;; Checks lookup results and ordering across a fork of a wide, sparse table.
(defspec wide-sparse-forks-match-the-slot-model 25
  (prop/for-all [size (gen/choose 12000 110000)
                 offset (gen/one-of [(gen/return 0)
                                     (gen/choose 0 (- Long/MAX_VALUE 110000))])
                 distinct-count (gen/choose 1 1000)
                 edits (gen/vector (gen/choose 0 110000) 1 48)]
    (let [size (long size)
          offset (long offset)
          distinct-count (long distinct-count)
          ^PkidVector start (if (zero? offset)
                              PkidVector/EMPTY
                              (.dissocPkids (.assocPkids PkidVector/EMPTY [(dec offset)] nil) [(dec offset)]))

          ^PkidVector original
          (coll/reduce-> (range size) start
            (fn [table value]
              (conj table (mod (long value) distinct-count))))

          edited-slots (coll/into-> edits #{} (map #(mod (long %) size)))
          queries (coll/into-> edits (conj #{0} (dec distinct-count) distinct-count)
                    (map #(mod (long %) distinct-count)))
          ids (coll/into-> edited-slots #{} (map #(+ offset (long %))))

          matches-before
          (coll/reduce-> (range size) {}
            (fn [result slot]
              (let [slot (long slot)
                    value (mod slot distinct-count)]
                (if (contains? queries value)
                  (update result value coll/conj-vector (+ offset slot))
                  result))))

          matches-after
          (coll/into-> matches-before {}
            (keep (fn [[value matches]]
                    (when-let [remaining (coll/not-empty (coll/into-> matches [] (remove ids)))]
                      [value remaining]))))

          check-lookup
          (fn check-lookup [^PkidVector table expected]
            (and (coll/every? (fn [value] (= (get expected value []) (.findPkids table value))) queries)
                 (= expected (.findPkidsByValue table queries))))]
      (and (= size (count original))
           (check-lookup original matches-before)
           (let [^PkidVector after (.dissocPkids original ids)]
             (and (= (- size (count ids)) (count after))
                  (check-lookup after matches-after)
                  (check-lookup original matches-before)))))))

;; Checks concurrent lookup while multiple forks share parts of a wide table.
(defspec concurrent-lookups-match-each-fork 25
  (prop/for-all [size (gen/choose 8000 42000)
                 edits (gen/vector (gen/choose 0 42000) 2 24)]
    (let [size (long size)
          ^PkidVector table (reduce conj PkidVector/EMPTY (range size))
          slots (mapv #(mod (long %) size) edits)
          removed (set slots)
          removed-in-order (into [] (coll/sort removed))
          ^PkidVector after (.dissocPkids table slots)
          ^PkidVector restored (.assocPkids after slots :restored)
          gate (CountDownLatch. 1)
          pool (Executors/newFixedThreadPool 3 (-> (Thread/ofPlatform) (.daemon true) .factory))]
      (try
        (let [jobs (mapv (fn [[^PkidVector fork replaced-slots restored]]
                           (.submit pool
                                    ^Callable
                                    (fn verify-fork []
                                      (.await gate)
                                      (let [expected
                                            (reduce (fn [matches slot]
                                                      (if (contains? replaced-slots slot)
                                                        matches
                                                        (assoc matches slot [slot])))
                                                    {}
                                                    slots)]
                                        (and (= (if restored removed-in-order [])
                                                (.findPkids fork :restored))
                                             (coll/every? (fn [slot]
                                                            (= (get expected slot []) (.findPkids fork slot)))
                                                          slots)
                                             (= expected (.findPkidsByValue fork removed)))))))
                         [[table #{} false]
                          [after removed false]
                          [restored removed true]])]
          (.countDown gate)
          (coll/every? deref jobs))
        (finally
          (.shutdownNow pool))))))

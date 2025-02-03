;; Copyright 2020-2025 The Defold Foundation
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

(ns util.weak-interner-test
  (:require [clojure.test :refer :all]
            [util.coll :refer [pair]])
  (:import [com.defold.util WeakInterner]))

(deftest constructor-test
  (is (some? (WeakInterner. 16)))
  (is (some? (WeakInterner. 16, 0.75)))
  (is (thrown? IllegalArgumentException (WeakInterner. -1)))
  (is (thrown? IllegalArgumentException (WeakInterner. Integer/MAX_VALUE)))
  (is (thrown? IllegalArgumentException (WeakInterner. 16 -1.0)))
  (is (thrown? IllegalArgumentException (WeakInterner. 16 Float/NaN))))

(deftest interning-test
  (let [weak-interner (WeakInterner. 16)
        interned-value (.intern weak-interner (pair 12345 :label))]
    (is (= interned-value (pair 12345 :label)))
    (is (identical? interned-value (.intern weak-interner (pair 12345 :label))))))

(deftest growth-test
  (let [weak-interner (WeakInterner. 0)
        first-value (.intern weak-interner (pair 1 :first))
        second-value (.intern weak-interner (pair 2 :second))
        third-value (.intern weak-interner (pair 3 :third))]
    (is (identical? first-value (.intern weak-interner (pair 1 :first))))
    (is (identical? second-value (.intern weak-interner (pair 2 :second))))
    (is (identical? third-value (.intern weak-interner (pair 3 :third))))))

(deftest cleanup-test
  (let [weak-interner (WeakInterner. 4)
        references-atom (atom {})]
    (swap! references-atom assoc :first-value (.intern weak-interner (pair 1 :first)))
    (swap! references-atom assoc :second-value (.intern weak-interner (pair 2 :second)))
    (let [{:keys [first-value second-value]} @references-atom
          first-id (System/identityHashCode first-value)
          second-id (System/identityHashCode second-value)]
      (is (= first-id (System/identityHashCode (.intern weak-interner (pair 1 :first)))))
      (is (= second-id (System/identityHashCode (.intern weak-interner (pair 2 :second)))))
      (reset! references-atom {})
      (System/gc)
      (is (not= first-id (System/identityHashCode (.intern weak-interner (pair 1 :first)))))
      (is (not= second-id (System/identityHashCode (.intern weak-interner (pair 2 :second))))))))

(deftest multi-threaded-access-test
  (let [weak-interner (WeakInterner. 0)

        interned-values-by-node-id
        (->> (repeatedly 1000000 #(long (rand-int 100)))
             (pmap #(.intern weak-interner (pair % :label)))
             (vec)
             (group-by key))]

    (doseq [[first-value & remaining-values] (vals interned-values-by-node-id)]
      (is (every? #(identical? first-value %) remaining-values)))))

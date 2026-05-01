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

(ns util.weak-interner-test
  (:require [clojure.test :refer :all]
            [internal.graph.types :as gt])
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
        interned-endpoint (.intern weak-interner (gt/->Endpoint 12345 :label))]
    (is (= interned-endpoint (gt/->Endpoint 12345 :label)))
    (is (identical? interned-endpoint (.intern weak-interner (gt/->Endpoint 12345 :label))))))

(deftest growth-test
  (let [weak-interner (WeakInterner. 0)
        first-endpoint (.intern weak-interner (gt/->Endpoint 1 :first))
        second-endpoint (.intern weak-interner (gt/->Endpoint 2 :second))
        third-endpoint (.intern weak-interner (gt/->Endpoint 3 :third))]
    (is (identical? first-endpoint (.intern weak-interner (gt/->Endpoint 1 :first))))
    (is (identical? second-endpoint (.intern weak-interner (gt/->Endpoint 2 :second))))
    (is (identical? third-endpoint (.intern weak-interner (gt/->Endpoint 3 :third))))))

(deftest cleanup-test
  (let [weak-interner (WeakInterner. 4)
        references-atom (atom {})]
    (swap! references-atom assoc :first-endpoint (.intern weak-interner (gt/->Endpoint 1 :first)))
    (swap! references-atom assoc :second-endpoint (.intern weak-interner (gt/->Endpoint 2 :second)))
    (let [{:keys [first-endpoint second-endpoint]} @references-atom
          first-id (System/identityHashCode first-endpoint)
          second-id (System/identityHashCode second-endpoint)]
      (is (= first-id (System/identityHashCode (.intern weak-interner (gt/->Endpoint 1 :first)))))
      (is (= second-id (System/identityHashCode (.intern weak-interner (gt/->Endpoint 2 :second)))))
      (reset! references-atom {})
      (System/gc)
      (is (not= first-id (System/identityHashCode (.intern weak-interner (gt/->Endpoint 1 :first)))))
      (is (not= second-id (System/identityHashCode (.intern weak-interner (gt/->Endpoint 2 :second))))))))

(deftest multi-threaded-access-test
  (let [weak-interner (WeakInterner. 0)

        interned-endpoints-by-node-id
        (->> (repeatedly 1000000 #(long (rand-int 100)))
             (pmap #(.intern weak-interner (gt/->Endpoint % :label)))
             (vec)
             (group-by gt/endpoint-node-id))]

    (doseq [[first-endpoint & remaining-endpoints] (vals interned-endpoints-by-node-id)]
      (is (every? #(identical? first-endpoint %) remaining-endpoints)))))

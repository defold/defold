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

(ns internal.graph.types-test
  (:require [clojure.test :refer :all]
            [internal.graph.types :as gt]))

(deftest graph-associative-behavior
  (let [nodes (Object.)
        sarcs (Object.)
        successors (Object.)
        tarcs (Object.)
        tx-id (Object.)
        graph-values (Object.)
        overrides (Object.)
        node->overrides (Object.)
        graph (gt/->Graph nodes sarcs successors tarcs tx-id graph-values overrides node->overrides)]
    (testing "field lookup"
      (is (identical? nodes (:nodes graph)))
      (is (identical? sarcs (:sarcs graph)))
      (is (identical? successors (:successors graph)))
      (is (identical? tarcs (:tarcs graph)))
      (is (identical? tx-id (:tx-id graph)))
      (is (identical? graph-values (:graph-values graph)))
      (is (identical? overrides (:overrides graph)))
      (is (identical? node->overrides (:node->overrides graph)))
      (is (= ::not-found (get graph ::unknown ::not-found)))
      (is (contains? graph :nodes))
      (is (not (contains? graph ::unknown)))
      (is (= :nodes (key (find graph :nodes))))
      (is (identical? nodes (val (find graph :nodes)))))

    (testing "association"
      (are [key value] (identical? graph (assoc graph key value))
        :nodes nodes
        :sarcs sarcs
        :successors successors
        :tarcs tarcs
        :tx-id tx-id
        :graph-values graph-values
        :overrides overrides
        :node->overrides node->overrides)

      (let [new-nodes (Object.)
            updated-graph (assoc graph :nodes new-nodes)]
        (is (not (identical? graph updated-graph)))
        (is (identical? new-nodes (:nodes updated-graph)))
        (is (identical? sarcs (:sarcs updated-graph))))

      (is (thrown? IllegalArgumentException
                   (assoc graph ::unknown nil))))

    (testing "equality and hashing"
      (let [equal-graph (gt/->Graph nodes sarcs successors tarcs tx-id graph-values overrides node->overrides)
            different-graph (assoc graph :nodes (Object.))]
        (is (= graph equal-graph))
        (is (= (hash graph) (hash equal-graph)))
        (is (= (.hashCode graph) (.hashCode equal-graph)))
        (is (not= graph different-graph))))

    (testing "unsupported persistent collection operations"
      (is (thrown? UnsupportedOperationException (seq graph)))
      (is (thrown? UnsupportedOperationException (count graph)))
      (is (thrown? UnsupportedOperationException (conj graph [:nodes nodes])))
      (is (thrown? UnsupportedOperationException (empty graph))))))

(deftest endpoint-comparable
  (is (thrown? NullPointerException
               (.compareTo nil
                           (gt/endpoint 0 :a))))
  (is (thrown? NullPointerException
               (.compareTo (gt/endpoint 0 :a)
                           nil)))
  (is (thrown? ClassCastException
               (.compareTo ""
                           (gt/endpoint 0 :a))))
  (is (thrown? ClassCastException
               (.compareTo (gt/endpoint 0 :a)
                           "")))

  (is (neg? (.compareTo (gt/endpoint 0 :a)
                        (gt/endpoint 1 :a))))
  (is (zero? (.compareTo (gt/endpoint 0 :a)
                         (gt/endpoint 0 :a))))
  (is (pos? (.compareTo (gt/endpoint 1 :a)
                        (gt/endpoint 0 :a))))
  (is (neg? (.compareTo (gt/endpoint 1 :a)
                        (gt/endpoint 1 :b))))
  (is (zero? (.compareTo (gt/endpoint 1 :a)
                         (gt/endpoint 1 :a))))
  (is (pos? (.compareTo (gt/endpoint 1 :b)
                        (gt/endpoint 1 :a))))

  (is (= [(gt/endpoint 0 :a)
          (gt/endpoint 0 :b)
          (gt/endpoint 1 :a)
          (gt/endpoint 1 :b)]
         (vec (into (sorted-set)
                    [(gt/endpoint 1 :b)
                     (gt/endpoint 0 :b)
                     (gt/endpoint 1 :a)
                     (gt/endpoint 0 :a)
                     (gt/endpoint 1 :b)
                     (gt/endpoint 0 :b)
                     (gt/endpoint 1 :a)
                     (gt/endpoint 0 :a)])))))

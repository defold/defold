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

(ns internal.txsteps.override-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(deftest override-node-creation-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id helpers/OverrideTestNode
                 directly-owned-node-id helpers/OverrideTestNode
                 indirectly-owned-node-id helpers/OverrideTestNode]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (testing "Nodes."
                (is (coll/empty? (g/overrides basis owner-node-id)))
                (is (coll/empty? (g/overrides basis directly-owned-node-id)))
                (is (coll/empty? (g/overrides basis indirectly-owned-node-id)))
                (is (= #{owner-node-id
                         directly-owned-node-id
                         indirectly-owned-node-id}
                       (set (g/node-ids graph)))))

              (testing "Explicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (ig/explicit-sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (ig/explicit-sources basis directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Implicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (g/targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (g/targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (g/sources basis directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Internal arc tables."
                (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                       (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                       (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])
                  [first-order-override-owner-node-id :as overrides-of-owner-node-id] (g/overrides basis owner-node-id)
                  [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                  [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)]
              (is (= 1 (count overrides-of-owner-node-id)))
              (is (= 1 (count overrides-of-directly-owned-node-id)))
              (is (= 1 (count overrides-of-indirectly-owned-node-id)))
              (is (g/node-id? first-order-override-owner-node-id))
              (is (g/node-id? first-order-override-directly-owned-node-id))
              (is (g/node-id? first-order-override-indirectly-owned-node-id))

              (testing "Nodes."
                (is (= owner-node-id (g/override-original basis first-order-override-owner-node-id)))
                (is (= directly-owned-node-id (g/override-original basis first-order-override-directly-owned-node-id)))
                (is (= indirectly-owned-node-id (g/override-original basis first-order-override-indirectly-owned-node-id)))
                (is (= #{owner-node-id
                         directly-owned-node-id
                         indirectly-owned-node-id
                         first-order-override-owner-node-id
                         first-order-override-directly-owned-node-id
                         first-order-override-indirectly-owned-node-id}
                       (set (g/node-ids graph)))))

              (testing "Explicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (ig/explicit-sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (ig/explicit-sources basis directly-owned-node-id :regular-cascade-delete-input)))
                (is (= [] (ig/explicit-targets basis first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [] (ig/explicit-targets basis first-order-override-indirectly-owned-node-id :property-output)))
                (is (= [] (ig/explicit-sources basis first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (= [] (ig/explicit-sources basis first-order-override-directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Implicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (g/targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (g/targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (g/sources basis directly-owned-node-id :regular-cascade-delete-input)))
                (is (= [[first-order-override-owner-node-id :regular-cascade-delete-input]] (g/targets basis first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[first-order-override-directly-owned-node-id :regular-cascade-delete-input]] (g/targets basis first-order-override-indirectly-owned-node-id :property-output)))
                (is (= [[first-order-override-directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (= [[first-order-override-indirectly-owned-node-id :property-output]] (g/sources basis first-order-override-directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Internal arc tables."
                (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                       (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                       (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
                (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id first-order-override-indirectly-owned-node-id :property-output)))
                (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-directly-owned-node-id :regular-cascade-delete-input))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/override owner-node-id))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-limited-traversal-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          traverse-fn
          (g/make-override-traverse-fn
            (fn limited-override-traverse-fn [basis arc]
              (is (gt/basis? basis))
              (= :regular-cascade-delete-output (gt/source-label arc))))

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id helpers/OverrideTestNode
                 directly-owned-node-id helpers/OverrideTestNode
                 indirectly-owned-node-id helpers/OverrideTestNode]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (testing "Nodes."
                (is (nil? (g/overrides basis owner-node-id)))
                (is (nil? (g/overrides basis directly-owned-node-id)))
                (is (nil? (g/overrides basis indirectly-owned-node-id)))
                (is (= #{owner-node-id
                         directly-owned-node-id
                         indirectly-owned-node-id}
                       (set (g/node-ids graph)))))

              (testing "Explicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (ig/explicit-sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (ig/explicit-sources basis directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Implicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (g/targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (g/targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (g/sources basis directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Internal arc tables."
                (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                       (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                       (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])
                  [first-order-override-owner-node-id :as overrides-of-owner-node-id] (g/overrides basis owner-node-id)
                  [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                  overrides-of-indirectly-owned-node-id (g/overrides basis indirectly-owned-node-id)]
              (is (= 1 (count overrides-of-owner-node-id)))
              (is (= 1 (count overrides-of-directly-owned-node-id)))
              (is (= 0 (count overrides-of-indirectly-owned-node-id)))
              (is (g/node-id? first-order-override-owner-node-id))
              (is (g/node-id? first-order-override-directly-owned-node-id))

              (testing "Nodes."
                (is (= owner-node-id (g/override-original basis first-order-override-owner-node-id)))
                (is (= directly-owned-node-id (g/override-original basis first-order-override-directly-owned-node-id)))
                (is (= #{owner-node-id
                         directly-owned-node-id
                         indirectly-owned-node-id
                         first-order-override-owner-node-id
                         first-order-override-directly-owned-node-id}
                       (set (g/node-ids graph)))))

              (testing "Explicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input]] (ig/explicit-targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (ig/explicit-sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (ig/explicit-sources basis directly-owned-node-id :regular-cascade-delete-input)))
                (is (= [] (ig/explicit-targets basis first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [] (ig/explicit-sources basis first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (= [] (ig/explicit-sources basis first-order-override-directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Implicit connections."
                (is (= [[owner-node-id :regular-cascade-delete-input]] (g/targets basis directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-input] [first-order-override-directly-owned-node-id :regular-cascade-delete-input]] (g/targets basis indirectly-owned-node-id :property-output)))
                (is (= [[directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (g/sources basis directly-owned-node-id :regular-cascade-delete-input)))
                (is (= [[first-order-override-owner-node-id :regular-cascade-delete-input]] (g/targets basis first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (= [[first-order-override-directly-owned-node-id :regular-cascade-delete-output]] (g/sources basis first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output]] (g/sources basis first-order-override-directly-owned-node-id :regular-cascade-delete-input))))

              (testing "Internal arc tables."
                (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                       (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
                (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                       (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                       (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
                (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id first-order-override-directly-owned-node-id :regular-cascade-delete-output)))
                (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-owner-node-id :regular-cascade-delete-input)))
                (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-directly-owned-node-id :regular-cascade-delete-input))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/override owner-node-id {:traverse-fn traverse-fn}))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-init-props-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id [helpers/OverrideTestNode :property :owner-property-value]
                 directly-owned-node-id [helpers/OverrideTestNode :property :directly-owned-property-value]
                 indirectly-owned-node-id [helpers/OverrideTestNode :property :indirectly-owned-property-value]]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :regular-cascade-delete-output directly-owned-node-id :regular-cascade-delete-input))))

          original-property-value->overridden-property-value
          {:owner-property-value :first-order-override-owner-property-value
           :directly-owned-property-value :first-order-override-directly-owned-property-value
           :indirectly-owned-property-value :first-order-override-indirectly-owned-property-value}

          init-props-fn
          (fn init-props-fn [basis original-node-id node-type]
            (is (gt/basis? basis))
            (is (g/node-id? original-node-id))
            (is (= helpers/OverrideTestNode node-type))
            (is (= helpers/OverrideTestNode (g/node-type* basis original-node-id)))
            (let [original-property-value (g/raw-property-value basis original-node-id :property)
                  overridden-property-value (original-property-value->overridden-property-value original-property-value)]
              (is (contains? original-property-value->overridden-property-value original-property-value))
              {:property overridden-property-value}))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (coll/empty? (g/overrides basis owner-node-id)))
                (is (coll/empty? (g/overrides basis directly-owned-node-id)))
                (is (coll/empty? (g/overrides basis indirectly-owned-node-id))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)
                    [first-order-override-owner-node-id :as overrides-of-owner-node-id] (g/overrides basis owner-node-id)
                    [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                    [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)]
                (is (= 1 (count overrides-of-owner-node-id)))
                (is (= 1 (count overrides-of-directly-owned-node-id)))
                (is (= 1 (count overrides-of-indirectly-owned-node-id)))
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-owner-property-value
                       (g/raw-property-value basis first-order-override-owner-node-id :property)
                       (g/node-value first-order-override-owner-node-id :property-output evaluation-context)))
                (is (= :first-order-override-directly-owned-property-value
                       (g/raw-property-value basis first-order-override-directly-owned-node-id :property)
                       (g/node-value first-order-override-directly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-indirectly-owned-property-value
                       (g/raw-property-value basis first-order-override-indirectly-owned-node-id :property)
                       (g/node-value first-order-override-indirectly-owned-node-id :property-output evaluation-context))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/override owner-node-id {:init-props-fn init-props-fn}))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-properties-by-node-id-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id [helpers/OverrideTestNode :property :owner-property-value]
                 directly-owned-node-id [helpers/OverrideTestNode :property :directly-owned-property-value]
                 indirectly-owned-node-id [helpers/OverrideTestNode :property :indirectly-owned-property-value]]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :regular-cascade-delete-output directly-owned-node-id :regular-cascade-delete-input))))

          original-node-id->overridden-property-value
          {owner-node-id :first-order-override-owner-property-value
           directly-owned-node-id :first-order-override-directly-owned-property-value
           indirectly-owned-node-id :first-order-override-indirectly-owned-property-value}

          properties-by-node-id
          (fn properties-by-node-id [original-node-id]
            (is (g/node-id? original-node-id))
            (is (contains? original-node-id->overridden-property-value original-node-id))
            {:property (original-node-id->overridden-property-value original-node-id)})

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (coll/empty? (g/overrides basis owner-node-id)))
                (is (coll/empty? (g/overrides basis directly-owned-node-id)))
                (is (coll/empty? (g/overrides basis indirectly-owned-node-id))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)
                    [first-order-override-owner-node-id :as overrides-of-owner-node-id] (g/overrides basis owner-node-id)
                    [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                    [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)]
                (is (= 1 (count overrides-of-owner-node-id)))
                (is (= 1 (count overrides-of-directly-owned-node-id)))
                (is (= 1 (count overrides-of-indirectly-owned-node-id)))
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-owner-property-value
                       (g/raw-property-value basis first-order-override-owner-node-id :property)
                       (g/node-value first-order-override-owner-node-id :property-output evaluation-context)))
                (is (= :first-order-override-directly-owned-property-value
                       (g/raw-property-value basis first-order-override-directly-owned-node-id :property)
                       (g/node-value first-order-override-directly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-indirectly-owned-property-value
                       (g/raw-property-value basis first-order-override-indirectly-owned-node-id :property)
                       (g/node-value first-order-override-indirectly-owned-node-id :property-output evaluation-context))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/override owner-node-id {:properties-by-node-id properties-by-node-id}))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-init-fn-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id [helpers/OverrideTestNode :property :owner-property-value]
                 directly-owned-node-id [helpers/OverrideTestNode :property :directly-owned-property-value]
                 indirectly-owned-node-id [helpers/OverrideTestNode :property :indirectly-owned-property-value]]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :regular-cascade-delete-output directly-owned-node-id :regular-cascade-delete-input))))

          original-property-value->overridden-property-value
          {:owner-property-value :first-order-override-owner-property-value
           :directly-owned-property-value :first-order-override-directly-owned-property-value
           :indirectly-owned-property-value :first-order-override-indirectly-owned-property-value}

          init-fn
          (fn init-fn [evaluation-context original-node-id->override-node-id]
            (is (g/evaluation-context? evaluation-context))
            (is (map? original-node-id->override-node-id))
            (coll/into-> original-node-id->override-node-id []
              (mapcat
                (fn [[original-node-id override-node-id]]
                  (is (g/node-id? original-node-id))
                  (is (g/node-id? override-node-id))
                  (let [original-property-value (g/node-value original-node-id :property evaluation-context)
                        overridden-property-value (original-property-value->overridden-property-value original-property-value)]
                    (is (contains? original-property-value->overridden-property-value original-property-value))
                    (g/set-property override-node-id :property overridden-property-value))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (coll/empty? (g/overrides basis owner-node-id)))
                (is (coll/empty? (g/overrides basis directly-owned-node-id)))
                (is (coll/empty? (g/overrides basis indirectly-owned-node-id))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)
                    [first-order-override-owner-node-id :as overrides-of-owner-node-id] (g/overrides basis owner-node-id)
                    [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                    [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)]
                (is (= 1 (count overrides-of-owner-node-id)))
                (is (= 1 (count overrides-of-directly-owned-node-id)))
                (is (= 1 (count overrides-of-indirectly-owned-node-id)))
                (is (= :owner-property-value
                       (g/raw-property-value basis owner-node-id :property)
                       (g/node-value owner-node-id :property-output evaluation-context)))
                (is (= :directly-owned-property-value
                       (g/raw-property-value basis directly-owned-node-id :property)
                       (g/node-value directly-owned-node-id :property-output evaluation-context)))
                (is (= :indirectly-owned-property-value
                       (g/raw-property-value basis indirectly-owned-node-id :property)
                       (g/node-value indirectly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-owner-property-value
                       (g/raw-property-value basis first-order-override-owner-node-id :property)
                       (g/node-value first-order-override-owner-node-id :property-output evaluation-context)))
                (is (= :first-order-override-directly-owned-property-value
                       (g/raw-property-value basis first-order-override-directly-owned-node-id :property)
                       (g/node-value first-order-override-directly-owned-node-id :property-output evaluation-context)))
                (is (= :first-order-override-indirectly-owned-property-value
                       (g/raw-property-value basis first-order-override-indirectly-owned-node-id :property)
                       (g/node-value first-order-override-indirectly-owned-node-id :property-output evaluation-context))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/override owner-node-id {} init-fn))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-invalid-init-props-fn-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [_original-node-id [helpers/OverrideTestNode :property :original-property-value]])))

          ensure-unmodified!
          (fn ensure-unmodified! []
            (let [basis (g/now)]
              (is (= :original-property-value
                     (g/raw-property-value basis original-node-id :property)
                     (g/node-value original-node-id :property-output)))
              (is (coll/empty? (g/overrides basis original-node-id)))))]

      (testing "Before transaction attempt."
        (ensure-unmodified!))

      (testing "Transaction attempt fails schema validation."
        (is (thrown-with-msg?
              ExceptionInfo
              #"SCHEMA-VALIDATION"
              (binding [in/*suppress-schema-warnings* true]
                (g/transact
                  (g/override
                    original-node-id
                    {:init-props-fn (constantly {:property "invalid-non-keyword-value"})}))))))

      (testing "After transaction attempt."
        (ensure-unmodified!)))))

(deftest override-node-creation-with-invalid-properties-by-node-id-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [_original-node-id [helpers/OverrideTestNode :property :original-property-value]])))

          ensure-unmodified!
          (fn ensure-unmodified! []
            (let [basis (g/now)]
              (is (= :original-property-value
                     (g/raw-property-value basis original-node-id :property)
                     (g/node-value original-node-id :property-output)))
              (is (coll/empty? (g/overrides basis original-node-id)))))]

      (testing "Before transaction attempt."
        (ensure-unmodified!))

      (testing "Transaction attempt fails schema validation."
        (is (thrown-with-msg?
              ExceptionInfo
              #"SCHEMA-VALIDATION"
              (binding [in/*suppress-schema-warnings* true]
                (g/transact
                  (g/override
                    original-node-id
                    {:properties-by-node-id (constantly {:property "invalid-non-keyword-value"})}))))))

      (testing "After transaction attempt."
        (ensure-unmodified!)))))

(deftest override-node-creation-with-invalid-init-fn-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [_original-node-id [helpers/OverrideTestNode :property :original-property-value]])))

          init-fn
          (fn init-fn [_evaluation-context original-node-id->override-node-id]
            (g/set-property
              (original-node-id->override-node-id original-node-id)
              :property
              "invalid-non-keyword-value"))

          ensure-unmodified!
          (fn ensure-unmodified! []
            (let [basis (g/now)]
              (is (= :original-property-value
                     (g/raw-property-value basis original-node-id :property)
                     (g/node-value original-node-id :property-output)))
              (is (coll/empty? (g/overrides basis original-node-id)))))]

      (testing "Before transaction attempt."
        (ensure-unmodified!))

      (testing "Transaction attempt fails schema validation."
        (is (thrown-with-msg?
              ExceptionInfo
              #"SCHEMA-VALIDATION"
              (binding [in/*suppress-schema-warnings* true]
                (g/transact
                  (g/override original-node-id {} init-fn))))))

      (testing "After transaction attempt."
        (ensure-unmodified!)))))

(deftest undo-override-node-creation-invalidates-original-successors-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-node graph-id helpers/OverrideTestNode)))]

      (g/transact
        (g/override original-node-id))

      (let [[override-node-id] (g/overrides (g/now) original-node-id)]
        (is (= #{(g/endpoint override-node-id :property-output)}
               (set (g/successors (g/now) original-node-id :property-output))))

        (g/undo! :undo/global)

        (is (= #{}
               (set (g/successors (g/now) original-node-id :property-output))))))))

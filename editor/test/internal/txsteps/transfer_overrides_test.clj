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

(ns internal.txsteps.transfer-overrides-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(deftest override-transfer-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [indirectly-owned-node-id
           directly-owned-node-id
           owner-node-id
           first-order-override-owner-node-id
           second-order-override-owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [indirectly-owned-node-id helpers/OverrideTestNode
                 directly-owned-node-id helpers/OverrideTestNode
                 owner-node-id helpers/OverrideTestNode]
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id nil
                  (fn [_evaluation-context id-lookup]
                    (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                      (g/override first-order-override-owner-node-id)))))))

          [first-order-override-directly-owned-node-id
           first-order-override-indirectly-owned-node-id
           second-order-override-directly-owned-node-id
           second-order-override-indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)))

          [replacement-indirectly-owned-node-id
           replacement-directly-owned-node-id
           replacement-owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [replacement-indirectly-owned-node-id helpers/OverrideTestNode
                 replacement-directly-owned-node-id helpers/OverrideTestNode
                 replacement-owner-node-id helpers/OverrideTestNode]
                (g/connect replacement-indirectly-owned-node-id :property-output replacement-directly-owned-node-id :regular-cascade-delete-input)
                (g/connect replacement-directly-owned-node-id :regular-cascade-delete-output replacement-owner-node-id :regular-cascade-delete-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (is (= [first-order-override-owner-node-id] (g/overrides basis owner-node-id)))
              (is (coll/empty? (g/overrides basis replacement-owner-node-id)))
              (is (= [first-order-override-directly-owned-node-id] (g/overrides basis directly-owned-node-id)))
              (is (= [first-order-override-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)))
              (is (coll/empty? (g/overrides basis replacement-directly-owned-node-id)))
              (is (coll/empty? (g/overrides basis replacement-indirectly-owned-node-id)))
              (is (= [second-order-override-owner-node-id] (g/overrides basis first-order-override-owner-node-id)))
              (is (= [second-order-override-directly-owned-node-id] (g/overrides basis first-order-override-directly-owned-node-id)))
              (is (= [second-order-override-indirectly-owned-node-id] (g/overrides basis first-order-override-indirectly-owned-node-id)))
              (is (= owner-node-id (g/override-original basis first-order-override-owner-node-id)))
              (is (= directly-owned-node-id (g/override-original basis first-order-override-directly-owned-node-id)))
              (is (= indirectly-owned-node-id (g/override-original basis first-order-override-indirectly-owned-node-id)))
              (is (= owner-node-id (:root-id (ig/override-by-id basis (g/override-id basis first-order-override-owner-node-id)))))
              (is (= first-order-override-owner-node-id (g/override-original basis second-order-override-owner-node-id)))
              (is (= first-order-override-directly-owned-node-id (g/override-original basis second-order-override-directly-owned-node-id)))
              (is (= first-order-override-indirectly-owned-node-id (g/override-original basis second-order-override-indirectly-owned-node-id)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       replacement-indirectly-owned-node-id
                       replacement-directly-owned-node-id
                       replacement-owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id
                       first-order-override-directly-owned-node-id
                       second-order-override-directly-owned-node-id
                       first-order-override-indirectly-owned-node-id
                       second-order-override-indirectly-owned-node-id}
                     (into #{} (g/node-ids graph))))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[replacement-indirectly-owned-node-id :property-output replacement-directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id replacement-indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id replacement-directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)))
              (is (= [[replacement-directly-owned-node-id :regular-cascade-delete-output replacement-owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id replacement-directly-owned-node-id :regular-cascade-delete-output)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-target-arc-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
              (is (= [[replacement-directly-owned-node-id :regular-cascade-delete-output replacement-owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-target-arc-tuples basis graph-id replacement-owner-node-id :regular-cascade-delete-input)))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])
                  [first-order-override-replacement-directly-owned-node-id :as overrides-of-replacement-directly-owned-node-id] (g/overrides basis replacement-directly-owned-node-id)
                  [first-order-override-replacement-indirectly-owned-node-id :as overrides-of-replacement-indirectly-owned-node-id] (g/overrides basis replacement-indirectly-owned-node-id)
                  [second-order-override-replacement-directly-owned-node-id :as overrides-of-first-order-override-replacement-directly-owned-node-id] (g/overrides basis first-order-override-replacement-directly-owned-node-id)
                  [second-order-override-replacement-indirectly-owned-node-id :as overrides-of-first-order-override-replacement-indirectly-owned-node-id] (g/overrides basis first-order-override-replacement-indirectly-owned-node-id)]
              (is (coll/empty? (g/overrides basis owner-node-id)))
              (is (= [first-order-override-owner-node-id] (g/overrides basis replacement-owner-node-id)))
              (is (= [second-order-override-owner-node-id] (g/overrides basis first-order-override-owner-node-id)))
              (is (coll/empty? (g/overrides basis directly-owned-node-id)))
              (is (coll/empty? (g/overrides basis indirectly-owned-node-id)))
              (is (= 1 (count overrides-of-replacement-directly-owned-node-id)))
              (is (= 1 (count overrides-of-replacement-indirectly-owned-node-id)))
              (is (= 1 (count overrides-of-first-order-override-replacement-directly-owned-node-id)))
              (is (= 1 (count overrides-of-first-order-override-replacement-indirectly-owned-node-id)))
              (is (not= first-order-override-directly-owned-node-id first-order-override-replacement-directly-owned-node-id))
              (is (not= first-order-override-indirectly-owned-node-id first-order-override-replacement-indirectly-owned-node-id))
              (is (not= second-order-override-directly-owned-node-id second-order-override-replacement-directly-owned-node-id))
              (is (not= second-order-override-indirectly-owned-node-id second-order-override-replacement-indirectly-owned-node-id))
              (is (= replacement-owner-node-id (g/override-original basis first-order-override-owner-node-id)))
              (is (= replacement-owner-node-id (:root-id (ig/override-by-id basis (g/override-id basis first-order-override-owner-node-id)))))
              (is (= replacement-directly-owned-node-id (g/override-original basis first-order-override-replacement-directly-owned-node-id)))
              (is (= replacement-indirectly-owned-node-id (g/override-original basis first-order-override-replacement-indirectly-owned-node-id)))
              (is (= first-order-override-owner-node-id (g/override-original basis second-order-override-owner-node-id)))
              (is (= first-order-override-replacement-directly-owned-node-id (g/override-original basis second-order-override-replacement-directly-owned-node-id)))
              (is (= first-order-override-replacement-indirectly-owned-node-id (g/override-original basis second-order-override-replacement-indirectly-owned-node-id)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       replacement-indirectly-owned-node-id
                       replacement-directly-owned-node-id
                       replacement-owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id
                       first-order-override-replacement-directly-owned-node-id
                       second-order-override-replacement-directly-owned-node-id
                       first-order-override-replacement-indirectly-owned-node-id
                       second-order-override-replacement-indirectly-owned-node-id}
                     (into #{} (g/node-ids graph))))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[replacement-indirectly-owned-node-id :property-output replacement-directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id replacement-indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id replacement-directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)))
              (is (= [[replacement-directly-owned-node-id :regular-cascade-delete-output replacement-owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id replacement-directly-owned-node-id :regular-cascade-delete-output)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-target-arc-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
              (is (= [[replacement-directly-owned-node-id :regular-cascade-delete-output replacement-owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-target-arc-tuples basis graph-id replacement-owner-node-id :regular-cascade-delete-input)))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [basis-before (g/now)
              tx-result (g/transact
                          (g/transfer-overrides {owner-node-id replacement-owner-node-id}))
              basis-after (g/now)

              [first-order-override-replacement-directly-owned-node-id
               first-order-override-replacement-indirectly-owned-node-id
               second-order-override-replacement-directly-owned-node-id
               second-order-override-replacement-indirectly-owned-node-id
               :as created-node-ids]
              (g/tx-nodes-added tx-result)]

          (is (= 4 (count created-node-ids)))
          (is (= (into {}
                       (map (fn [node-id]
                              [node-id (g/node-by-id basis-before node-id)]))
                       [first-order-override-directly-owned-node-id
                        second-order-override-directly-owned-node-id
                        first-order-override-indirectly-owned-node-id
                        second-order-override-indirectly-owned-node-id])
                 (:nodes-deleted tx-result)))
          (is (= replacement-owner-node-id (g/override-original basis-after first-order-override-owner-node-id)))
          (is (= replacement-owner-node-id (:root-id (ig/override-by-id basis-after (g/override-id basis-after first-order-override-owner-node-id)))))
          (is (= replacement-directly-owned-node-id (g/override-original basis-after first-order-override-replacement-directly-owned-node-id)))
          (is (= replacement-indirectly-owned-node-id (g/override-original basis-after first-order-override-replacement-indirectly-owned-node-id)))
          (is (= first-order-override-owner-node-id (g/override-original basis-after second-order-override-owner-node-id)))
          (is (= first-order-override-replacement-directly-owned-node-id (g/override-original basis-after second-order-override-replacement-directly-owned-node-id)))
          (is (= first-order-override-replacement-indirectly-owned-node-id (g/override-original basis-after second-order-override-replacement-indirectly-owned-node-id))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

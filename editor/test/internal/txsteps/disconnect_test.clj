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

(ns internal.txsteps.disconnect-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest disconnect-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :regular-input))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :regular-input]] (g/targets basis source-node-id :property-output)))
                  (is (= [[source-node-id :property-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc indices."
                  (is (= [[source-node-id :property-output target-node-id :regular-input]]
                         (helpers/index-source-arc-tuples basis graph-id source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :source-value (g/node-value target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :property-output)))
                  (is (= [] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc indices."
                  (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id source-node-id) :property-output)))
                  (is (not (contains? (helpers/index-target-arcs-by-label basis graph-id target-node-id) :regular-input))))

                (testing "Output values."
                  (is (= nil (g/node-value target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect source-node-id :property-output target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest disconnect-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :array-input))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :array-input]] (g/targets basis source-node-id :property-output)))
                  (is (= [[source-node-id :property-output]] (g/sources basis target-node-id :array-input))))

                (testing "Internal arc indices."
                  (is (= [[source-node-id :property-output target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:source-value] (g/node-value target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :property-output)))
                  (is (= [] (g/sources basis target-node-id :array-input))))

                (testing "Internal arc indices."
                  (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id source-node-id) :property-output)))
                  (is (not (contains? (helpers/index-target-arcs-by-label basis graph-id target-node-id) :array-input))))

                (testing "Output values."
                  (is (= [] (g/node-value target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect source-node-id :property-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest disconnect-first-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [first-source-node-id second-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [first-source-node-id [helpers/ConnectionSourceNode :property :first-value]
                                      second-source-node-id [helpers/ConnectionSourceNode :property :second-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect first-source-node-id :property-output target-node-id :array-input)
                (g/connect second-source-node-id :property-output target-node-id :array-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)]
              (is (g/connected? basis first-source-node-id :property-output target-node-id :array-input))
              (is (g/connected? basis second-source-node-id :property-output target-node-id :array-input))
              (is (= [[target-node-id :array-input]] (g/targets basis first-source-node-id :property-output)))
              (is (= [[target-node-id :array-input]] (g/targets basis second-source-node-id :property-output)))
              (is (= #{[first-source-node-id :property-output]
                       [second-source-node-id :property-output]}
                     (set (g/sources basis target-node-id :array-input))))
              (testing "Internal arc indices."
                (is (= [[first-source-node-id :property-output target-node-id :array-input]]
                       (helpers/index-source-arc-tuples basis graph-id first-source-node-id :property-output)))
                (is (= [[second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/index-source-arc-tuples basis graph-id second-source-node-id :property-output)))
                (is (= #{[first-source-node-id :property-output target-node-id :array-input]
                         [second-source-node-id :property-output target-node-id :array-input]}
                       (set (helpers/index-target-arc-tuples basis graph-id target-node-id :array-input)))))
              (is (= [:first-value :second-value] (g/node-value target-node-id :array-output)))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)]
              (is (not (g/connected? basis first-source-node-id :property-output target-node-id :array-input)))
              (is (g/connected? basis second-source-node-id :property-output target-node-id :array-input))
              (is (= [] (g/targets basis first-source-node-id :property-output)))
              (is (= [[target-node-id :array-input]] (g/targets basis second-source-node-id :property-output)))
              (is (= [[second-source-node-id :property-output]] (g/sources basis target-node-id :array-input)))
              (testing "Internal arc indices."
                (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id first-source-node-id) :property-output)))
                (is (= [[second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/index-source-arc-tuples basis graph-id second-source-node-id :property-output)
                       (helpers/index-target-arc-tuples basis graph-id target-node-id :array-input))))
              (is (= [:second-value] (g/node-value target-node-id :array-output)))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect first-source-node-id :property-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest disconnect-shadowing-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :property :initial-source-value]
                                      _shadowing-source-node-id [helpers/ConnectionSourceNode :property :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :property-output original-target-node-id :regular-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-lookup]
                  (let [first-order-override-target-node-id (get id-lookup original-target-node-id)]
                    (g/connect shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input))))))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input]]
                         (helpers/index-source-arc-tuples basis graph-id shadowing-source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id first-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input] [first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id shadowing-source-node-id) :property-output)))
                  (is (not (contains? (helpers/index-target-arcs-by-label basis graph-id first-order-override-target-node-id) :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest disconnect-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :property :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :property :initial-source-two-value]
                                      _shadowing-source-node-id [helpers/ConnectionSourceNode :property :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :property-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :property-output original-target-node-id :array-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-lookup]
                  (let [first-order-override-target-node-id (get id-lookup original-target-node-id)]
                    (g/connect shadowing-source-node-id :property-output first-order-override-target-node-id :array-input))))))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id shadowing-source-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id first-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :array-input)))
                  (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id shadowing-source-node-id) :property-output)))
                  (is (not (contains? (helpers/index-target-arcs-by-label basis graph-id first-order-override-target-node-id) :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect shadowing-source-node-id :property-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest disconnect-first-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-one-node-id shadowing-source-two-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :property :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :property :initial-source-two-value]
                                      _shadowing-source-one-node-id [helpers/ConnectionSourceNode :property :shadowing-source-one-value]
                                      _shadowing-source-two-node-id [helpers/ConnectionSourceNode :property :shadowing-source-two-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :property-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :property-output original-target-node-id :array-input))))

          [first-order-override-target-node-id second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-mapping]
                  (let [first-order-override-target-node-id (get id-mapping original-target-node-id)]
                    (concat
                      (g/connect shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input)
                      (g/connect shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input)
                      (g/override first-order-override-target-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= #{[shadowing-source-one-node-id :property-output]
                           [shadowing-source-two-node-id :property-output]}
                         (set (ig/explicit-sources basis first-order-override-target-node-id :array-input))))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= #{[shadowing-source-one-node-id :property-output]
                           [shadowing-source-two-node-id :property-output]}
                         (set (g/sources basis first-order-override-target-node-id :array-input))))
                  (is (= #{[shadowing-source-one-node-id :property-output]
                           [shadowing-source-two-node-id :property-output]}
                         (set (g/sources basis second-order-override-target-node-id :array-input)))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id shadowing-source-one-node-id :property-output)))
                  (is (= [[shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id shadowing-source-two-node-id :property-output)))
                  (is (= #{[shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input]
                           [shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input]}
                         (set (helpers/index-target-arc-tuples basis graph-id first-order-override-target-node-id :array-input)))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-two-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-two-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-two-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc indices."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/index-target-arc-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/index-source-arc-tuples basis graph-id shadowing-source-two-node-id :property-output)
                         (helpers/index-target-arc-tuples basis graph-id first-order-override-target-node-id :array-input)))
                  (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id shadowing-source-one-node-id) :property-output))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/disconnect shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-deletion-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [indirectly-owned-node-id
           directly-owned-node-id
           owner-node-id
           first-order-override-owner-node-id
           second-order-override-owner-node-id
           first-order-override-directly-owned-node-id
           first-order-override-indirectly-owned-node-id
           second-order-override-directly-owned-node-id
           second-order-override-indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [indirectly-owned-node-id helpers/ConnectionSourceNode
                 directly-owned-node-id helpers/ConnectionTargetNode
                 owner-node-id helpers/ConnectionTargetNode]
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id nil
                  (fn [_evaluation-context id-lookup]
                    (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                      (concat
                        (g/override first-order-override-owner-node-id)
                        (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input))))))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (is (= [first-order-override-directly-owned-node-id] (g/overrides basis directly-owned-node-id)))
              (is (= [first-order-override-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)))
              (is (= [second-order-override-directly-owned-node-id] (g/overrides basis first-order-override-directly-owned-node-id)))
              (is (= [second-order-override-indirectly-owned-node-id] (g/overrides basis first-order-override-indirectly-owned-node-id)))
              (is (= directly-owned-node-id (g/override-original basis first-order-override-directly-owned-node-id)))
              (is (= indirectly-owned-node-id (g/override-original basis first-order-override-indirectly-owned-node-id)))
              (is (= first-order-override-directly-owned-node-id (g/override-original basis second-order-override-directly-owned-node-id)))
              (is (= first-order-override-indirectly-owned-node-id (g/override-original basis second-order-override-indirectly-owned-node-id)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id
                       first-order-override-directly-owned-node-id
                       second-order-override-directly-owned-node-id
                       first-order-override-indirectly-owned-node-id
                       second-order-override-indirectly-owned-node-id}
                     (set (g/node-ids graph))))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                     (helpers/index-target-arc-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (is (empty? (g/overrides basis directly-owned-node-id)))
              (is (empty? (g/overrides basis indirectly-owned-node-id)))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/index-source-arc-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/index-target-arc-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (not (contains? (helpers/index-source-arcs-by-label basis graph-id directly-owned-node-id) :regular-cascade-delete-output)))
              (is (not (contains? (helpers/index-target-arcs-by-label basis graph-id owner-node-id) :regular-cascade-delete-input)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id}
                     (set (g/node-ids graph))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [basis-before (g/now)]
          (is (= (into {}
                       (map (fn [node-id]
                              [node-id (g/node-by-id basis-before node-id)]))
                       [first-order-override-directly-owned-node-id
                        second-order-override-directly-owned-node-id
                        first-order-override-indirectly-owned-node-id
                        second-order-override-indirectly-owned-node-id])
                 (:nodes-deleted
                   (g/transact
                     (g/disconnect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input))))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest undo-is-granular-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id
           first-target-node-id
           second-target-node-id
           third-target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes graph-id
                [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                 first-target-node-id helpers/ConnectionTargetNode
                 second-target-node-id helpers/ConnectionTargetNode
                 _third-target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output first-target-node-id :regular-input)
                (g/connect source-node-id :property-output second-target-node-id :regular-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)]
              (is (= #{[first-target-node-id :regular-input]
                       [second-target-node-id :regular-input]
                       [third-target-node-id :regular-input]}
                     (set (g/targets basis source-node-id :property-output))))
              (is (= [[source-node-id :property-output]]
                     (g/sources basis first-target-node-id :regular-input)
                     (g/sources basis second-target-node-id :regular-input)
                     (g/sources basis third-target-node-id :regular-input)))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)]
              (is (= #{[second-target-node-id :regular-input]
                       [third-target-node-id :regular-input]}
                     (set (g/targets basis source-node-id :property-output))))
              (is (= [] (g/sources basis first-target-node-id :regular-input)))
              (is (= [[source-node-id :property-output]]
                     (g/sources basis second-target-node-id :regular-input)
                     (g/sources basis third-target-node-id :regular-input)))))]

      (testing "Transact."
        (g/transact
          {:undo-key ::disconnect}
          (g/disconnect source-node-id :property-output first-target-node-id :regular-input))
        (g/transact
          {:undo-key ::connect}
          (g/connect source-node-id :property-output third-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! ::disconnect)
        (ensure-before!))

      (testing "Redo."
        (g/redo! ::disconnect)
        (ensure-after!)))))

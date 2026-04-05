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

(ns internal.txsteps.connect-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest introduce-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (concat
                (g/make-node graph-id helpers/ConnectionSourceNode :value :source-value)
                (g/make-node graph-id helpers/ConnectionTargetNode))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :value-output)))
                  (is (= [] (g/sources basis target-node-id :regular-input))))

                (testing "Output values."
                  (is (= nil (g/node-value target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :regular-input]] (g/targets basis source-node-id :value-output)))
                  (is (= [[source-node-id :value-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :source-value (g/node-value target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect source-node-id :value-output target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest replace-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-node-id replacement-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :value :initial-value]
                                      replacement-source-node-id [helpers/ConnectionSourceNode :value :replacement-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :value-output target-node-id :regular-input))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :regular-input]] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [] (g/targets basis replacement-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-value (g/node-value target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [[target-node-id :regular-input]] (g/targets basis replacement-source-node-id :value-output)))
                  (is (= [[replacement-source-node-id :value-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :replacement-value (g/node-value target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect replacement-source-node-id :value-output target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest introduce-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (concat
                (g/make-node graph-id helpers/ConnectionSourceNode :value :source-value)
                (g/make-node graph-id helpers/ConnectionTargetNode))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :value-output)))
                  (is (= [] (g/sources basis target-node-id :array-input))))

                (testing "Output values."
                  (is (= [] (g/node-value target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :array-input]] (g/targets basis source-node-id :value-output)))
                  (is (= [[source-node-id :value-output]] (g/sources basis target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:source-value] (g/node-value target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect source-node-id :value-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest append-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-node-id replacement-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :value :initial-value]
                                      replacement-source-node-id [helpers/ConnectionSourceNode :value :replacement-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :value-output target-node-id :array-input))))

          ensure-before!
          (fn ensure-before! []
            (is (g/connected? (g/now) initial-source-node-id :value-output target-node-id :array-input))
            (is (not (g/connected? (g/now) replacement-source-node-id :value-output target-node-id :array-input)))
            (is (= [[target-node-id :array-input]] (g/targets-of initial-source-node-id :value-output)))
            (is (= [] (g/targets-of replacement-source-node-id :value-output)))
            (is (= [[initial-source-node-id :value-output]] (g/sources-of target-node-id :array-input)))
            (is (= [:initial-value] (g/node-value target-node-id :array-output))))

          ensure-after!
          (fn ensure-after! []
            (is (g/connected? (g/now) initial-source-node-id :value-output target-node-id :array-input))
            (is (g/connected? (g/now) replacement-source-node-id :value-output target-node-id :array-input))
            (is (= [[target-node-id :array-input]] (g/targets-of initial-source-node-id :value-output)))
            (is (= [[target-node-id :array-input]] (g/targets-of replacement-source-node-id :value-output)))
            (is (= [[initial-source-node-id :value-output] [replacement-source-node-id :value-output]] (g/sources-of target-node-id :array-input)))
            (is (= [:initial-value :replacement-value] (g/node-value target-node-id :array-output))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect replacement-source-node-id :value-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest introduce-shadowing-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :value :initial-source-value]
                                      shadowing-source-node-id [helpers/ConnectionSourceNode :value :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :value-output original-target-node-id :regular-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id)))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :value-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input] [first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-node-id :value-output first-order-override-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest replace-shadowing-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-node-id initial-shadowing-source-node-id replacement-shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :value :initial-source-value]
                                      initial-shadowing-source-node-id [helpers/ConnectionSourceNode :value :initial-shadowing-source-value]
                                      replacement-shadowing-source-node-id [helpers/ConnectionSourceNode :value :replacement-shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :value-output original-target-node-id :regular-input))))

          [first-order-override-target-node-id second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-mapping]
                  (let [first-order-override-target-node-id (get id-mapping original-target-node-id)]
                    (concat
                      (g/connect initial-shadowing-source-node-id :value-output first-order-override-target-node-id :regular-input)
                      (g/override first-order-override-target-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis initial-shadowing-source-node-id :value-output)))
                  (is (= [] (ig/explicit-targets basis replacement-shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis initial-shadowing-source-node-id :value-output)))
                  (is (= [] (g/targets basis replacement-shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :value-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :value-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :value-output)))
                  (is (= [] (ig/explicit-targets basis initial-shadowing-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis replacement-shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :value-output)))
                  (is (= [] (g/targets basis initial-shadowing-source-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis replacement-shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-node-id :value-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :value-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :value-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :replacement-shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :replacement-shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect replacement-shadowing-source-node-id :value-output first-order-override-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest introduce-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :value :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :value :initial-source-two-value]
                                      shadowing-source-node-id [helpers/ConnectionSourceNode :value :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :value-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :value-output original-target-node-id :array-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id)))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :value-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :value-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :value-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-node-id :value-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest append-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-one-node-id shadowing-source-two-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :value :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :value :initial-source-two-value]
                                      shadowing-source-one-node-id [helpers/ConnectionSourceNode :value :shadowing-source-one-value]
                                      shadowing-source-two-node-id [helpers/ConnectionSourceNode :value :shadowing-source-two-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :value-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :value-output original-target-node-id :array-input))))

          [first-order-override-target-node-id second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-mapping]
                  (let [first-order-override-target-node-id (get id-mapping original-target-node-id)]
                    (concat
                      (g/connect shadowing-source-one-node-id :value-output first-order-override-target-node-id :array-input)
                      (g/override first-order-override-target-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-one-node-id :value-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-two-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-one-node-id :value-output)))
                  (is (= [] (g/targets basis shadowing-source-two-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-one-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-two-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output] [shadowing-source-two-node-id :value-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :value-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-one-node-id :value-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-two-node-id :value-output)))
                  (is (= [[initial-source-one-node-id :value-output] [initial-source-two-node-id :value-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output] [shadowing-source-two-node-id :value-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :value-output] [shadowing-source-two-node-id :value-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-two-node-id :value-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

(deftest override-node-creation-test
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
                [indirectly-owned-node-id helpers/ConnectionSourceNode
                 directly-owned-node-id helpers/ConnectionTargetNode
                 owner-node-id helpers/ConnectionTargetNode]
                (g/connect indirectly-owned-node-id :value-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id nil
                  (fn [_evaluation-context id-lookup]
                    (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                      (g/override first-order-override-owner-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (is (empty? (g/overrides directly-owned-node-id)))
            (is (empty? (g/overrides indirectly-owned-node-id)))
            (is (= #{indirectly-owned-node-id
                     directly-owned-node-id
                     owner-node-id
                     first-order-override-owner-node-id
                     second-order-override-owner-node-id}
                   (set (g/node-ids (g/graph graph-id))))))

          ensure-after!
          (fn ensure-after! []
            (let [[first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides directly-owned-node-id)
                  [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides indirectly-owned-node-id)
                  [second-order-override-directly-owned-node-id :as overrides-of-first-order-override-directly-owned-node-id] (g/overrides first-order-override-directly-owned-node-id)
                  [second-order-override-indirectly-owned-node-id :as overrides-of-first-order-override-indirectly-owned-node-id] (g/overrides first-order-override-indirectly-owned-node-id)]
              (when (and (is (= 1 (count overrides-of-directly-owned-node-id)))
                         (is (= 1 (count overrides-of-indirectly-owned-node-id)))
                         (is (= 1 (count overrides-of-first-order-override-directly-owned-node-id)))
                         (is (= 1 (count overrides-of-first-order-override-indirectly-owned-node-id)))
                         (is (g/node-id? first-order-override-directly-owned-node-id))
                         (is (g/node-id? first-order-override-indirectly-owned-node-id))
                         (is (g/node-id? second-order-override-directly-owned-node-id))
                         (is (g/node-id? second-order-override-indirectly-owned-node-id)))
                (is (= #{indirectly-owned-node-id
                         directly-owned-node-id
                         owner-node-id
                         first-order-override-owner-node-id
                         second-order-override-owner-node-id
                         first-order-override-directly-owned-node-id
                         second-order-override-directly-owned-node-id
                         first-order-override-indirectly-owned-node-id
                         second-order-override-indirectly-owned-node-id}
                       (set (g/node-ids (g/graph graph-id))))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [[first-order-override-directly-owned-node-id
               first-order-override-indirectly-owned-node-id
               second-order-override-directly-owned-node-id
               second-order-override-indirectly-owned-node-id
               :as created-node-ids]
              (g/tx-nodes-added
                (g/transact
                  (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)))]
          (is (= 4 (count created-node-ids)))
          (is (= directly-owned-node-id (g/override-original first-order-override-directly-owned-node-id)))
          (is (= indirectly-owned-node-id (g/override-original first-order-override-indirectly-owned-node-id)))
          (is (= first-order-override-directly-owned-node-id (g/override-original second-order-override-directly-owned-node-id)))
          (is (= first-order-override-indirectly-owned-node-id (g/override-original second-order-override-indirectly-owned-node-id)))
          (is (= [first-order-override-directly-owned-node-id] (g/overrides directly-owned-node-id)))
          (is (= [first-order-override-indirectly-owned-node-id] (g/overrides indirectly-owned-node-id)))
          (is (= [second-order-override-directly-owned-node-id] (g/overrides first-order-override-directly-owned-node-id)))
          (is (= [second-order-override-indirectly-owned-node-id] (g/overrides first-order-override-indirectly-owned-node-id))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

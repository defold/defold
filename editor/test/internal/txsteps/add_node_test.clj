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

(ns internal.txsteps.add-node-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [internal.graph.types :as gt]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(g/defnode ReturnsTxResultWithNodesAddedTestNode)

(deftest adds-nodes-to-graph-test
  (test-support/with-clean-system
    (let [added-node-ids (vec (g/take-node-ids world 20))]

      (testing "Before transact."
        (doseq [node-id added-node-ids]
          (is (nil? (g/node-by-id node-id)))))

      (testing "Transact."
        (g/transact
          (mapv #(g/add-node (g/construct ReturnsTxResultWithNodesAddedTestNode :_node-id %))
                added-node-ids))
        (doseq [node-id added-node-ids]
          (let [node (g/node-by-id node-id)]
            (when (is (some? node))
              (is (g/node-instance*? ReturnsTxResultWithNodesAddedTestNode node))
              (is (= node-id (gt/node-id node))))))))))

(deftest returns-tx-result-with-nodes-added-test
  (test-support/with-clean-system
    (let [added-node-ids (vec (g/take-node-ids world 20))]
      (testing "Returns tx-result with added node-ids in construction order."
        (is (= added-node-ids
               (g/tx-nodes-added
                 (g/transact
                   (map #(g/add-node (g/construct ReturnsTxResultWithNodesAddedTestNode :_node-id %))
                        added-node-ids)))))))))

(g/defnode PropertyHasDefaultValueTestNode
  (property property-with-default g/Any (default :default-property-value)))

(deftest property-has-default-value-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct PropertyHasDefaultValueTestNode :_node-id node-id)))
      (is (= :default-property-value (g/node-value node-id :property-with-default))))))

(g/defnode InvokesPropertySettersTestNode
  (inherits helpers/EffectLogNode)
  (property effecting-property-1 g/Any
            (default :effecting-property-1-default)
            (set (helpers/effect-log-setter :effecting-property-1)))
  (property effecting-property-2 g/Any
            (default :effecting-property-2-default)
            (set (helpers/effect-log-setter :effecting-property-2)))
  (property effecting-property-3 g/Any
            (default :effecting-property-3-default)
            (set (helpers/effect-log-setter :effecting-property-3)))
  (property effecting-property-4 g/Any
            (default :effecting-property-4-default)
            (set (helpers/effect-log-setter :effecting-property-4)))
  (property effecting-property-5 g/Any
            (default :effecting-property-5-default)
            (set (helpers/effect-log-setter :effecting-property-5)))
  (property effecting-property-6 g/Any
            (default :effecting-property-6-default)
            (set (helpers/effect-log-setter :effecting-property-6)))
  (property effecting-property-7 g/Any
            (default :effecting-property-7-default)
            (set (helpers/effect-log-setter :effecting-property-7)))
  (property effecting-property-8 g/Any
            (default :effecting-property-8-default)
            (set (helpers/effect-log-setter :effecting-property-8)))
  (property effecting-property-9 g/Any
            (default :effecting-property-9-default)
            (set (helpers/effect-log-setter :effecting-property-9))))

(deftest invokes-property-setters-with-default-values-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct InvokesPropertySettersTestNode
                      :_node-id node-id)))
      (is (= [{:prop-kw :effecting-property-1
               :old-value nil
               :new-value :effecting-property-1-default}
              {:prop-kw :effecting-property-2
               :old-value nil
               :new-value :effecting-property-2-default}
              {:prop-kw :effecting-property-3
               :old-value nil
               :new-value :effecting-property-3-default}
              {:prop-kw :effecting-property-4
               :old-value nil
               :new-value :effecting-property-4-default}
              {:prop-kw :effecting-property-5
               :old-value nil
               :new-value :effecting-property-5-default}
              {:prop-kw :effecting-property-6
               :old-value nil
               :new-value :effecting-property-6-default}
              {:prop-kw :effecting-property-7
               :old-value nil
               :new-value :effecting-property-7-default}
              {:prop-kw :effecting-property-8
               :old-value nil
               :new-value :effecting-property-8-default}
              {:prop-kw :effecting-property-9
               :old-value nil
               :new-value :effecting-property-9-default}]
             (helpers/effect-log node-id))
          "Property setters run in declaration order."))))

(deftest invokes-property-setters-with-specified-values-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct InvokesPropertySettersTestNode
                      :_node-id node-id
                      :effecting-property-9 :effecting-property-9-value
                      :effecting-property-8 :effecting-property-8-value
                      :effecting-property-7 :effecting-property-7-value
                      :effecting-property-6 :effecting-property-6-value
                      :effecting-property-5 :effecting-property-5-value
                      :effecting-property-4 :effecting-property-4-value
                      :effecting-property-3 :effecting-property-3-value
                      :effecting-property-2 :effecting-property-2-value
                      :effecting-property-1 :effecting-property-1-value)))
      (is (= [{:prop-kw :effecting-property-1
               :old-value nil
               :new-value :effecting-property-1-value}
              {:prop-kw :effecting-property-2
               :old-value nil
               :new-value :effecting-property-2-value}
              {:prop-kw :effecting-property-3
               :old-value nil
               :new-value :effecting-property-3-value}
              {:prop-kw :effecting-property-4
               :old-value nil
               :new-value :effecting-property-4-value}
              {:prop-kw :effecting-property-5
               :old-value nil
               :new-value :effecting-property-5-value}
              {:prop-kw :effecting-property-6
               :old-value nil
               :new-value :effecting-property-6-value}
              {:prop-kw :effecting-property-7
               :old-value nil
               :new-value :effecting-property-7-value}
              {:prop-kw :effecting-property-8
               :old-value nil
               :new-value :effecting-property-8-value}
              {:prop-kw :effecting-property-9
               :old-value nil
               :new-value :effecting-property-9-value}]
             (helpers/effect-log node-id))
          "Property setters run in declaration order."))))

(g/defnode UndoRedoTestNode
  (inherits helpers/EffectLogNode)

  (property basic-property g/Any)
  (output basic-output g/Any :cached
          (gu/passthrough basic-property))

  (property defaulted-basic-property g/Any
            (default :defaulted-basic-property-default))
  (output defaulted-basic-output g/Any :cached
          (gu/passthrough defaulted-basic-property))

  (property effecting-property g/Any
            (set (helpers/effect-log-setter :effecting-property)))
  (output effecting-output g/Any :cached
          (gu/passthrough effecting-property))

  (property defaulted-effecting-property g/Any
            (default :defaulted-effecting-property-default)
            (set (helpers/effect-log-setter :defaulted-effecting-property)))
  (output defaulted-effecting-output g/Any :cached
          (gu/passthrough defaulted-effecting-property)))

(deftest generates-single-undo-step-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          node-id (first (g/take-node-ids graph-id 1))]

      (testing "Before transact."
        (is (= 0 (g/undo-stack-count graph-id))))

      (testing "Transact."
        (g/transact
          (g/add-node (g/construct UndoRedoTestNode
                        :_node-id node-id
                        :basic-property :basic-property-value
                        :effecting-property :effecting-property-value)))
        (is (= 1 (g/undo-stack-count graph-id)))))))

(deftest undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          node-id (first (g/take-node-ids graph-id 1))

          ensure-node-absent-from-graph!
          (fn ensure-node-absent-from-graph! []
            (is (nil? (g/node-by-id node-id)))
            (is (thrown? IllegalArgumentException (g/node-value node-id :_node-id))))

          ensure-node-present-in-graph!
          (fn ensure-node-present-in-graph! []
            (is (some? (g/node-by-id node-id)))
            (is (= :basic-property-value
                   (g/node-value node-id :basic-property)
                   (g/node-value node-id :basic-output)))
            (is (= :defaulted-basic-property-default
                   (g/node-value node-id :defaulted-basic-property)
                   (g/node-value node-id :defaulted-basic-output)))
            (is (= :effecting-property-value
                   (g/node-value node-id :effecting-property)
                   (g/node-value node-id :effecting-output)))
            (is (= :defaulted-effecting-property-default
                   (g/node-value node-id :defaulted-effecting-property)
                   (g/node-value node-id :defaulted-effecting-output)))
            (is (= [{:prop-kw :effecting-property
                     :old-value nil
                     :new-value :effecting-property-value}
                    {:prop-kw :defaulted-effecting-property
                     :old-value nil
                     :new-value :defaulted-effecting-property-default}]
                   (helpers/effect-log node-id))))]

      (testing "Before transact."
        (ensure-node-absent-from-graph!))

      (testing "Transact."
        (g/transact
          (g/add-node (g/construct UndoRedoTestNode
                        :_node-id node-id
                        :basic-property :basic-property-value
                        :effecting-property :effecting-property-value)))
        (ensure-node-present-in-graph!))

      (testing "Node absent after undo."
        (g/undo! graph-id)
        (ensure-node-absent-from-graph!))

      (testing "Node present after redo."
        (g/redo! graph-id)
        (ensure-node-present-in-graph!)))))

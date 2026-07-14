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
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]
            [util.coll :as coll]))

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
            (is (g/node-instance*? ReturnsTxResultWithNodesAddedTestNode node))
            (is (= node-id (some-> node gt/node-id)))))))))

(deftest returns-tx-result-with-nodes-added-test
  (test-support/with-clean-system
    (let [added-node-ids (vec (g/take-node-ids world 20))]
      (testing "Returns tx-result with added node-ids in construction order."
        (is (= added-node-ids
               (g/tx-nodes-added
                 (g/transact
                   (map #(g/add-node (g/construct ReturnsTxResultWithNodesAddedTestNode :_node-id %))
                        added-node-ids)))))))))

(deftest adds-multiple-nodes-with-single-change-test
  (test-support/with-clean-system
    (let [added-node-ids (vec (g/take-node-ids world 20))
          added-nodes (mapv #(g/construct ReturnsTxResultWithNodesAddedTestNode :_node-id %)
                             added-node-ids)
          tx-result (g/transact (g/add-nodes added-nodes))]
      (is (= added-node-ids (g/tx-nodes-added tx-result)))
      (is (= 1 (count (:undoable-changes tx-result))))

      (g/undo! :undo/global)
      (doseq [node-id added-node-ids]
        (is (nil? (g/node-by-id node-id))))

      (g/redo! :undo/global)
      (doseq [node-id added-node-ids]
        (is (g/node-instance*? ReturnsTxResultWithNodesAddedTestNode (g/node-by-id node-id)))))))

(deftest registers-override-node-relationships-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          override-id (gt/make-override-id graph-id 1000)
          [original-node-id override-node-id] (vec (g/take-node-ids graph-id 2))
          original-node (g/construct helpers/OverrideTestNode :_node-id original-node-id)
          override-node (in/make-override-node override-id override-node-id helpers/OverrideTestNode original-node-id {})]
      (g/transact
        (g/add-nodes [original-node override-node]))
      (is (= [override-node-id] (g/overrides original-node-id)))

      (g/undo! :undo/global)
      (is (nil? (g/node-by-id original-node-id)))
      (is (nil? (g/overrides original-node-id)))

      (g/redo! :undo/global)
      (is (= [override-node-id] (g/overrides original-node-id))))))

(g/defnode PropertyHasDefaultValueTestNode
  (property property-with-default g/Any (default :default-property-value)))

(deftest property-has-default-value-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct PropertyHasDefaultValueTestNode :_node-id node-id)))
      (is (= :default-property-value (g/node-value node-id :property-with-default))))))

(g/defnode InvokesPropertySettersTestBaseNode
  (inherits helpers/EffectLogNode)
  (property inherited-effecting-property-1 g/Any
            (default :inherited-effecting-property-1-default)
            (set (helpers/effect-log-setter :inherited-effecting-property-1)))
  (property inherited-effecting-property-2 g/Any
            (default :inherited-effecting-property-2-default)
            (set (helpers/effect-log-setter :inherited-effecting-property-2)))
  (property inherited-effecting-property-3 g/Any
            (default :inherited-effecting-property-3-default)
            (set (helpers/effect-log-setter :inherited-effecting-property-3)))
  (property inherited-effecting-property-4 g/Any
            (default :inherited-effecting-property-4-default)
            (set (helpers/effect-log-setter :inherited-effecting-property-4)))
  (property inherited-effecting-property-5 g/Any
            (default :inherited-effecting-property-5-default)
            (set (helpers/effect-log-setter :inherited-effecting-property-5))))

(g/defnode InvokesPropertySettersTestNode
  (inherits InvokesPropertySettersTestBaseNode)
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
  (property inherited-effecting-property-1 g/Any
            (default :inherited-effecting-property-1-overridden-default))
  (property inherited-effecting-property-2 g/Any
            (default :inherited-effecting-property-2-overridden-default)))

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
              {:prop-kw :inherited-effecting-property-1
               :old-value nil
               :new-value :inherited-effecting-property-1-overridden-default}
              {:prop-kw :inherited-effecting-property-2
               :old-value nil
               :new-value :inherited-effecting-property-2-overridden-default}
              {:prop-kw :inherited-effecting-property-3
               :old-value nil
               :new-value :inherited-effecting-property-3-default}
              {:prop-kw :inherited-effecting-property-4
               :old-value nil
               :new-value :inherited-effecting-property-4-default}
              {:prop-kw :inherited-effecting-property-5
               :old-value nil
               :new-value :inherited-effecting-property-5-default}]
             (helpers/effect-log node-id))
          "Property setters run in declaration order across inherited node types."))))

(deftest invokes-property-setters-with-specified-values-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct InvokesPropertySettersTestNode
                      :_node-id node-id
                      :effecting-property-5 :effecting-property-5-value
                      :effecting-property-4 :effecting-property-4-value
                      :effecting-property-3 :effecting-property-3-value
                      :effecting-property-2 :effecting-property-2-value
                      :effecting-property-1 :effecting-property-1-value
                      :inherited-effecting-property-5 :inherited-effecting-property-5-value
                      :inherited-effecting-property-4 :inherited-effecting-property-4-value
                      :inherited-effecting-property-3 :inherited-effecting-property-3-value
                      :inherited-effecting-property-2 :inherited-effecting-property-2-value
                      :inherited-effecting-property-1 :inherited-effecting-property-1-value)))
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
              {:prop-kw :inherited-effecting-property-1
               :old-value nil
               :new-value :inherited-effecting-property-1-value}
              {:prop-kw :inherited-effecting-property-2
               :old-value nil
               :new-value :inherited-effecting-property-2-value}
              {:prop-kw :inherited-effecting-property-3
               :old-value nil
               :new-value :inherited-effecting-property-3-value}
              {:prop-kw :inherited-effecting-property-4
               :old-value nil
               :new-value :inherited-effecting-property-4-value}
              {:prop-kw :inherited-effecting-property-5
               :old-value nil
               :new-value :inherited-effecting-property-5-value}]
             (helpers/effect-log node-id))
          "Property setters run in declaration order across inherited node types."))))

(deftest does-not-invoke-property-setters-with-nil-default-values-test
  (test-support/with-clean-system
    (let [node-id (first (g/take-node-ids world 1))]
      (g/transact
        (g/add-node (g/construct helpers/PropertyTestNode
                      :_node-id node-id)))

      (testing "Ensure the property has a nil default value so the test itself is correct."
        (let [[_prop-kw default-value :as property-setter-info]
              (coll/first-where
                (fn [[prop-kw _default-value]]
                  (= :effecting-property prop-kw))
                (in/ordered-property-setter-infos (g/node-type* node-id)))]

          (is (some? property-setter-info))
          (is (nil? default-value))))

      ;; Note: I'm not entirely sure about this, but it has been like this for a
      ;; long time. I think this may be an attempt t an optimization rather than
      ;; desirable behavior. We might want to change this behavior so that
      ;; setters are invoked with the default value for unassigned properties if
      ;; it turns out to cause problems.
      (testing "Effects from setter not applied after construction."
        (is (= []
               (g/node-value node-id :effect-log-property)
               (g/node-value node-id :effect-log-output)))
        (is (= nil
               (gt/assigned-properties (g/node-by-id node-id)))))

      (testing "Effects from setter applied after setting the value."
        (g/transact
          (g/set-property node-id :effecting-property nil))
        (is (= [{:prop-kw :effecting-property
                 :old-value nil
                 :new-value nil}]
               (g/node-value node-id :effect-log-property)
               (g/node-value node-id :effect-log-output)))
        (is (= {:effecting-property nil
                :effect-log-property [{:prop-kw :effecting-property
                                       :old-value nil
                                       :new-value nil}]}
               (gt/assigned-properties (g/node-by-id node-id)))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= []
               (g/node-value node-id :effect-log-property)
               (g/node-value node-id :effect-log-output)))
        (is (= nil
               (gt/assigned-properties (g/node-by-id node-id)))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= [{:prop-kw :effecting-property
                 :old-value nil
                 :new-value nil}]
               (g/node-value node-id :effect-log-property)
               (g/node-value node-id :effect-log-output)))
        (is (= {:effecting-property nil
                :effect-log-property [{:prop-kw :effecting-property
                                       :old-value nil
                                       :new-value nil}]}
               (gt/assigned-properties (g/node-by-id node-id))))))))

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
    (let [graph-id (g/make-graph!)
          node-id (first (g/take-node-ids graph-id 1))]

      (testing "Before transact."
        (is (= 0 (g/undo-stack-count :undo/global))))

      (testing "Transact."
        (g/transact
          (g/add-node (g/construct UndoRedoTestNode
                        :_node-id node-id
                        :basic-property :basic-property-value
                        :effecting-property :effecting-property-value)))
        (is (= 1 (g/undo-stack-count :undo/global)))))))

(deftest undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
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
        (g/undo! :undo/global)
        (ensure-node-absent-from-graph!))

      (testing "Node present after redo."
        (g/redo! :undo/global)
        (ensure-node-present-in-graph!)))))

(deftest undo-add-node-with-non-undoable-outgoing-connection-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undo-key ::add-source}
              (g/make-nodes graph-id
                [_source-node-id [helpers/ConnectionSourceNode :property :source-value]])))

          [target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes graph-id
                [target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :regular-input))))

          ensure-source-exists!
          (fn ensure-source-exists! []
            (let [basis (g/now)]
              (is (g/node-by-id basis source-node-id))
              (is (g/node-by-id basis target-node-id))
              (is (= :source-value (g/node-value target-node-id :regular-output)))))

          ensure-source-does-not-exist!
          (fn ensure-source-does-not-exist! []
            (let [basis (g/now)]
              (is (nil? (g/node-by-id basis source-node-id)))
              (is (g/node-by-id basis target-node-id))
              (is (= nil (g/node-value target-node-id :regular-output)))))]

      (testing "Before undo."
        (ensure-source-exists!))

      (testing "Undo."
        (g/undo! ::add-source)
        (ensure-source-does-not-exist!))

      (testing "Redo."
        (g/redo! ::add-source)
        (ensure-source-exists!)))))

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

(ns dynamo.integration.visibility-enablement
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))


(g/defnode OutputNode
  (property can-change-prop g/Bool (default false))
  (output true-output g/Bool (g/constantly true))
  (output false-output g/Bool (g/constantly false)))

(g/defnode VisibilityTestNode
  (input a-input g/Bool)
  (property hidden-prop g/Str (dynamic visible (g/constantly false)))
  (property visible-prop g/Str (dynamic visible (g/constantly true)))
  (property a-prop g/Str (dynamic visible (g/fnk [a-input] a-input))))

(defn- property-visible [node key]
  (get-in (g/node-value node :_properties) [:properties key :visible]))

(deftest test-visibility
  (testing "visible functions reflect in the properties"
    (with-clean-system
      (let [[vnode] (tx-nodes (g/make-node world VisibilityTestNode))]
        (is (false? (property-visible vnode :hidden-prop)))
        (is (true? (property-visible vnode :visible-prop))))))

  (testing "visible functions connnected to false inputs are hidden"
    (with-clean-system
      (let [[vnode onode] (tx-nodes (g/make-node world VisibilityTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :false-output vnode :a-input))
        (is (false? (property-visible vnode :a-prop))))))

  (testing "visible functions connnected to true inputs are shown"
    (with-clean-system
      (let [[vnode onode] (tx-nodes (g/make-node world VisibilityTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :true-output vnode :a-input))
        (is (true? (property-visible vnode :a-prop))))))

  (testing "visible functions connnected to properties can change values"
    (with-clean-system
      (let [[vnode onode] (tx-nodes (g/make-node world VisibilityTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :can-change-prop vnode :a-input))
        (is (false? (property-visible vnode :a-prop)))
        (g/transact (g/set-property onode :can-change-prop true))
        (is (true? (property-visible vnode :a-prop)))))))


(g/defnode EnablementTestNode
  (input a-input g/Bool)
  (property disabled-prop g/Str (dynamic enabled (g/constantly false)))
  (property enabled-prop g/Str (dynamic enabled (g/constantly true)))
  (property a-prop g/Str (dynamic enabled (g/fnk [a-input] a-input))))

(defn- property-enabled [node key]
  (get-in (g/node-value node :_properties) [:properties key :enabled]))

(deftest test-enbablement
  (testing "enablement functions reflect in the properties"
    (with-clean-system
      (let [[enode] (tx-nodes (g/make-node world EnablementTestNode))]
        (is (false? (property-enabled enode :disabled-prop)))
        (is (true? (property-enabled enode :enabled-prop))))))

  (testing "enablement functions connected to false inputs are disabled"
    (with-clean-system
      (let [[enode onode] (tx-nodes (g/make-node world EnablementTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :false-output enode :a-input))
        (is (false? (property-enabled enode :a-prop))))))

  (testing "enablement functions connected to true inputs are enabled"
    (with-clean-system
      (let [[enode onode] (tx-nodes (g/make-node world EnablementTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :true-output enode :a-input))
        (is (true? (property-enabled enode :a-prop))))))

  (testing "enablement functions connnected to properties can change values"
    (with-clean-system
      (let [[enode onode] (tx-nodes (g/make-node world EnablementTestNode)
                                    (g/make-node world OutputNode))]
        (g/transact (g/connect onode :can-change-prop enode :a-input))
        (is (false? (property-enabled enode :a-prop)))
        (g/transact (g/set-property onode :can-change-prop true))
        (is (true? (property-enabled enode :a-prop)))))))

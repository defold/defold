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

(ns internal.txsteps.update-property-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(deftest invokes-update-fn-with-args-test
  (test-support/with-clean-system
    (let [initial-basic-property-value :initial-basic-property-value
          node-id (g/make-node! world helpers/PropertyTestNode :basic-property initial-basic-property-value)]

      (testing "Before transact."
        (is (= initial-basic-property-value (g/raw-property-value (g/now) node-id :basic-property))))

      (testing "Transact."
        (g/update-property
          node-id :basic-property
          (fn update-fn [old-value & supplied-args]
            (is (= initial-basic-property-value old-value))
            (is (= [:first-arg :second-arg] supplied-args))
            initial-basic-property-value)
          :first-arg
          :second-arg)))))

(deftest sets-property-to-update-fn-return-value-test
  (test-support/with-clean-system
    (let [initial-basic-property-value :old-basic-property-value
          node-id (g/make-node! world helpers/PropertyTestNode :basic-property initial-basic-property-value)]

      (testing "Before transact."
        (is (= :old-basic-property-value (g/raw-property-value (g/now) node-id :basic-property))))

      (testing "Transact."
        (g/transact
          (g/update-property
            node-id :basic-property
            (constantly :new-basic-property-value)))
        (is (= :new-basic-property-value (g/raw-property-value (g/now) node-id :basic-property)))))))

(deftest basic-property-undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct helpers/PropertyTestNode
                                :_node-id original-node-id
                                :basic-property :original-basic-property-value))
                  (g/override original-node-id)))))

          ensure-original-unmodified!
          (fn ensure-original-unmodified! []
            (testing "Original node is unmodified."
              (is (= :original-basic-property-value
                     (g/raw-property-value (g/now) original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))))

          ensure-override-unmodified!
          (fn ensure-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= :original-basic-property-value
                     (g/raw-property-value (g/now) override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))

          ensure-original-modified!
          (fn ensure-original-modified! []
            (testing "Original node is modified."
              (is (= :original-basic-property-value-updated
                     (g/raw-property-value (g/now) original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))))

          ensure-original-modified-override-unmodified!
          (fn ensure-original-modified-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= :original-basic-property-value-updated
                     (g/raw-property-value (g/now) override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))

          ensure-original-modified-override-modified!
          (fn ensure-override-modified! []
            (testing "Override node is modified."
              (is (= :overridden-basic-property-value
                     (g/raw-property-value (g/now) override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact on original node."
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Transact on original node."
        (g/transact
          (g/update-property
            original-node-id :basic-property
            (fn [old-value]
              (is (= :original-basic-property-value old-value))
              :original-basic-property-value-updated)))
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Undo change to original node."
        (g/undo! graph-id)
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Redo change to original node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Transact on override node."
        (g/transact
          (g/update-property
            override-node-id :basic-property
            (fn [old-value]
              (is (= :original-basic-property-value-updated old-value))
              :overridden-basic-property-value)))
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!))

      (testing "Undo change to override node."
        (g/undo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Redo change to override node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!)))))

(deftest effecting-property-undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (g/make-node! graph-id helpers/PropertyTestNode)

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct helpers/PropertyTestNode :_node-id original-node-id))
                  (g/override original-node-id helpers/effect-log-node-override-opts)))))

          ensure-original-unmodified!
          (fn ensure-original-unmodified! []
            (testing "Original node is unmodified."
              (is (= nil
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= nil
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= []
                     (g/node-value original-node-id :effect-log-property)
                     (g/node-value original-node-id :effect-log-output)))))

          ensure-override-unmodified!
          (fn ensure-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= nil
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= nil
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= []
                     (g/node-value override-node-id :effect-log-property)
                     (g/node-value override-node-id :effect-log-output)))))

          ensure-original-modified!
          (fn ensure-original-modified! []
            (testing "Original node is modified."
              (is (= :original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= [{:prop-kw :effecting-property
                       :old-value nil
                       :new-value :original-effecting-property-value}]
                     (g/node-value original-node-id :effect-log-property)
                     (g/node-value original-node-id :effect-log-output)))))

          ensure-original-modified-override-unmodified!
          (fn ensure-original-modified-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= :original-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= []
                     (g/node-value override-node-id :effect-log-property)
                     (g/node-value override-node-id :effect-log-output)))))

          ensure-original-modified-override-modified!
          (fn ensure-override-modified! []
            (testing "Override node is modified."
              (is (= :original-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :overridden-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:prop-kw :effecting-property
                       :old-value :original-effecting-property-value
                       :new-value :overridden-effecting-property-value}]
                     (g/node-value override-node-id :effect-log-property)
                     (g/node-value override-node-id :effect-log-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact on original node."
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Setter is called during transact on original node."
        (binding [helpers/property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= original-node-id self))
                    (is (nil? old-value))
                    (is (= :original-effecting-property-value new-value))
                    (is (= :original-basic-property-value
                           (g/node-value self :basic-property evaluation-context)
                           (g/node-value self :basic-output evaluation-context))
                        "Called with in-transaction evaluation-context.")
                    (is (= :original-effecting-property-value
                           (g/node-value self :effecting-property evaluation-context)
                           (g/node-value self :effecting-output evaluation-context))
                        "Backing property was assigned before invoking setter."))]
          (g/transact
            (concat
              (g/set-property original-node-id :basic-property :original-basic-property-value)
              (g/update-property
                original-node-id :effecting-property
                (fn [old-value]
                  (is (= nil old-value))
                  :original-effecting-property-value))))))

      (testing "After transact on original node."
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Undo change to original node."
        (g/undo! graph-id)
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Redo change to original node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Setter is called during transact on override node."
        (binding [helpers/property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= override-node-id self))
                    (is (= :original-effecting-property-value old-value))
                    (is (= :overridden-effecting-property-value new-value))
                    (is (= :overridden-effecting-property-value
                           (g/node-value self :effecting-property evaluation-context)
                           (g/node-value self :effecting-output evaluation-context))
                        "Backing property was assigned before invoking setter."))]
          (g/transact
            (g/update-property
              override-node-id :effecting-property
              (fn [old-value]
                (is (= :original-effecting-property-value old-value))
                :overridden-effecting-property-value)))))

      (testing "Transact on override node."
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!))

      (testing "Undo change to override node."
        (g/undo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Redo change to override node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!)))))

(deftest validation-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          node-id (g/make-node! graph-id helpers/PropertyTestNode)]

      (testing "Before transaction attempt."
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Transaction attempt fails."
        (is (thrown?
              ExceptionInfo
              (g/transact
                (g/update-property
                  node-id :basic-property
                  (fn [old-value]
                    (is (= nil old-value))
                    "invalid-non-keyword-value"))))))

      (testing "After transaction attempt."
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))))))

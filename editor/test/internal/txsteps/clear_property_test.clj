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

(ns internal.txsteps.clear-property-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(deftest basic-property-undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          original-props
          {:basic-property :original-basic-property-value
           :effecting-property :original-effecting-property-value}

          override-props
          (assoc helpers/effect-log-node-init-props
            :basic-property :overridden-basic-property-value
            :effecting-property :overridden-effecting-property-value)

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct helpers/PropertyTestNode (assoc original-props :_node-id original-node-id)))
                  (g/override original-node-id {:init-props-fn (constantly override-props)})))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Can't clear property on original."
        (is (thrown?
              ExceptionInfo
              (g/transact
                (g/clear-property original-node-id :basic-property)))))

      (testing "Before transact."
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :overridden-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Transact."
        (g/transact
          (g/clear-property override-node-id :basic-property))
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :original-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Undo."
        (g/undo! graph-id)
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :overridden-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Redo."
        (g/redo! graph-id)
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :original-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output)))))))

(deftest effecting-property-undo-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          original-props
          {:basic-property :original-basic-property-value
           :effecting-property :original-effecting-property-value}

          override-props
          (assoc helpers/effect-log-node-init-props
            :basic-property :overridden-basic-property-value
            :effecting-property :overridden-effecting-property-value)

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct helpers/PropertyTestNode (assoc original-props :_node-id original-node-id)))
                  (g/override original-node-id {:init-props-fn (constantly override-props)})))))

          ensure-before!
          (fn ensure-before! []
            (testing "Original node."
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
                     (g/node-value original-node-id :effect-log-output))))

            (testing "Override node."
              (is (= :overridden-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :overridden-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:prop-kw :effecting-property
                       :old-value nil
                       :new-value :overridden-effecting-property-value}]
                     (g/node-value override-node-id :effect-log-property)
                     (g/node-value override-node-id :effect-log-output)))))

          ensure-after!
          (fn ensure-after! []
            (testing "Original node."
              (is (= :new-original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= [{:prop-kw :effecting-property
                       :old-value nil
                       :new-value :original-effecting-property-value}]
                     (g/node-value original-node-id :effect-log-property)
                     (g/node-value original-node-id :effect-log-output))))

            (testing "Override node."
              (is (= :overridden-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:prop-kw :effecting-property
                       :old-value nil
                       :new-value :overridden-effecting-property-value}
                      {:prop-kw :effecting-property
                       :old-value :overridden-effecting-property-value
                       :new-value nil}]
                     (g/node-value override-node-id :effect-log-property)
                     (g/node-value override-node-id :effect-log-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact."
        (ensure-before!))

      (testing "Setter is called during transact."
        (binding [helpers/property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= override-node-id self))
                    (is (= :overridden-effecting-property-value old-value))
                    (is (nil? new-value))
                    (is (= :new-original-basic-property-value
                           (g/node-value original-node-id :basic-property evaluation-context)
                           (g/node-value original-node-id :basic-output evaluation-context))
                        "Called with in-transaction evaluation-context.")
                    (is (= :original-effecting-property-value
                           (g/node-value override-node-id :effecting-property evaluation-context)
                           (g/node-value override-node-id :effecting-output evaluation-context))
                        "Backing property was cleared before invoking setter."))]
          (g/transact
            (concat
              (g/set-property original-node-id :basic-property :new-original-basic-property-value)
              (g/clear-property override-node-id :effecting-property)))))

      (testing "After transact."
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))

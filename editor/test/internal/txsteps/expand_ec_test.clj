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

(ns internal.txsteps.expand-ec-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest expand-ec-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          node-id (g/make-node! graph-id helpers/PropertyTestNode)
          tx-data-context-map {}]

      (testing "Transact."
        (g/transact
          {:tx-data-context-map tx-data-context-map}
          (concat
            (g/set-property node-id :basic-property :prior-basic-property-value)
            (g/expand-ec
              (fn expanded-tx-steps [evaluation-context node-id property-label value]
                (is (g/evaluation-context? evaluation-context))
                (is (identical? tx-data-context-map (deref (:tx-data-context evaluation-context))))
                (is (= :prior-basic-property-value (g/node-value node-id :basic-property evaluation-context)))
                (g/set-property node-id property-label value))
              node-id
              :basic-property
              :basic-property-value)))
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))))))

(deftest expand-ec-within-expanded-tx-steps-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          node-id (g/make-node! graph-id helpers/PropertyTestNode)
          tx-data-context-map {}]

      (testing "Transact."
        (g/transact
          {:tx-data-context-map tx-data-context-map}
          (g/expand-ec
            (fn expanded-tx-steps [evaluation-context node-id]
              (is (g/evaluation-context? evaluation-context))
              (is (identical? tx-data-context-map (deref (:tx-data-context evaluation-context))))
              (swap! (:tx-data-context evaluation-context) assoc :added-key :added-value)
              (concat
                (g/set-property node-id :basic-property :basic-property-value)
                (g/expand-ec
                  (fn nested-expanded-tx-steps [evaluation-context node-id]
                    (is (g/evaluation-context? evaluation-context))
                    (is (= {:added-key :added-value} (deref (:tx-data-context evaluation-context))))
                    (is (= :basic-property-value (g/node-value node-id :basic-property evaluation-context)))
                    (g/set-property node-id :effecting-property :effecting-property-value))
                  node-id)))
            node-id))
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= :effecting-property-value
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= nil
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= :effecting-property-value
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output)))))))

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

(ns internal.txsteps.expand-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest expand-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          node-id (g/make-node! graph-id helpers/PropertyTestNode)]

      (testing "Transact."
        (g/transact
          (g/expand
            (fn expanded-tx-steps [node-id property-label value]
              (g/set-property node-id property-label value))
            node-id
            :basic-property
            :basic-property-value))
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

(deftest expand-within-expanded-tx-steps-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          node-id (g/make-node! graph-id helpers/PropertyTestNode)]

      (testing "Transact."
        (g/transact
          (g/expand
            (fn expanded-tx-steps [node-id]
              (concat
                (g/set-property node-id :basic-property :basic-property-value)
                (g/expand
                  (fn nested-expanded-tx-steps [node-id]
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

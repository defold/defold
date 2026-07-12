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

(ns internal.txsteps.invalidate-test
  (:require [clojure.set :as set]
            [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest evicts-cache-entries-associated-with-invalidated-node-outputs-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          invalidated-node-id (g/make-node! graph-id helpers/PropertyTestNode
                                :basic-property :invalidated-basic-property-value
                                :effecting-property :invalidated-effecting-property-value)
          unaffected-node-id (g/make-node! graph-id helpers/PropertyTestNode
                               :basic-property :unaffected-basic-property-value
                               :effecting-property :unaffected-effecting-property-value)
          invalidated-node-output-endpoints (test-support/cacheable-endpoints invalidated-node-id)
          unaffected-node-output-endpoints (test-support/cacheable-endpoints unaffected-node-id)
          output-endpoints (set/union invalidated-node-output-endpoints
                                      unaffected-node-output-endpoints)]

      (testing "Cached before transact."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints)))))

      (testing "Transact."
        (let [tx-result (g/transact
                          (g/invalidate invalidated-node-id))]
          (is (= invalidated-node-output-endpoints
                 (set/intersection output-endpoints (:outputs-modified tx-result))))
          (is (= unaffected-node-output-endpoints
                 (set/intersection output-endpoints (test-support/cached-endpoints)))))))))

(deftest evicts-cache-entries-associated-with-explicit-successor-outputs-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id
           target-node-id
           downstream-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                 target-node-id helpers/ConnectionTargetNode
                 downstream-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :regular-input)
                (g/connect target-node-id :regular-output downstream-node-id :regular-input))))

          source-node-output-endpoints (test-support/cacheable-endpoints source-node-id)
          direct-successor-output-endpoint (g/endpoint target-node-id :regular-output)
          transitive-successor-output-endpoint (g/endpoint downstream-node-id :regular-output)
          output-endpoints (set/union source-node-output-endpoints
                                      #{direct-successor-output-endpoint
                                        transitive-successor-output-endpoint})]

      (testing "Cached before transact."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints)))))

      (testing "Transact."
        (let [tx-result (g/transact
                          (g/invalidate source-node-id))]
          (is (= output-endpoints
                 (set/intersection output-endpoints (:outputs-modified tx-result))))
          (is (= #{}
                 (set/intersection output-endpoints (test-support/cached-endpoints)))))))))

(deftest evicts-cache-entries-associated-with-implicit-override-successor-outputs-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [owner-node-id
           directly-owned-node-id
           indirectly-owned-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [owner-node-id [helpers/OverrideTestNode :property :owner-property-value]
                 directly-owned-node-id [helpers/OverrideTestNode :property :directly-owned-property-value]
                 indirectly-owned-node-id [helpers/OverrideTestNode :property :indirectly-owned-property-value]]
                (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id))))

          [first-order-override-owner-node-id] (g/overrides owner-node-id)
          [first-order-override-directly-owned-node-id] (g/overrides directly-owned-node-id)
          [first-order-override-indirectly-owned-node-id] (g/overrides indirectly-owned-node-id)

          invalidated-node-output-endpoints (test-support/cacheable-endpoints first-order-override-indirectly-owned-node-id)
          direct-successor-output-endpoint (g/endpoint first-order-override-directly-owned-node-id :regular-cascade-delete-output)
          transitive-successor-output-endpoint (g/endpoint first-order-override-owner-node-id :regular-cascade-delete-output)
          output-endpoints (set/union invalidated-node-output-endpoints
                                      #{direct-successor-output-endpoint
                                        transitive-successor-output-endpoint})]

      (testing "Implicit connections."
        (let [basis (g/now)]
          (is (g/connected? basis
                            first-order-override-indirectly-owned-node-id :property-output
                            first-order-override-directly-owned-node-id :regular-cascade-delete-input))
          (is (g/connected? basis
                            first-order-override-directly-owned-node-id :regular-cascade-delete-output
                            first-order-override-owner-node-id :regular-cascade-delete-input))))

      (testing "Cached before transact."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints)))))

      (testing "Transact."
        (let [tx-result (g/transact
                          (g/invalidate first-order-override-indirectly-owned-node-id))]
          (is (= output-endpoints
                 (set/intersection output-endpoints (:outputs-modified tx-result))))
          (is (= #{}
                 (set/intersection output-endpoints (test-support/cached-endpoints)))))))))

(deftest invalidates-node-outputs-on-undo-and-redo-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          invalidated-node-id (g/make-node! graph-id helpers/PropertyTestNode
                                :basic-property :invalidated-basic-property-value
                                :effecting-property :invalidated-effecting-property-value)
          unaffected-node-id (g/make-node! graph-id helpers/PropertyTestNode
                               :basic-property :unaffected-basic-property-value
                               :effecting-property :unaffected-effecting-property-value)
          invalidated-node-output-endpoints (test-support/cacheable-endpoints invalidated-node-id)
          unaffected-node-output-endpoints (test-support/cacheable-endpoints unaffected-node-id)
          output-endpoints (set/union invalidated-node-output-endpoints
                                      unaffected-node-output-endpoints)]

      (testing "Cached before transact."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints)))))

      (testing "Transact."
        (let [tx-result (g/transact
                          (g/invalidate invalidated-node-id))]
          (is (= 1 (count (:undoable-changes tx-result))))
          (is (= unaffected-node-output-endpoints
                 (set/intersection output-endpoints (test-support/cached-endpoints))))))

      (testing "Undo."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints))))
        (g/undo! :undo/global)
        (is (= unaffected-node-output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints)))))

      (testing "Redo."
        (helpers/encache-endpoints! output-endpoints)
        (is (= output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints))))
        (g/redo! :undo/global)
        (is (= unaffected-node-output-endpoints
               (set/intersection output-endpoints (test-support/cached-endpoints))))))))

;; Copyright 2020-2025 The Defold Foundation
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

(ns dynamo.integration.garbage-collection
"Garbage disposal of nodes on the dynamo graph level"
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode EmptyNode)

(defn gnodes [world]
  (-> (g/graph world) :nodes vals))

(deftest test-deleting-nodes
  (testing "adding one node and deleting it"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world EmptyNode))]
        (is (= 1 (count (gnodes world))))
        (g/transact (g/delete-node node))
        (is (= 0 (count (gnodes world)))))))

  (testing "adding twos node and deleting one"
      (with-clean-system
        (let [[node1 node2] (tx-nodes (g/make-node world EmptyNode)
                                      (g/make-node world EmptyNode))
              graph-nodes (-> (g/graph world) :nodes vals)]
          (is (= 2 (count (gnodes world) )))
          (g/transact (g/delete-node node1))
          (is (= 1 (count (gnodes world)))))))

  (testing "adding twos node and deleting one, then adding it back"
    (with-clean-system
      (let [[node1 node2] (tx-nodes (g/make-node world EmptyNode)
                                    (g/make-node world EmptyNode))
            graph-nodes (-> (g/graph world) :nodes vals)]
        (is (= 2 (count (gnodes world))))
        (g/transact (g/delete-node node1))
        (is (= 1 (count (gnodes world))))
        (g/transact (g/make-node world EmptyNode))
        (is (= 2 (count (gnodes world)))))))

  (testing "adding twos node and deleting one, then adding it back and deleting it"
    (with-clean-system
      (let [[node1 node2] (tx-nodes (g/make-node world EmptyNode)
                                    (g/make-node world EmptyNode))
            graph-nodes (-> (g/graph world) :nodes vals)]
        (is (= 2 (count (gnodes world))))
        (g/transact (g/delete-node node1))
        (is (= 1 (count (gnodes world))))
        (let [[node3] (tx-nodes (g/make-node world EmptyNode))]
          (is (= 2 (count (gnodes world))))
          (g/transact (g/delete-node node3))
          (is (= 1 (count (gnodes world))))))))

  (testing "adding 100 nodes, then deleting 50"
    (with-clean-system
      (let [nodes (g/tx-nodes-added (g/transact
                                     (repeatedly 100 #(g/make-node world EmptyNode))) )]
        (is (= 100 (count (gnodes world))))
        (g/transact (mapv #(g/delete-node %) (take 50 nodes)))
        (is (= 50 (count (gnodes world))))))))

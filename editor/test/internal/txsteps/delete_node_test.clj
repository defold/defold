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

(ns internal.txsteps.delete-node-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [support.test-support :as test-support]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(defn- node-info [basis node-id]
  {:node-id node-id
   :node-type-kw (g/node-type-kw basis node-id)
   :override-original (g/override-original basis node-id)})

(defn- setup-ownership-hierarchy! [graph-id owner-node-type owned-node-type]
  (let [[owner-node-id
         regular-owned-node-id
         array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (g/make-nodes graph-id
              [owner-node-id owner-node-type
               regular-owned-node-id owned-node-type
               array-owned-node-id owned-node-type]
              (g/connect regular-owned-node-id :_node-id owner-node-id :regular-cascade-delete-input)
              (g/connect array-owned-node-id :_node-id owner-node-id :array-cascade-delete-input))))]

    {:owner-node-id owner-node-id
     :regular-owned-node-id regular-owned-node-id
     :array-owned-node-id array-owned-node-id}))

(defn- setup-override-hierarchy! [graph-id owner-node-type owned-node-type]
  (let [{:keys [owner-node-id
                regular-owned-node-id
                array-owned-node-id]}
        (setup-ownership-hierarchy! graph-id owner-node-type owned-node-type)

        [first-order-override-owner-node-id
         first-order-override-regular-owned-node-id
         first-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (g/override owner-node-id)))

        [second-order-override-owner-node-id
         second-order-override-regular-owned-node-id
         second-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (g/override first-order-override-owner-node-id)))

        [first-order-override-first-order-override-regular-owned-node-id
         first-order-override-first-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (concat
              (g/override first-order-override-regular-owned-node-id)
              (g/override first-order-override-array-owned-node-id))))

        [second-order-override-first-order-override-regular-owned-node-id
         second-order-override-first-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (concat
              (g/override first-order-override-first-order-override-regular-owned-node-id)
              (g/override first-order-override-first-order-override-array-owned-node-id))))

        [first-order-override-second-order-override-regular-owned-node-id
         first-order-override-second-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (concat
              (g/override second-order-override-regular-owned-node-id)
              (g/override second-order-override-array-owned-node-id))))

        [second-order-override-second-order-override-regular-owned-node-id
         second-order-override-second-order-override-array-owned-node-id]
        (g/tx-nodes-added
          (g/transact
            (concat
              (g/override first-order-override-second-order-override-regular-owned-node-id)
              (g/override first-order-override-second-order-override-array-owned-node-id))))]

    (testing "Setup successful."
      (is (= owner-node-id (g/override-original first-order-override-owner-node-id)))
      (is (= regular-owned-node-id (g/override-original first-order-override-regular-owned-node-id)))
      (is (= array-owned-node-id (g/override-original first-order-override-array-owned-node-id)))
      (is (= first-order-override-owner-node-id (g/override-original second-order-override-owner-node-id)))
      (is (= first-order-override-regular-owned-node-id (g/override-original second-order-override-regular-owned-node-id)))
      (is (= first-order-override-array-owned-node-id (g/override-original second-order-override-array-owned-node-id)))
      (is (= first-order-override-regular-owned-node-id (g/override-original first-order-override-first-order-override-regular-owned-node-id)))
      (is (= first-order-override-array-owned-node-id (g/override-original first-order-override-first-order-override-array-owned-node-id)))
      (is (= first-order-override-first-order-override-regular-owned-node-id (g/override-original second-order-override-first-order-override-regular-owned-node-id)))
      (is (= first-order-override-first-order-override-array-owned-node-id (g/override-original second-order-override-first-order-override-array-owned-node-id)))
      (is (= first-order-override-first-order-override-regular-owned-node-id (g/override-original second-order-override-first-order-override-regular-owned-node-id)))
      (is (= first-order-override-first-order-override-array-owned-node-id (g/override-original second-order-override-first-order-override-array-owned-node-id)))
      (is (= first-order-override-second-order-override-regular-owned-node-id (g/override-original second-order-override-second-order-override-regular-owned-node-id)))
      (is (= first-order-override-second-order-override-array-owned-node-id (g/override-original second-order-override-second-order-override-array-owned-node-id))))

    {:owner-node-id owner-node-id
     :regular-owned-node-id regular-owned-node-id
     :array-owned-node-id array-owned-node-id
     :first-order-override-owner-node-id first-order-override-owner-node-id
     :first-order-override-regular-owned-node-id first-order-override-regular-owned-node-id
     :first-order-override-array-owned-node-id first-order-override-array-owned-node-id
     :second-order-override-owner-node-id second-order-override-owner-node-id
     :second-order-override-regular-owned-node-id second-order-override-regular-owned-node-id
     :second-order-override-array-owned-node-id second-order-override-array-owned-node-id
     :first-order-override-first-order-override-regular-owned-node-id first-order-override-first-order-override-regular-owned-node-id
     :first-order-override-first-order-override-array-owned-node-id first-order-override-first-order-override-array-owned-node-id
     :second-order-override-first-order-override-regular-owned-node-id second-order-override-first-order-override-regular-owned-node-id
     :second-order-override-first-order-override-array-owned-node-id second-order-override-first-order-override-array-owned-node-id
     :first-order-override-second-order-override-regular-owned-node-id first-order-override-second-order-override-regular-owned-node-id
     :first-order-override-second-order-override-array-owned-node-id first-order-override-second-order-override-array-owned-node-id
     :second-order-override-second-order-override-regular-owned-node-id second-order-override-second-order-override-regular-owned-node-id
     :second-order-override-second-order-override-array-owned-node-id second-order-override-second-order-override-array-owned-node-id}))

(g/defnode OwnedTestNode
  (output cached-output g/Any :cached (g/fnk [_node-id] _node-id)))

(g/defnode OwnerTestNode
  (output cached-output g/Any :cached (g/fnk [_node-id] _node-id))

  (input regular-cascade-delete-input g/Any :cascade-delete)
  (output regular-cascade-delete-output g/Any :cached
          (g/fnk [regular-cascade-delete-input] regular-cascade-delete-input))

  (input array-cascade-delete-input g/Any :array :cascade-delete)
  (output array-cascade-delete-output g/Any :cached
          (g/fnk [array-cascade-delete-input] array-cascade-delete-input)))

(deftest deletes-nodes-from-graph-test
  (test-support/with-clean-system
    (let [node-ids (sort (vals (setup-override-hierarchy! world OwnerTestNode OwnedTestNode)))]

      (testing "Before transact."
        (doseq [node-id node-ids]
          (let [node (g/node-by-id node-id)]
            (when (is (some? node))
              (is (= node-id (gt/node-id node)))))))

      (testing "Transact."
        (g/transact
          (mapv g/delete-node node-ids))
        (is (= {}
               (into {}
                     (keep (fn [node-id]
                             (when-some [node (g/node-by-id node-id)]
                               [node-id node])))
                     node-ids)))))))

(deftest returns-tx-result-with-nodes-deleted-test
  (test-support/with-clean-system
    (let [node-ids (sort (vals (setup-override-hierarchy! world OwnerTestNode OwnedTestNode)))
          basis-before (g/now)]
      (testing "Returns tx-result with nodes-deleted map."
        (is (= (into {}
                     (map (fn [node-id]
                            [node-id (g/node-by-id basis-before node-id)]))
                     node-ids)
               (:nodes-deleted
                 (g/transact
                   (map g/delete-node node-ids)))))))))

(deftest evicts-cache-entries-associated-with-deleted-nodes-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          key->node-id (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)
          node-id->node-info (partial node-info (g/now))

          cached-endpoints
          (coll/into-> key->node-id (sorted-set)
            (map #(g/endpoint (val %) :cached-output)))

          encache!
          (fn encache! []
            (doseq [endpoint cached-endpoints]
              (let [node-id (g/endpoint-node-id endpoint)
                    output-label (g/endpoint-label endpoint)]
                (g/node-value node-id output-label))))

          ensure-encached!
          (fn ensure-encached! []
            (is (= cached-endpoints (test-support/cached-endpoints))))

          ensure-evicted!
          (fn ensure-evicted! []
            (is (= []
                   (coll/into-> (test-support/cached-endpoints) []
                     (map (fn [endpoint]
                            (let [node-id (g/endpoint-node-id endpoint)
                                  output-label (g/endpoint-label endpoint)
                                  node-info (node-id->node-info node-id)]
                              (assoc node-info :output-label output-label))))))))]

      (testing "Encached before transact."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by transact."
        (g/transact
          (g/delete-node (:owner-node-id key->node-id)))
        (ensure-evicted!))

      (testing "Remains evicted after undo."
        (g/undo! graph-id)
        (ensure-evicted!))

      (testing "Encache at undo state before redo."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by redo."
        (g/redo! graph-id)
        (ensure-evicted!)))))

(deftest evicts-cache-entries-associated-with-successor-outputs-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)

          {:keys [owner-node-id
                  regular-owned-node-id
                  array-owned-node-id]}
          (setup-ownership-hierarchy! graph-id OwnerTestNode OwnedTestNode)

          first-order-override-owner-node-id
          (first
            (g/tx-nodes-added
              (g/transact
                (g/override owner-node-id))))

          second-order-override-owner-node-id
          (first
            (g/tx-nodes-added
              (g/transact
                (g/override first-order-override-owner-node-id))))

          node-id->node-info (partial node-info (g/now))

          successor-endpoints
          (into (sorted-set)
                (mapcat (fn [node-id]
                          [(g/endpoint node-id :regular-cascade-delete-output)
                           (g/endpoint node-id :array-cascade-delete-output)]))
                [owner-node-id
                 first-order-override-owner-node-id
                 second-order-override-owner-node-id])

          encache!
          (fn encache! []
            (doseq [endpoint successor-endpoints]
              (let [node-id (g/endpoint-node-id endpoint)
                    output-label (g/endpoint-label endpoint)]
                (g/node-value node-id output-label))))

          ensure-encached!
          (fn ensure-encached! []
            (is (= successor-endpoints (test-support/cached-endpoints))))

          ensure-evicted!
          (fn ensure-evicted! []
            (is (= []
                   (coll/into-> (test-support/cached-endpoints) []
                     (map (fn [endpoint]
                            (let [node-id (g/endpoint-node-id endpoint)
                                  output-label (g/endpoint-label endpoint)
                                  node-info (node-id->node-info node-id)]
                              (assoc node-info :output-label output-label))))))))]

      (testing "Encached before transact."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by transact."
        (g/transact
          (concat
            (g/delete-node regular-owned-node-id)
            (g/delete-node array-owned-node-id)))
        (ensure-evicted!))

      (testing "Remains evicted after undo."
        (g/undo! graph-id)
        (ensure-evicted!))

      (testing "Encache at undo state before redo."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by redo."
        (g/redo! graph-id)
        (ensure-evicted!)))))

(deftest undo-redo-node-deletion-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          {:keys [owner-node-id] :as key->node-id} (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)
          node-ids (sort (vals key->node-id))
          node-id->node-info (partial node-info (g/now))

          ensure-nodes-present-in-graph!
          (fn ensure-nodes-present-in-graph! []
            (testing "Nodes are present in the graph."
              (every? some? (map g/node-by-id node-ids))))

          ensure-nodes-absent-from-graph!
          (fn ensure-nodes-absent-from-graph! []
            (testing "Nodes are absent from the graph."
              (is (= []
                     (coll/into-> node-ids []
                       (filter g/node-by-id)
                       (map node-id->node-info))))))]

      (testing "Before transact."
        (ensure-nodes-present-in-graph!))

      (testing "Transact."
        (g/transact
          (g/delete-node owner-node-id))
        (ensure-nodes-absent-from-graph!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-nodes-present-in-graph!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-nodes-absent-from-graph!)))))

(deftest undo-redo-node-user-data-deletion-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          {:keys [owner-node-id] :as key->node-id} (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)

          ensure-user-data-present-in-system!
          (fn ensure-user-data-present-in-system! []
            (testing "User data is present in the system."
              (doseq [[key node-id] key->node-id]
                (is (= key (g/user-data node-id :key))))))

          ensure-user-data-absent-from-system!
          (fn ensure-user-data-absent-from-system! []
            (testing "User data is absent from the system."
              (doseq [[_key node-id] key->node-id]
                (is (nil? (g/user-data node-id :key))))))]

      ;; Associate user-data with each node-id.
      (doseq [[key node-id] key->node-id]
        (g/user-data! node-id :key key))

      (testing "Before transact."
        (ensure-user-data-present-in-system!))

      (testing "Transact."
        (g/transact
          (g/delete-node owner-node-id))
        (ensure-user-data-absent-from-system!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-user-data-present-in-system!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-user-data-absent-from-system!)))))

;; TODO(decouple-undo-from-graph): Formalize user-data semantics.
;;   There are several issues with how things work at the moment.
;;   1. Override nodes of deleted nodes are not included among the
;;      :nodes-deleted in the tx-result. This leads to their user-data remaining
;;      in the system even though the node itself is deleted.
;;   2. The user-data associated with a node is removed from the system when the
;;      node is deleted from its graph. However, if the graph has history, the
;;      deletion is undoable. But since user-data is outside of the undo system,
;;      undoing the deletion will not restore the user-data associated with the
;;      deleted node.
;;   It might be more correct to not delete user-data along with the node at
;;   all, and instead delete user-data explicitly. However, once we decouple the
;;   undo system from the graph state, we could potentially remove graph
;;   user-data as a concept and just store the information in regular
;;   properties.
(ns-unmap *ns* 'undo-redo-node-user-data-deletion-test)

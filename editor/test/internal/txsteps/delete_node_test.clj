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
  (:require [clojure.set :as set]
            [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

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
            (is (some? node))
            (is (= node-id (gt/node-id node))))))

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

(deftest deletes-multiple-nodes-with-single-change-test
  (test-support/with-clean-system
    (let [node-ids (sort (vals (setup-override-hierarchy! world OwnerTestNode OwnedTestNode)))
          tx-result (g/transact (g/delete-nodes node-ids))]
      (is (= 1 (count (:undoable-changes tx-result))))
      (is (= (set node-ids) (set (keys (:nodes-deleted tx-result)))))
      (is (= []
             (coll/into-> node-ids []
               (keep g/node-by-id))))

      (g/undo! :undo/global)
      (is (= (set node-ids)
             (coll/into-> node-ids #{}
               (filter g/node-by-id))))

      (g/redo! :undo/global)
      (is (= []
             (coll/into-> node-ids []
               (keep g/node-by-id)))))))

(deftest evicts-cache-entries-associated-with-deleted-nodes-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          key->node-id (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)
          node-id->key (set/map-invert key->node-id)

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
                     (map g/endpoint-node-id)
                     (map node-id->key)))))]

      (testing "Encached before transact."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by transact."
        (g/transact
          (g/delete-node (:owner-node-id key->node-id)))
        (ensure-evicted!))

      (testing "Remains evicted after undo."
        (g/undo! :undo/global)
        (ensure-evicted!))

      (testing "Encache at undo state before redo."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by redo."
        (g/redo! :undo/global)
        (ensure-evicted!)))))

(deftest evicts-cache-entries-associated-with-successor-outputs-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          {:as key->node-id
           :keys [owner-node-id
                  regular-owned-node-id
                  array-owned-node-id
                  first-order-override-owner-node-id
                  second-order-override-owner-node-id]}
          (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)

          node-id->key (set/map-invert key->node-id)

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
                     (map g/endpoint-node-id)
                     (map node-id->key)))))]

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
        (g/undo! :undo/global)
        (ensure-evicted!))

      (testing "Encache at undo state before redo."
        (encache!)
        (ensure-encached!))

      (testing "Evicted by redo."
        (g/redo! :undo/global)
        (ensure-evicted!)))))

(deftest undo-redo-node-deletion-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          {:keys [owner-node-id] :as key->node-id} (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)
          node-ids (sort (vals key->node-id))
          node-id->key (set/map-invert key->node-id)

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
                       (map node-id->key))))))]

      (testing "Before transact."
        (ensure-nodes-present-in-graph!))

      (testing "Transact."
        (g/transact
          (g/delete-node owner-node-id))
        (ensure-nodes-absent-from-graph!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-nodes-present-in-graph!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-nodes-absent-from-graph!)))))

(deftest undo-redo-node-user-data-deletion-test
  ;; TODO(decouple-undo-from-graph): Revise user-data semantics?
  ;;   The user-data associated with a node is removed from the system when the
  ;;   node is deleted from its graph. However, if the graph has history, the
  ;;   deletion is undoable. But since user-data is outside of the undo system,
  ;;   undoing the deletion will not restore the user-data associated with the
  ;;   deleted node.
  ;;   It might be more correct to not delete user-data along with the node at
  ;;   all, and instead delete user-data explicitly. However, once we decouple
  ;;   the undo system from the graph state, we could potentially remove graph
  ;;   user-data as a concept and just store the information in regular
  ;;   properties.
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)
          {:keys [owner-node-id] :as key->node-id} (setup-override-hierarchy! graph-id OwnerTestNode OwnedTestNode)
          node-id->key (coll/into-> key->node-id (sorted-map) (map coll/flip))

          current-user-data
          (fn current-user-data []
            (coll/into-> key->node-id (sorted-map)
              (map val)
              (keep (fn [node-id]
                      (when-some [user-data (g/user-data node-id :key)]
                        [node-id user-data])))))

          ensure-user-data-present-in-system!
          (fn ensure-user-data-present-in-system! []
            (testing "User data is present in the system."
              (is (= node-id->key (current-user-data)))))

          ensure-user-data-absent-from-system!
          (fn ensure-user-data-absent-from-system! []
            (testing "User data is absent from the system."
              (is (= {} (current-user-data)))))]

      ;; Associate user-data with each node-id.
      (doseq [[key node-id] key->node-id]
        (g/user-data! node-id :key key))

      (testing "Before transact."
        (ensure-user-data-present-in-system!))

      (testing "User data is removed by transact."
        (g/transact
          (g/delete-node owner-node-id))
        (ensure-user-data-absent-from-system!))

      (testing "User data is not restored by undo."
        (g/undo! :undo/global)
        (ensure-user-data-absent-from-system!)))))

(deftest undo-node-deletion-restores-connection-order-test
  (testing "Source arcs."
    (test-support/with-clean-system
      (let [graph-id (g/make-graph!)

            [source-node-id
             first-target-node-id
             second-target-node-id
             third-target-node-id]
            (g/tx-nodes-added
              (g/transact
                {:undoable false}
                (g/make-nodes graph-id
                  [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                   first-target-node-id helpers/ConnectionTargetNode
                   second-target-node-id helpers/ConnectionTargetNode
                   _third-target-node-id helpers/ConnectionTargetNode]
                  (g/connect source-node-id :property-output first-target-node-id :regular-input)
                  (g/connect source-node-id :property-output second-target-node-id :regular-input))))]

        (g/transact
          {:undo-key ::delete-target}
          (g/delete-node first-target-node-id))

        (g/transact
          {:undo-key ::connect-later-target}
          (g/connect source-node-id :property-output third-target-node-id :regular-input))

        (is (= [[second-target-node-id :regular-input]
                [third-target-node-id :regular-input]]
               (g/targets (g/now) source-node-id :property-output)))

        (g/undo! ::delete-target)

        (is (= [[first-target-node-id :regular-input]
                [second-target-node-id :regular-input]
                [third-target-node-id :regular-input]]
               (g/targets (g/now) source-node-id :property-output))))))

  (testing "Target arcs."
    (test-support/with-clean-system
      (let [graph-id (g/make-graph!)

            [first-source-node-id
             second-source-node-id
             third-source-node-id
             target-node-id]
            (g/tx-nodes-added
              (g/transact
                {:undoable false}
                (g/make-nodes graph-id
                  [first-source-node-id [helpers/ConnectionSourceNode :property :first-value]
                   second-source-node-id [helpers/ConnectionSourceNode :property :second-value]
                   _third-source-node-id [helpers/ConnectionSourceNode :property :third-value]
                   target-node-id helpers/ConnectionTargetNode]
                  (g/connect first-source-node-id :property-output target-node-id :array-input)
                  (g/connect second-source-node-id :property-output target-node-id :array-input))))]

        (g/transact
          {:undo-key ::delete-source}
          (g/delete-node second-source-node-id))

        (g/transact
          {:undo-key ::connect-later-source}
          (g/connect third-source-node-id :property-output target-node-id :array-input))

        (is (= [[first-source-node-id :property-output]
                [third-source-node-id :property-output]]
               (g/sources (g/now) target-node-id :array-input)))

        (g/undo! ::delete-source)

        (is (= [[first-source-node-id :property-output]
                [second-source-node-id :property-output]
                [third-source-node-id :property-output]]
               (g/sources (g/now) target-node-id :array-input)))))))

(deftest undo-node-deletion-preserves-empty-arc-table-next-pkid-test
  (testing "Source arcs."
    (test-support/with-clean-system
      (let [graph-id (g/make-graph!)

            [source-node-id
             first-target-node-id
             second-target-node-id]
            (g/tx-nodes-added
              (g/transact
                {:undoable false}
                (g/make-nodes graph-id
                  [source-node-id [helpers/ConnectionSourceNode :property :source-value]
                   first-target-node-id helpers/ConnectionTargetNode
                   _second-target-node-id helpers/ConnectionTargetNode]
                  (g/connect source-node-id :property-output first-target-node-id :regular-input))))]

        (g/transact
          {:undo-key ::disconnect-first-target}
          (g/disconnect source-node-id :property-output first-target-node-id :regular-input))

        (g/transact
          {:undo-key ::delete-source}
          (g/delete-node source-node-id))

        (g/undo! ::delete-source)

        (g/transact
          {:undo-key ::connect-second-target}
          (g/connect source-node-id :property-output second-target-node-id :regular-input))

        (g/undo! ::disconnect-first-target)

        (is (= [[first-target-node-id :regular-input]
                [second-target-node-id :regular-input]]
               (g/targets (g/now) source-node-id :property-output))))))

  (testing "Target arcs."
    (test-support/with-clean-system
      (let [graph-id (g/make-graph!)

            [first-source-node-id
             second-source-node-id
             target-node-id]
            (g/tx-nodes-added
              (g/transact
                {:undoable false}
                (g/make-nodes graph-id
                  [first-source-node-id [helpers/ConnectionSourceNode :property :first-value]
                   _second-source-node-id [helpers/ConnectionSourceNode :property :second-value]
                   target-node-id helpers/ConnectionTargetNode]
                  (g/connect first-source-node-id :property-output target-node-id :array-input))))]

        (g/transact
          {:undo-key ::disconnect-first-source}
          (g/disconnect first-source-node-id :property-output target-node-id :array-input))

        (g/transact
          {:undo-key ::delete-target}
          (g/delete-node target-node-id))

        (g/undo! ::delete-target)

        (g/transact
          {:undo-key ::connect-second-source}
          (g/connect second-source-node-id :property-output target-node-id :array-input))

        (g/undo! ::disconnect-first-source)

        (is (= [[first-source-node-id :property-output]
                [second-source-node-id :property-output]]
               (g/sources (g/now) target-node-id :array-input)))))))

(deftest undo-node-deletion-invalidates-restored-source-successors-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [source-node-id helpers/ConnectionSourceNode
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :regular-input))))

          successor-endpoint (g/endpoint target-node-id :regular-output)]

      (is (= #{successor-endpoint}
             (set (g/successors (g/now) source-node-id :property-output))))

      (g/transact
        (g/delete-node target-node-id))

      (is (= #{}
             (set (g/successors (g/now) source-node-id :property-output))))

      (g/undo! :undo/global)

      (is (= #{successor-endpoint}
             (set (g/successors (g/now) source-node-id :property-output)))))))

(deftest delete-override-node-invalidates-original-successors-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-node graph-id helpers/OverrideTestNode)))

          [override-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-node-id)))

          override-successor-endpoint (g/endpoint override-node-id :property-output)]

      (is (= #{override-successor-endpoint}
             (set (g/successors (g/now) original-node-id :property-output))))

      (g/transact
        (g/delete-node override-node-id))

      (is (= #{}
             (set (g/successors (g/now) original-node-id :property-output))))

      (g/undo! :undo/global)

      (is (= #{override-successor-endpoint}
             (set (g/successors (g/now) original-node-id :property-output)))))))

(deftest undo-delete-override-node-preserves-later-overrides-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [original-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-node graph-id helpers/OverrideTestNode)))

          [deleted-override-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-node-id)))]

      (g/transact
        {:undo-key ::delete-override}
        (g/delete-node deleted-override-node-id))

      (let [[later-override-node-id]
            (g/tx-nodes-added
              (g/transact
                {:undo-key ::later-override}
                (g/override original-node-id)))]

        (let [basis (g/now)]
          (is (= #{later-override-node-id} (set (g/overrides basis original-node-id))))
          (is (= nil (g/node-by-id basis deleted-override-node-id)))
          (is (= original-node-id (g/override-original basis later-override-node-id))))

        (g/undo! ::delete-override)

        (let [basis (g/now)]
          (is (= #{deleted-override-node-id later-override-node-id} (set (g/overrides basis original-node-id))))
          (is (= original-node-id (g/override-original basis deleted-override-node-id)))
          (is (= original-node-id (g/override-original basis later-override-node-id))))))))

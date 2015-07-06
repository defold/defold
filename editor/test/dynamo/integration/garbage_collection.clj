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
        (is (= 2 (count (gnodes world) )))
        (g/transact (g/delete-node node1))
        (is (= 1 (count (gnodes world))))
        (g/transact (g/make-node world EmptyNode))
        (is (= 2 (count (gnodes world)))))))

  (testing "adding twos node and deleting one, then adding it back and deleting it"
    (with-clean-system
      (let [[node1 node2] (tx-nodes (g/make-node world EmptyNode)
                                    (g/make-node world EmptyNode))
            graph-nodes (-> (g/graph world) :nodes vals)]
        (is (= 2 (count (gnodes world) )))
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


(def dispose-counter (atom 0))
(g/defnode DisposableNode
  g/IDisposable
  (dispose [this] (swap! dispose-counter inc)))

(deftest test-disposing-nodes
  (testing "adding one node, deleting it, and disposing"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world DisposableNode))]
        (g/transact (g/delete-node node))
        (is (= 0 @dispose-counter))
        (g/dispose-pending)
        (is (= 1 @dispose-counter)))))

  (testing "disposing twice only cleans up once for a deleted node"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world DisposableNode))]
        (g/transact (g/delete-node node))
        (is (= 0 @dispose-counter))
        (g/dispose-pending)
        (g/dispose-pending)
        (is (= 1 @dispose-counter)))))

  (testing "disposing if a node is not deleted does nothing"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world DisposableNode))]
        (is (= 0 @dispose-counter))
        (g/dispose-pending)
        (is (= 0 @dispose-counter)))))

  (testing "disposing lots of nodes"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [nodes (g/tx-nodes-added (g/transact
                                     (repeatedly 100 #(g/make-node world DisposableNode))) )]
        (g/transact (mapv #(g/delete-node %) (take 50 nodes)))
        (g/dispose-pending)
        (is (= 50 @dispose-counter)))))

  (testing "disposing after deleting node and undoing does not call dispose"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [my-graph (g/make-graph! :history true)
            [node] (tx-nodes (g/make-node my-graph DisposableNode))]
        (is (= 0 @dispose-counter))
        (g/transact (g/delete-node node))
        (g/undo my-graph)
        (g/dispose-pending)
        (is (= 0 @dispose-counter)))))

  (testing "disposing after deleting node and undo and redo does call dispose"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [my-graph (g/make-graph! :history true)
            [node] (tx-nodes (g/make-node my-graph DisposableNode))]
        (is (= 0 @dispose-counter))
        (g/transact (g/delete-node node))
        (g/undo my-graph)
        (g/redo my-graph)
        (g/dispose-pending)
        (is (= 1 @dispose-counter))))))

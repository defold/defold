(ns dynamo.integration.node-become
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))


(g/defnode InputOutputNode
  (output output-a g/Str (g/always "Cake is tasty"))
  (output output-b g/Str (g/always "Bread is tasty"))
  (input input-a g/Str)
  (output passthrough-a g/Str (g/fnk [input-a] (str "passthrough: " input-a))))

(g/defnode OriginalNode
  (property a-property g/Str (default "original-node-property"))
  (property z-property g/Str (default "zeee"))
  (input input-a g/Str)
  (input input-b g/Str)
  (output output-a g/Str (g/fnk [input-a] (str "original: " input-a)))
  (output output-b g/Str (g/fnk [input-b] (str "original: " input-b))))

(g/defnode NewNode
  (property a-property g/Str (default "new-node-property"))
  (property b-property g/Str (default "foo"))
  (input input-a g/Str)
  (output output-a g/Str (g/fnk [input-a] (str "new: " input-a))))

(deftest test-become

  (testing "turns one node-type into another node-type"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            _        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
            new-node (g/refresh node)]
        (is (= "OriginalNode" (:name (g/node-type node))))
        (is (= "NewNode" (:name (g/node-type new-node)))) )))

  (testing "node id is is the same"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            _        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
            new-node (g/refresh node)]
        (is (= (g/node-id node) (g/node-id new-node))))))

  (testing "properties from the original node are carried over if they exist on the new node"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            _        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
            new-node (g/refresh node)]
        (is (= "original-node-property" (g/node-value new-node :a-property))))))

  (testing "properties from the original node are removed if they don't exist on the new node"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))]
        (is (= "zeee" (g/node-value node :z-property)))
        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
        (let [new-node (g/refresh node)]
          (is (thrown-with-msg? Exception #"No such output, input, or property" (g/node-value new-node :z-property)))))))

  (testing "inputs from the original node are still connected if they exist on the new node"
    (with-clean-system
      (let [[node io-node]   (tx-nodes (g/make-node world OriginalNode)
                                       (g/make-node world InputOutputNode))]
        (g/transact (g/connect (g/node-id io-node) :output-a (g/node-id node) :input-a))
        (is (= "original: Cake is tasty" (g/node-value node :output-a)))
        (is (g/connected? (g/now) (g/node-id io-node) :output-a (g/node-id node) :input-a))
        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
        (let [new-node (g/refresh node)]
          (is (= "new: Cake is tasty" (g/node-value new-node :output-a)))
          (is (g/connected? (g/now) (g/node-id io-node) :output-a (g/node-id new-node) :input-a))))))

  (testing "inputs from the original node are disconnected if they don't exist on the new node"
    (with-clean-system
      (let [[node io-node]   (tx-nodes (g/make-node world OriginalNode)
                                       (g/make-node world InputOutputNode))]
        (is (not (g/connected? (g/now) (g/node-id io-node) :output-b (g/node-id node) :input-b)))
        (g/transact (g/connect (g/node-id io-node) :output-b (g/node-id node) :input-b))
        (is (= "original: Bread is tasty" (g/node-value node :output-b)))
        (is (g/connected? (g/now) (g/node-id io-node) :output-b (g/node-id node) :input-b))
        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
        (let [new-node (g/refresh node)]
          (is (thrown-with-msg? Exception #"No such output, input, or property" (g/node-value new-node :output-b)))
          (is (not (g/connected? (g/now) (g/node-id io-node) :output-b (g/node-id new-node) :input-b)))))))

  (testing "outputs from the original node are still connected if they exist on the new node"
    (with-clean-system
      (let [[node io-node]   (tx-nodes (g/make-node world OriginalNode)
                                       (g/make-node world InputOutputNode))]
        (g/transact (g/connect (g/node-id node) :a-property (g/node-id io-node) :input-a))
        (is (= "passthrough: original-node-property" (g/node-value io-node :passthrough-a)))
        (is (g/connected? (g/now) (g/node-id node) :a-property (g/node-id io-node) :input-a))
        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
        (let [new-node (g/refresh node)]
          (is (= "passthrough: original-node-property" (g/node-value io-node :passthrough-a)))
          (is (g/connected? (g/now) (g/node-id node) :a-property (g/node-id io-node) :input-a))))))

  (testing "outputs from the original node are disconnected if they don't exist on the new node"
    (with-clean-system
      (let [[node io-node]   (tx-nodes (g/make-node world OriginalNode)
                                       (g/make-node world InputOutputNode))]
        (g/transact (g/connect (g/node-id node) :z-property (g/node-id io-node) :input-a))
        (is (= "passthrough: zeee" (g/node-value io-node :passthrough-a)))
        (is (g/connected? (g/now) (g/node-id node) :z-property (g/node-id io-node) :input-a))
        (g/transact (g/become (g/node-id node) (g/construct NewNode)))
        (let [new-node (g/refresh node)]
          (is (= "passthrough: " (g/node-value io-node :passthrough-a)))
          (is (not (g/connected? (g/now) (g/node-id node) :z-property (g/node-id io-node) :input-a))))))))

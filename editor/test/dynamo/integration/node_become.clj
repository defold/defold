(ns dynamo.integration.node-become
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [dynamo.integration.node-become-support :refer :all]))

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

(g/defnode CachedOutputNode
  (property number g/Any (default (atom 0)))
  (output output-a g/Str :cached (g/fnk [number] (str "val" (swap! number inc)))))

(deftest test-become

  (testing "turns one node-type into another node-type"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            old-node (g/node-by-id node)
            _        (g/transact (g/become node (g/construct NewNode)))
            new-node (g/node-by-id node)]
        (is (= "dynamo.integration.node-become/OriginalNode" (:name (g/node-type old-node))))
        (is (= "dynamo.integration.node-become/NewNode" (:name (g/node-type new-node)))))))

  (testing "node id is is the same"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            old-node (g/node-by-id node)
            _        (g/transact (g/become node (g/construct NewNode)))
            new-node (g/node-by-id node)]
        (is (= (g/node-id old-node) (g/node-id new-node))))))

  (testing "properties from the original node are carried over if they exist on the new node"
    (with-clean-system
      (let [[node]   (tx-nodes (g/make-node world OriginalNode))
            _        (g/transact (g/become node (g/construct NewNode)))
            new-node (g/node-by-id node)]
        (is (= "original-node-property" (g/node-value new-node :a-property))))))

  (testing "properties from the original node are removed if they don't exist on the new node"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world OriginalNode))]
        (is (= "zeee" (g/node-value node :z-property)))
        (g/transact (g/become node (g/construct NewNode)))
        (is (thrown-with-msg? Exception #"No such output, input, or property"
                              (g/node-value node :z-property))))))

  (testing "inputs from the original node are still connected if they exist on the new node"
    (with-clean-system
      (let [[node io-node] (tx-nodes (g/make-node world OriginalNode)
                                     (g/make-node world InputOutputNode))]
        (g/transact (g/connect io-node :output-a node :input-a))
        (is (= "original: Cake is tasty" (g/node-value node :output-a)))
        (is (g/connected? (g/now) io-node :output-a node :input-a))
        (g/transact (g/become node (g/construct NewNode)))
        (is (= "new: Cake is tasty" (g/node-value node :output-a)))
        (is (g/connected? (g/now) io-node :output-a node :input-a)))))

  (testing "inputs from the original node are disconnected if they don't exist on the new node"
    (with-clean-system
      (let [[node io-node] (tx-nodes (g/make-node world OriginalNode)
                                     (g/make-node world InputOutputNode))]
        (is (not (g/connected? (g/now) io-node :output-b node :input-b)))
        (g/transact (g/connect io-node :output-b node :input-b))
        (is (= "original: Bread is tasty" (g/node-value node :output-b)))
        (is (g/connected? (g/now) io-node :output-b node :input-b))
        (g/transact (g/become node (g/construct NewNode)))
        (is (thrown-with-msg? Exception #"No such output, input, or property"
                              (g/node-value node :output-b)))
        (is (not (g/connected? (g/now) io-node :output-b node :input-b))))))

  (testing "outputs from the original node are still connected if they exist on the new node"
    (with-clean-system
      (let [[node io-node] (tx-nodes (g/make-node world OriginalNode)
                                     (g/make-node world InputOutputNode))]
        (g/transact (g/connect node :a-property io-node :input-a))
        (is (= "passthrough: original-node-property" (g/node-value io-node :passthrough-a)))
        (is (g/connected? (g/now) node :a-property io-node :input-a))
        (g/transact (g/become node (g/construct NewNode)))
        (is (= "passthrough: original-node-property" (g/node-value io-node :passthrough-a)))
        (is (g/connected? (g/now) node :a-property io-node :input-a)))))

  (testing "outputs from the original node are disconnected if they don't exist on the new node"
    (with-clean-system
      (let [[node io-node] (tx-nodes (g/make-node world OriginalNode)
                                     (g/make-node world InputOutputNode))]
        (g/transact (g/connect node :z-property io-node :input-a))
        (is (= "passthrough: zeee" (g/node-value io-node :passthrough-a)))
        (is (g/connected? (g/now) node :z-property io-node :input-a))
        (g/transact (g/become node (g/construct NewNode)))
        (is (= "passthrough: " (g/node-value io-node :passthrough-a)))
        (is (not (g/connected? (g/now) node :z-property io-node :input-a)))))))

(deftest test-caching
  (testing "caching is invalidated during become"
    (with-clean-system
      (let [[out-node in-node] (tx-nodes (g/make-nodes world [out-node CachedOutputNode
                                                              in-node NewNode]
                                           (g/connect out-node :output-a in-node :input-a)))
            val-fn (fn [] (g/node-value in-node :output-a))
            original-val (val-fn)]
        (is (= original-val (val-fn)))
        (g/transact (g/become out-node (g/construct CachedOutputNode)))
        (is (not= original-val (val-fn)))))))

(deftest test-reload
  (with-clean-system
    (require 'dynamo.integration.node-become-support :reload)
      (let [[node-a node-b] (tx-nodes (g/make-nodes world [node-a NodeA
                                                           node-b NodeB]
                                        (g/connect node-a :output-a node-b :input-b)
                                        (g/connect node-b :output-b node-a :input-a)))
            node-a-type (g/node-type* node-a)]
        (is (g/node-instance? NodeA node-a))
        ;; RELOAD
        (require 'dynamo.integration.node-become-support :reload)
        (is (g/node-instance? NodeA node-a))
        (is (not= node-a-type (g/node-type* node-a))))))

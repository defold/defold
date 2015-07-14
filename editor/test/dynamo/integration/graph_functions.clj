(ns dynamo.integration.graph-functions
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode SourceNode
  (property a-property g/Str)
  (output an-output g/Str (g/always "An output")))

(g/defnode SinkNode
  (input an-input g/Str)
  (input an-array-input g/Str :array)
  (output an-output g/Str (g/fnk [an-input] (str an-input " except in Florida")))
  (output an-array-output [g/Str] (g/fnk [an-array-input] an-array-input)))

(g/defnode OutputOnlyNode
  (output an-ouput g/Str (g/always "empty")))

(g/defnode ChainedOutputNode
  (property foo g/Str (default "c"))
  (output a-output g/Str (g/fnk [foo] (str foo "a")))
  (output b-output g/Str (g/fnk [a-output] (str a-output "k")))
  (output c-output g/Str (g/fnk [b-output] (str b-output "e"))))

(deftest test-inputs
  (testing "inputs know about nodes externally connected to them"
    (with-clean-system
      (let [[source sink] (tx-nodes (g/make-node world SourceNode)
                                    (g/make-node world SinkNode))
            source-id     (g/node-id source)
            sink-id       (g/node-id sink)]
        (g/transact (g/connect source :an-output sink :an-input))
        (is (= [[source-id :an-output sink-id :an-input]] (g/inputs sink-id)))))))

(defn- has-no-inputs
  [ntype]
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world ntype))
          nid    (g/node-id node)]
      (empty? (g/inputs nid)))))

(deftest test-empty-inputs
  (testing "nodes without defined inputs do not have results"
    (is (has-no-inputs OutputOnlyNode)))
  (testing "nodes with unconnected inputs do not have results"
    (is (has-no-inputs SinkNode))
    (is (has-no-inputs ChainedOutputNode))))

(deftest test-outputs
  (testing "outputs know about nodes connected to them"
    (with-clean-system
      (let [[source sink] (tx-nodes (g/make-node world SourceNode)
                                    (g/make-node world SinkNode))
            source-id     (g/node-id source)
            sink-id       (g/node-id sink)]
        (g/transact (g/connect source :an-output sink :an-input))
        (is (= [[source-id :an-output sink-id :an-input]] (g/outputs source-id)))))))

(defn- has-no-outputs
  [ntype]
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world ntype))
          nid    (g/node-id node)]
      (empty? (g/outputs nid)))))

(deftest test-empty-outputs
  (testing "nodes without defined outputs do not have results"
    (is (has-no-outputs OutputOnlyNode)))
  (testing "nodes with unconnected outputs do not have results"
    (is (has-no-outputs SinkNode))
    (is (has-no-outputs ChainedOutputNode))))


(deftest chained-connected-input-nodes
  (testing "only immediate node inputs should be returned"
    (with-clean-system
      (let [[source sink1 sink2] (tx-nodes (g/make-node world SourceNode)
                                           (g/make-node world SinkNode)
                                           (g/make-node world SinkNode))
            source-id            (g/node-id source)
            sink1-id             (g/node-id sink1)
            sink2-id             (g/node-id sink2)]
        (g/transact (g/connect source :an-output sink1 :an-input))
        (g/transact (g/connect sink1 :an-output sink2 :an-input))
        (is (= [[sink1-id :an-output sink2-id :an-input]] (g/inputs sink2-id)))))))


(deftest array-input-nodes
  (testing "multiple connections to a single input"
    (with-clean-system
      (let [[source1 source2 sink] (tx-nodes (g/make-node world SourceNode)
                                             (g/make-node world SourceNode)
                                             (g/make-node world SinkNode))
            source1-id             (g/node-id source1)
            source2-id             (g/node-id source2)
            sink-id                (g/node-id sink)]
        (g/transact (g/connect source1 :an-output sink :an-array-input))
        (g/transact (g/connect source2 :an-output sink :an-array-input))
        (is (= [[source1-id :an-output sink-id :an-array-input]
                [source2-id :an-output sink-id :an-array-input]]
               (g/inputs sink-id)))))))

(deftest multiple-consumer-output-nodes
  (testing "multiple connections to a single input"
    (with-clean-system
      (let [[source sink1 sink2] (tx-nodes (g/make-node world SourceNode)
                                           (g/make-node world SinkNode)
                                           (g/make-node world SinkNode))
            source-id            (g/node-id source)
            sink1-id             (g/node-id sink1)
            sink2-id             (g/node-id sink2)]
        (g/transact (g/connect source :an-output sink1 :an-input))
        (g/transact (g/connect source :an-output sink2 :an-input))
        (is (= [[source-id :an-output sink1-id :an-input]
                [source-id :an-output sink2-id :an-input]]
               (g/outputs source-id)))))))

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
      (let [[source-id sink-id] (tx-nodes (g/make-node world SourceNode)
                                          (g/make-node world SinkNode))]
        (g/transact (g/connect source-id :an-output sink-id :an-input))
        (is (= [[source-id :an-output sink-id :an-input]] (g/inputs sink-id)))))))

(defn- has-no-inputs
  [ntype]
  (with-clean-system
    (let [[nid] (tx-nodes (g/make-node world ntype))]
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
      (let [[source-id sink-id] (tx-nodes (g/make-node world SourceNode)
                                          (g/make-node world SinkNode))]
        (g/transact (g/connect source-id :an-output sink-id :an-input))
        (is (= [[source-id :an-output sink-id :an-input]] (g/outputs source-id)))))))

(defn- has-no-outputs
  [ntype]
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world ntype))]
      (empty? (g/outputs node)))))

(deftest test-empty-outputs
  (testing "nodes without defined outputs do not have results"
    (is (has-no-outputs OutputOnlyNode)))
  (testing "nodes with unconnected outputs do not have results"
    (is (has-no-outputs SinkNode))
    (is (has-no-outputs ChainedOutputNode))))


(deftest chained-connected-input-nodes
  (testing "only immediate node inputs should be returned"
    (with-clean-system
      (let [[source-id sink1-id sink2-id] (tx-nodes (g/make-node world SourceNode)
                                                    (g/make-node world SinkNode)
                                                    (g/make-node world SinkNode))]
        (g/transact (g/connect source-id :an-output sink1-id :an-input))
        (g/transact (g/connect sink1-id :an-output sink2-id :an-input))
        (is (= [[sink1-id :an-output sink2-id :an-input]] (g/inputs sink2-id)))))))


(deftest array-input-nodes
  (testing "multiple connections to a single input"
    (with-clean-system
      (let [[source1-id source2-id sink-id] (tx-nodes (g/make-node world SourceNode)
                                                      (g/make-node world SourceNode)
                                                      (g/make-node world SinkNode))]
        (g/transact (g/connect source1-id :an-output sink-id :an-array-input))
        (g/transact (g/connect source2-id :an-output sink-id :an-array-input))
        (is (= [[source1-id :an-output sink-id :an-array-input]
                [source2-id :an-output sink-id :an-array-input]]
               (g/inputs sink-id)))))))

(deftest multiple-consumer-output-nodes
  (testing "multiple connections to a single input"
    (with-clean-system
      (let [[source-id sink1-id sink2-id] (tx-nodes (g/make-node world SourceNode)
                                                    (g/make-node world SinkNode)
                                                    (g/make-node world SinkNode))]
        (g/transact (g/connect source-id :an-output sink1-id :an-input))
        (g/transact (g/connect source-id :an-output sink2-id :an-input))
        (is (= [[source-id :an-output sink1-id :an-input]
                [source-id :an-output sink2-id :an-input]]
               (g/outputs source-id)))))))

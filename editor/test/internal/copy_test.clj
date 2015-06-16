(ns internal.copy-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :as ts]
            [dynamo.types :as t]))

;;  simple copy: two nodes, one arc
(g/defnode ConsumerNode
  (property a-property t/Str (default "foo"))
  (input consumes-value t/Str :array))

(g/defnode ProducerNode
  (output produces-value t/Str (g/fnk [] "A string")))

(g/defnode ConsumeAndProduceNode
  (inherits ConsumerNode)
  (inherits ProducerNode))

;; (comment
;;   {:roots #{id-of-the-most-important-copied-node}
;;    :nodes #{{:serial-id 'id1' :node-type NodeType}
;;             {:serial-id 'id2' :node-type NodeType
;;              :properties {property-name property-value}}
;;             :arcs  #{[#producer{ id2 :bar} #consumer{id1 :foo}]
;;                      [#producer{ id2 :bar} #consumer{id2 :bar} "proj/local/path.png"]
;;                      [#resource{ "/images/1.jpg" :image} #consumer{ id1 :images}] }}}

;;   )

(deftest simple-copy
  (ts/with-clean-system
    (let [[node1 node2] (ts/tx-nodes (g/make-node world ConsumerNode)
                                     (g/make-node world ProducerNode))
          node1-id      (g/node-id node1)
          node2-id      (g/node-id node2)]
      (g/transact
       (g/connect node2 :produces-value node1 :consumes-value))

      (let [fragment            (g/copy [node1-id] (constantly false))
            fragment-nodes      (:nodes fragment)
            serial-ids          (map :serial-id fragment-nodes)
            arc-node-references (into #{} (concat (map #(:node-id (first %))  (:arcs fragment))
                                                  (map #(:node-id (second %)) (:arcs fragment))))]
        (is (= 1 (count (:roots fragment))))
        (is (every? integer? (:roots fragment)))
        (is (every? integer? serial-ids))
        (is (= (count serial-ids) (count (distinct serial-ids))))
        (is (= [ConsumerNode ProducerNode] (map :node-type fragment-nodes)))
        (is (= {:a-property "foo"} (:properties (first fragment-nodes))))
        (is (= 1 (count (:arcs fragment))))
        (is (every? #(satisfies? g/Producer %) (map first (:arcs fragment))))
        (is (every? #(satisfies? g/Consumer %) (map second (:arcs fragment))))
        (is (empty? (set/difference (set arc-node-references) (set serial-ids))))))))


(deftest diamond-copy
  (ts/with-clean-system
    (let [[node1 node2 node3 node4] (ts/tx-nodes (g/make-node world ConsumerNode)
                                                 (g/make-node world ConsumeAndProduceNode)
                                                 (g/make-node world ConsumeAndProduceNode)
                                                 (g/make-node world ProducerNode) )
          node1-id (g/node-id node1)]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node1 :consumes-value)
        (g/connect node4 :produces-value node3 :consumes-value)
        (g/connect node4 :produces-value node2 :consumes-value)])
      (let [fragment            (g/copy [node1-id] (constantly true))
            fragment-nodes      (:nodes fragment)
            serial-ids          (map :serial-id fragment-nodes)
            arc-node-references (into #{} (concat (map #(:node-id (first %))  (:arcs fragment))
                                                  (map #(:node-id (second %)) (:arcs fragment))))]
        (is (= 1 (count (:roots fragment))))
        (is (every? integer? (:roots fragment)))
        (is (every? integer? serial-ids))
        (is (= (count serial-ids) (count (distinct serial-ids))))
        (is (= 4 (count (:arcs fragment))))
        (is (= 4 (count fragment-nodes)))
        (is (every? #(satisfies? g/Producer %) (map first (:arcs fragment))))
        (is (every? #(satisfies? g/Consumer %) (map second (:arcs fragment))))
        (is (empty? (set/difference (set arc-node-references) (set serial-ids))))))))

;;  diamond copy: four nodes in a diamond formation.
;;  short circuit an infinite cycle
;;  cross graph copy: two nodes, in different graphs, cuts off at graph boundary
;;  resource: two nodes, one arc, upstream is a resource
;;  no functions: one node, a property which is a function, should not be serialized

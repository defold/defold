(ns internal.copy-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :as ts]
            [dynamo.types :as t]))

;;  simple copy: two nodes, one arc
(g/defnode ConsumerNode
  (property a-property t/Str (default "foo"))
  (input consumes-value t/Str))

(g/defnode ProducerNode
  (output produces-value t/Str (g/fnk [] "A string")))


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

      (let [fragment (g/copy [node1-id] (constantly false))
            fragment-nodes (:nodes fragment)]
        (is (= [0]  (:roots fragment)))
        (is (= [0 1] (map :serial-id fragment-nodes)))
        (is (= [ConsumerNode ProducerNode] (map :node-type fragment-nodes)))
        (is (= {:a-property "foo"} (:properties (first fragment-nodes))))
        (is (= 1 (count (:arcs fragment))))))))


;;  diamond copy: four nodes in a diamond formation.
;;  short circuit an infinite cycle
;;  cross graph copy: two nodes, in different graphs, cuts off at graph boundary
;;  resource: two nodes, one arc, upstream is a resource
;;  no functions: one node, a property which is a function, should not be serialized

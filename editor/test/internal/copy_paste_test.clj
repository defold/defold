(ns internal.copy-paste-test
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :as ts]
            [dynamo.types :as t])
  (:import [dynamo.graph Endpoint]))

;;  simple copy: two nodes, one arc
(g/defnode ConsumerNode
  (property a-property t/Str (default "foo"))
  (input consumes-value t/Str :array))

(g/defnode ProducerNode
  (output produces-value t/Str (g/fnk [] "A string")))

(g/defnode ConsumeAndProduceNode
  (inherits ConsumerNode)
  (inherits ProducerNode)

  (output produces-value t/Str (g/fnk [consumes-value] (str/join " " consumes-value))))

;; (comment
;;   {:roots #{id-of-the-most-important-copied-node}
;;    :nodes #{{:serial-id 'id1' :node-type NodeType}
;;             {:serial-id 'id2' :node-type NodeType
;;              :properties {property-name property-value}}
;;             :arcs  #{[#producer{ id2 :bar} #consumer{id1 :foo}]
;;                      [#producer{ id2 :bar} #consumer{id2 :bar} "proj/local/path.png"]
;;                      [#resource{ "/images/1.jpg" :image} #consumer{ id1 :images}] }}}

;;   )

(defn simple-copy-fragment [world]
  (let [[node1 node2] (ts/tx-nodes (g/make-node world ConsumerNode)
                                   (g/make-node world ProducerNode))
        node1-id      (g/node-id node1)
        node2-id      (g/node-id node2)]
    (g/transact
     (g/connect node2 :produces-value node1 :consumes-value))
    (g/copy [node1-id] (constantly false))))

(deftest simple-copy
  (ts/with-clean-system
    (let [fragment            (simple-copy-fragment world)
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
      (is (every? #(instance? Endpoint %) (map first (:arcs fragment))))
      (is (every? #(instance? Endpoint %) (map second (:arcs fragment))))
      (is (empty? (set/difference (set arc-node-references) (set serial-ids)))))))

(deftest simple-paste
  (ts/with-clean-system
    (let [fragment        (simple-copy-fragment world)
          paste-data      (g/paste world fragment)
          paste-tx-data   (:tx-data paste-data)
          paste-tx-result (g/transact paste-tx-data)
          new-nodes-added (g/tx-nodes-added paste-tx-result)
          [node1 node2]   (:nodes paste-data)]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 2 (count (:nodes paste-data))))
      (is (= [:create-node :create-node :connect]  (map :type paste-tx-data)))
      (is (= 2 (count new-nodes-added)))
      (is (= (g/node-id node1) (first (:root-node-ids paste-data))))
      (is (g/connected? (g/now) (g/node-id node2) :produces-value (g/node-id node1) :consumes-value)))))

(deftest poste-and-clone
  (ts/with-clean-system
    (let [fragment           (simple-copy-fragment world)
          paste-once         (g/paste world fragment)
          paste-once-result  (g/transact (:tx-data paste-once))
          [node1-once node2-once] (g/tx-nodes-added paste-once-result)
          paste-twice        (g/paste world fragment)
          paste-twice-result (g/transact (:tx-data paste-twice))
          [node1-twice node2-twice] (g/tx-nodes-added paste-twice-result)]
      (is (not= (g/node-id node1-once) (g/node-id node1-twice)))
      (is (not= (g/node-id node2-once) (g/node-id node2-twice)))
      (is (g/connected? (g/now) (g/node-id node2-once)  :produces-value (g/node-id node1-once)  :consumes-value))
      (is (g/connected? (g/now) (g/node-id node2-twice) :produces-value (g/node-id node1-twice) :consumes-value))
      (is (not (g/connected? (g/now) (g/node-id node2-once)  :produces-value (g/node-id node1-twice) :consumes-value)))
      (is (not (g/connected? (g/now) (g/node-id node2-twice) :produces-value (g/node-id node1-once)  :consumes-value))))))

(defn diamond-copy-fragment [world]
  (let [[node1 node2 node3 node4] (ts/tx-nodes (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world ProducerNode) )]
    (g/transact
     [(g/connect node2 :produces-value node1 :consumes-value)
      (g/connect node3 :produces-value node1 :consumes-value)
      (g/connect node4 :produces-value node3 :consumes-value)
      (g/connect node4 :produces-value node2 :consumes-value)])
    (g/copy [(g/node-id node1)] (constantly true))))

(deftest diamond-copy
  (ts/with-clean-system
    (let [fragment            (diamond-copy-fragment world)
          fragment-nodes      (:nodes fragment)
          serial-ids          (map :serial-id fragment-nodes)
          arc-node-references (into #{} (concat (map #(:node-id (first %))  (:arcs fragment))
                                                (map #(:node-id (second %)) (:arcs fragment))))]
      (is (= 4 (count (:arcs fragment))))
      (is (= 4 (count fragment-nodes)))
      (is (empty? (set/difference (set arc-node-references) (set serial-ids)))))))

(deftest diamond-paste
  (ts/with-clean-system
    (let [fragment        (diamond-copy-fragment world)
          paste-data      (g/paste world fragment)
          paste-tx-data   (:tx-data paste-data)
          paste-tx-result (g/transact paste-tx-data)
          new-nodes-added (g/tx-nodes-added paste-tx-result)
          [node1 node2 node3 node4]   (:nodes paste-data)]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 4 (count (:nodes paste-data))))
      (is (= [:create-node :create-node :create-node :create-node
              :connect     :connect     :connect     :connect]  (map :type paste-tx-data)))
      (is (= 4 (count new-nodes-added)))
      (is (= (g/node-id node1) (first (:root-node-ids paste-data))))
      (is (= (g/node-value node1 :produces-value) "A string A string")))))


(deftest short-circut
  (ts/with-clean-system
    (let [[node1 node2 node3] (ts/tx-nodes (g/make-node world ConsumerNode)
                                           (g/make-node world ConsumeAndProduceNode)
                                           (g/make-node world ConsumeAndProduceNode))
          node1-id            (g/node-id node1)]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)
        (g/connect node2 :produces-value node3 :consumes-value)])
      (let [fragment            (g/copy [node1-id] (constantly true))
            fragment-nodes      (:nodes fragment)]
        (is (= 3 (count (:arcs fragment))))
        (is (= 3 (count fragment-nodes)))))))


(deftest cross-graph-copy
  (ts/with-clean-system
    (let [g1                  (g/make-graph!)
          g2                  (g/make-graph!)
          [node1 node2 node3] (ts/tx-nodes (g/make-node g1 ConsumerNode)
                                           (g/make-node g1 ConsumeAndProduceNode)
                                           (g/make-node g2 ProducerNode))
          node1-id            (g/node-id node1)]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)])
      (let [fragment            (g/copy [node1-id] (constantly true))
            fragment-nodes      (:nodes fragment)]
        (is (= 1 (count (:arcs fragment))))
        (is (= 2 (count fragment-nodes)))))))

(g/defnode FunctionPropertyNode
  (property a-function t/Any))

(deftest no-functions
  (ts/with-clean-system
    (let [[node1]       (ts/tx-nodes (g/make-node world FunctionPropertyNode :a-function (fn [] false)))
          fragment      (g/copy [(g/node-id node1)] (constantly false))
          fragment-node (first (:nodes fragment))]
      (is (not (contains? (:properties fragment-node) :a-function))))))

;;  resource: two nodes, one arc, upstream is a resource
(g/defnode StopperNode
  "Not serializable"
  (input  discards-value t/Str)
  (output produces-value t/Str (g/fnk [] "here")))

(defn- stop-at-stoppers [node]
  (not (= "StopperNode" (:name (g/node-type node)))))

(defrecord StopArc [id label])

(defn- serialize-stopper [node label]
  (StopArc. (g/node-id node) label))

(deftest serialization-uses-predicates
  (ts/with-clean-system
    (let [[node1 node2 node3 node4] (ts/tx-nodes (g/make-node world ConsumerNode)
                                                 (g/make-node world ConsumeAndProduceNode)
                                                 (g/make-node world StopperNode)
                                                 (g/make-node world ProducerNode))
          node1-id                  (g/node-id node1)]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)
        (g/connect node4 :produces-value node3 :discards-value)])
      ;;; - where to stop?
      ;;; how to replace any node in arc with some other representation (serialize)?
      ;;; how to re-serialize a particular reprensation in a node arc that is custom?

      (let [fragment       (g/copy [node1-id] {:continue? stop-at-stoppers
                                               :write-handlers {StopperNode serialize-stopper}})
            fragment-nodes (:nodes fragment)]
        (is (= 2 (count (:arcs fragment))))
        (is (= 2 (count fragment-nodes)))
        (is (not (contains? (into #{} (map (comp :name :node-type) fragment-nodes)) "StopperNode")))
        (is (= #{StopArc Endpoint} (into #{} (map (comp class first) (:arcs fragment)))))
        (is (= #{Endpoint} (into #{} (map (comp class second) (:arcs fragment)))))))))

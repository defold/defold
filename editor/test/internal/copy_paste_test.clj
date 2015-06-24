(ns internal.copy-paste-test
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts])
  (:import [dynamo.graph Endpoint]))

;;  simple copy: two nodes, one arc
(g/defnode ConsumerNode
  (property a-property g/Str (default "foo"))
  (input consumes-value g/Str :array))

(g/defnode ProducerNode
  (output produces-value g/Str (g/fnk [] "A string")))

(g/defnode ConsumeAndProduceNode
  (inherits ConsumerNode)
  (inherits ProducerNode)

  (output produces-value g/Str (g/fnk [consumes-value] (str/join " " consumes-value))))

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
          paste-data      (g/paste world fragment {})
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

(deftest paste-and-clone
  (ts/with-clean-system
    (let [fragment           (simple-copy-fragment world)
          paste-once         (g/paste world fragment {})
          paste-once-result  (g/transact (:tx-data paste-once))
          [node1-once node2-once] (g/tx-nodes-added paste-once-result)
          paste-twice        (g/paste world fragment {})
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
          paste-data      (g/paste world fragment {})
          paste-tx-data   (:tx-data paste-data)
          paste-tx-result (g/transact paste-tx-data)
          new-nodes-added (g/tx-nodes-added paste-tx-result)
          new-root        (g/node-by-id-at (g/now) (first (:root-node-ids paste-data)))]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 4 (count (:nodes paste-data))))
      (is (= [:create-node :create-node :create-node :create-node
              :connect     :connect     :connect     :connect]  (map :type paste-tx-data)))
      (is (= 4 (count new-nodes-added)))
      (is (= (g/node-value new-root :produces-value) "A string A string")))))


(deftest short-circuit
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
  (property a-function g/Any))

(deftest no-functions
  (ts/with-clean-system
    (let [[node1]       (ts/tx-nodes (g/make-node world FunctionPropertyNode :a-function (fn [] false)))
          fragment      (g/copy [(g/node-id node1)] (constantly false))
          fragment-node (first (:nodes fragment))]
      (is (not (contains? (:properties fragment-node) :a-function))))))

;;  resource: two nodes, one arc, upstream is a resource
(g/defnode StopperNode
  "Not serializable"
  (property a-property g/Str)
  (input  discards-value g/Str)
  (output produces-value g/Str (g/fnk [a-property] a-property)))

(defn- stop-at-stoppers [node]
  (not (= "StopperNode" (:name (g/node-type node)))))

(defrecord StopArc [id label])

(defn- serialize-stopper [node label]
  (StopArc. (g/node-id node) label))

(defn serialization-uses-predicates-copy-fragment [world]
  (let [[node1 node2 node3 node4] (ts/tx-nodes (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world StopperNode :a-property "the one and only")
                                               (g/make-node world ProducerNode))]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)
        (g/connect node4 :produces-value node3 :discards-value)])
      [node3 (g/copy [(g/node-id node1)] {:continue? stop-at-stoppers
                                     :write-handlers {StopperNode serialize-stopper}})]))

(deftest serialization-uses-predicates
  (ts/with-clean-system
    (let [[_ fragment]   (serialization-uses-predicates-copy-fragment world)
          fragment-nodes (:nodes fragment)]
      (is (= 2 (count (:arcs fragment))))
      (is (= 2 (count fragment-nodes)))
      (is (not (contains? (into #{} (map (comp :name :node-type) fragment-nodes)) "StopperNode")))
      (is (= #{StopArc Endpoint} (into #{} (map (comp class first) (:arcs fragment)))))
      (is (= #{Endpoint} (into #{} (map (comp class second) (:arcs fragment))))))))

(defn resolve-by-id [record]
  [(:id record) (:label record)])

(deftest deserialization-with-resolver
  (ts/with-clean-system
    (let [[original-stopper fragment] (serialization-uses-predicates-copy-fragment world)
          paste-data                  (g/paste world fragment {:read-handlers {StopArc resolve-by-id}})
          paste-tx-data               (:tx-data paste-data)
          paste-tx-result             (g/transact paste-tx-data)
          new-nodes-added             (g/tx-nodes-added paste-tx-result)
          new-root                    (g/node-by-id-at (g/now) (first (:root-node-ids paste-data)))
          new-leaf                    (first (remove #(= (g/node-id %) (g/node-id new-root)) (:nodes paste-data)))]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 2 (count (:nodes paste-data)) (count new-nodes-added)))
      (is (= [:create-node :create-node :connect :connect]  (map :type paste-tx-data)))
      (is (g/connected? (g/now) (g/node-id original-stopper) :produces-value (g/node-id new-leaf) :consumes-value))
      (is (= "the one and only" (g/node-value new-root :produces-value))))))

(ns internal.copy-paste-test
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [support.test-support :as ts]))

(defn arc-node-references
  [fragment]
  (into #{} (concat (map first (:arcs fragment))
                    (map #(nth % 2) (:arcs fragment)))))


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
                                   (g/make-node world ProducerNode))]
    (g/transact
     (g/connect node2 :produces-value node1 :consumes-value))
    (g/copy [node1] {:traverse? (constantly true)})))

(deftest simple-copy
  (ts/with-clean-system
    (let [fragment            (simple-copy-fragment world)
          fragment-nodes      (:nodes fragment)
          serial-ids          (map :serial-id fragment-nodes)]
      (is (= 1 (count (:roots fragment))))
      (is (every? integer? (:roots fragment)))
      (is (every? integer? serial-ids))
      (is (= (count serial-ids) (count (distinct serial-ids))))
      (is (= #{ProducerNode ConsumerNode} (into #{}  (map :node-type fragment-nodes))))
      (is (= "foo" (:a-property (:properties (first (filter #(= ConsumerNode (:node-type %)) fragment-nodes))))))
      (is (= 1 (count (:arcs fragment))))
      (is (every? #(= 4 (count %)) (:arcs fragment)))
      (is (empty? (set/difference (arc-node-references fragment) (set serial-ids)))))))

(defn- pasted-node [type paste-data]
  (first (filter #(g/node-instance? type %) (:nodes paste-data))))

(deftest simple-paste
  (ts/with-clean-system
    (let [fragment        (simple-copy-fragment world)
          paste-data      (g/paste world fragment {})
          paste-tx-data   (:tx-data paste-data)
          paste-tx-result (g/transact paste-tx-data)
          new-nodes-added (g/tx-nodes-added paste-tx-result)
          producer        (pasted-node ProducerNode paste-data)
          consumer        (pasted-node ConsumerNode paste-data)]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 2 (count (:nodes paste-data))))
      (is (= [:create-node :create-node :connect]  (map :type paste-tx-data)))
      (is (= 2 (count new-nodes-added)))
      (is (every? #(contains? (into #{} (:nodes paste-data)) %) (:root-node-ids paste-data)))
      (is (g/connected? (g/now) producer :produces-value consumer :consumes-value)))))

(deftest paste-and-clone
  (ts/with-clean-system
    (let [fragment           (simple-copy-fragment world)
          paste-once         (g/paste world fragment {})
          paste-once-result  (g/transact (:tx-data paste-once))
          producer-once      (pasted-node ProducerNode paste-once)
          consumer-once      (pasted-node ConsumerNode paste-once)
          paste-twice        (g/paste world fragment {})
          paste-twice-result (g/transact (:tx-data paste-twice))
          producer-twice     (pasted-node ProducerNode paste-twice)
          consumer-twice     (pasted-node ConsumerNode paste-twice)]
      (is (not= producer-once producer-twice))
      (is (not= consumer-once consumer-twice))
      (is (g/connected? (g/now) producer-once  :produces-value consumer-once  :consumes-value))
      (is (g/connected? (g/now) producer-twice :produces-value consumer-twice :consumes-value))
      (is (not (g/connected? (g/now) producer-once  :produces-value consumer-twice :consumes-value)))
      (is (not (g/connected? (g/now) producer-twice :produces-value consumer-once  :consumes-value))))))

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
    (g/copy [node1] {:traverse? (constantly true)})))

(deftest diamond-copy
  (ts/with-clean-system
    (let [fragment            (diamond-copy-fragment world)
          fragment-nodes      (:nodes fragment)
          serial-ids          (map :serial-id fragment-nodes)
          arc-node-references (into #{} (concat (map first (:arcs fragment))
                                                (map #(nth % 2) (:arcs fragment))))]
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
                                           (g/make-node world ConsumeAndProduceNode))]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)
        (g/connect node2 :produces-value node3 :consumes-value)])
      (let [fragment            (g/copy [node1] {:traverse? (constantly true)})
            fragment-nodes      (:nodes fragment)
            paste-data          (g/paste world fragment {})
            paste-tx-data       (:tx-data paste-data)
            paste-tx-result     (g/transact paste-tx-data)
            new-nodes-added     (map #(g/node-by-id-at (g/now) %) (g/tx-nodes-added paste-tx-result))
            new-root            (g/node-by-id-at (g/now) (first (:root-node-ids paste-data)))
            [newleaf1 newleaf2] (remove #(= % new-root) new-nodes-added)]

        (testing "copy short-circuts cycles"
          (is (= 3 (count (:arcs fragment))))
                 (is (= 3 (count fragment-nodes)))
                 (is (= 3 (count new-nodes-added))))

        (testing "paste preserves cycle noodles and connections"
          (is (= #{ConsumeAndProduceNode} (set (map g/node-type [newleaf1 newleaf2]))))
          (is (g/connected? (g/now) (g/node-id newleaf1) :produces-value (g/node-id newleaf2) :consumes-value))
          (is (g/connected? (g/now) (g/node-id newleaf2) :produces-value (g/node-id newleaf1) :consumes-value)))))))

(deftest cross-graph-copy
  (ts/with-clean-system
    (let [g1                  (g/make-graph!)
          g2                  (g/make-graph!)
          [node1 node2 node3] (ts/tx-nodes (g/make-node g1 ConsumerNode)
                                           (g/make-node g1 ConsumeAndProduceNode)
                                           (g/make-node g2 ProducerNode))]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)])
      (let [fragment            (g/copy [node1] {:traverse? (constantly true)})
            fragment-nodes      (:nodes fragment)]
        (is (= 1 (count (:arcs fragment))))
        (is (= 2 (count fragment-nodes)))))))

(g/defnode FunctionPropertyNode
  (property a-function g/Any))

(deftest no-functions
  (ts/with-clean-system
    (let [[node1]       (ts/tx-nodes (g/make-node world FunctionPropertyNode :a-function (fn [] false)))
          fragment      (g/copy [node1] {:traverse? (constantly false)})
          fragment-node (first (:nodes fragment))]
      (is (not (contains? (:properties fragment-node) :a-function))))))

;;  resource: two nodes, one arc, upstream is a resource
(g/defnode StopperNode
  "Not serializable"
  (property a-property g/Str)
  (input  discards-value g/Str)
  (output produces-value g/Str (g/fnk [a-property] a-property)))

(defn- stop-at-stoppers [basis [src src-label tgt tgt-label]]
  (not (g/node-instance? StopperNode tgt)))

(defrecord Standin [original-id])

(defn- serialize-stopper [node]
  (Standin. (g/node-id node)))

(defn serialization-uses-predicates-copy-fragment [world]
  (let [[node1 node2 node3 node4] (ts/tx-nodes (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world ConsumeAndProduceNode)
                                               (g/make-node world StopperNode :a-property "the one and only")
                                               (g/make-node world ProducerNode))]
      (g/transact
       [(g/connect node2 :produces-value node1 :consumes-value)
        (g/connect node3 :produces-value node2 :consumes-value)
        (g/connect node4 :produces-value node3 :discards-value)])
      [node3 (g/copy [node1] {:traverse? (comp stop-at-stoppers)
                              :serializer (fn [basis node]
                                            (if (g/node-instance? StopperNode (g/node-id node))
                                              (serialize-stopper node)
                                              (g/default-node-serializer basis node)))})]))

(deftest serialization-uses-predicates
  (ts/with-clean-system
    (let [[_ fragment]   (serialization-uses-predicates-copy-fragment world)
          fragment-nodes (:nodes fragment)]
      (is (= 2 (count (:arcs fragment))))
      (is (= 3 (count fragment-nodes)))
      (is (not (contains? (into #{} (map (comp :name :node-type) fragment-nodes)) "internal.copy-paste-test/StopperNode"))))))

(defn resolve-by-id
  [basis graph-id record]
  (if (instance? Standin record)
    (g/node-by-id-at basis (:original-id record))
    (g/default-node-deserializer basis graph-id record)))

(deftest deserialization-with-resolver
  (ts/with-clean-system
    (let [[original-stopper fragment] (serialization-uses-predicates-copy-fragment world)
          paste-data                  (g/paste world fragment {:deserializer resolve-by-id})
          paste-tx-data               (:tx-data paste-data)
          paste-tx-result             (g/transact paste-tx-data)
          new-nodes-added             (g/tx-nodes-added paste-tx-result)
          new-root                    (g/node-by-id-at (g/now) (first (:root-node-ids paste-data)))
          new-leaf                    (first (remove #(= % (g/node-id new-root)) (:nodes paste-data)))]
      (is (= 1 (count (:root-node-ids paste-data))))
      (is (= 2 (count new-nodes-added)))
      (is (= 3 (count (:nodes paste-data))))
      (is (= [:create-node :create-node :connect :connect]  (map :type paste-tx-data)))
      (is (g/connected? (g/now) original-stopper :produces-value new-leaf :consumes-value))
      (is (= "the one and only" (g/node-value new-root :produces-value))))))

(defrecord StructuredValue [a b c])

(def extra-writers (transit/record-write-handlers StructuredValue))

(def extra-readers (transit/record-read-handlers StructuredValue))

(g/defnode RichNode
  (property deep-value StructuredValue
            (default (g/constantly (->StructuredValue 1 2 3)))))

(defn rich-value-fragment [world]
  (let [[node] (ts/tx-nodes (g/make-node world RichNode))]
    (g/copy [node] {:traverse? (constantly true)})))

(deftest roundtrip-serialization-deserialization
  (ts/with-clean-system
    (let [simple-fragment (simple-copy-fragment world)
          rich-fragment   (rich-value-fragment world)]
      (are [x] (= x (g/read-graph (g/write-graph x extra-writers) extra-readers))
        1
        [1]
        {:x 1}
        #{1 2}
        'food
        ConsumerNode
        simple-fragment
        rich-fragment))))

(g/defnode SetterNode
  (property producer g/NodeID
    (value (g/fnk [_node-id]
             (g/node-feeding-into _node-id :value)))
    (set (fn [basis self old-value new-value]
           (g/connect new-value :produces-value self :value))))
  (input value g/Str))

(deftest setter-called
  (ts/with-clean-system
    (let [[producer setter] (ts/tx-nodes (g/make-nodes world [producer ProducerNode
                                                              setter [SetterNode :producer producer]]))
          [new-setter] (ts/tx-nodes (:tx-data (g/paste world (g/copy [setter] {}) {})))]
      (is (= producer (g/node-value new-setter :producer))))))

;; -----------------------------------------------------------------------------
;; Override tests
;; -----------------------------------------------------------------------------

(g/defnode GameObjectNode
  (property test-property g/Str)
  (input child-outlines g/Any :array)
  (input component-id-pairs g/Any :array)
  (output base-url g/Str (g/fnk [] "/game-object"))
  (output id-counts g/Any (g/fnk [component-id-pairs] (frequencies (map first component-id-pairs)))))

(g/defnode EmbeddedComponent
  (property id g/Str)
  (input base-url g/Str)
  (input id-counts g/Any)
  (output component-id g/Any (g/fnk [_node-id id] [id _node-id]))
  (output node-outline g/Any (g/fnk [] {})))

(defn- copy-traverse [_basis [_src-node _src-label _tgt-node tgt-label]]
  ;; This is a simplification of the traversal rules from outline/default-copy-traverse.
  (= :child-outlines tgt-label))

(deftest copy-flattens-override-hierarchy
  (ts/with-clean-system

    ;; Add some random nodes to ensure node ids differ from serial ids.
    (g/transact (for [_ (range 100)] (g/make-node world ProducerNode)))

    ;; Create original nodes and override node.
    (let [[game-object component] (ts/tx-nodes (g/make-nodes world [game-object [GameObjectNode :test-property "original-value"]
                                                                    component [EmbeddedComponent :id "component"]]
                                                             (g/connect component :node-outline game-object :child-outlines)
                                                             (g/connect component :component-id game-object :component-id-pairs)
                                                             (g/connect game-object :base-url component :base-url)
                                                             (g/connect game-object :id-counts component :id-counts)))
          [or-game-object] (g/tx-nodes-added (g/transact (:tx-data (g/override game-object))))]

      ;; Override a property.
      (g/set-property! or-game-object :test-property "override-value")

      ;; Copy the override node.
      (let [{:keys [arcs nodes node-id->serial-id]} (g/copy [or-game-object] {:traverse? copy-traverse})
            copied-properties (fn [source-node-id]
                                (when-some [serial-id (node-id->serial-id source-node-id)]
                                  (some #(when (= serial-id (:serial-id %)) (:properties %)) nodes)))
            arcs-set (set arcs)
            copied-connection? (fn [source-node-id source-label target-node-id target-label]
                                 (contains? arcs-set [(node-id->serial-id source-node-id) source-label (node-id->serial-id target-node-id) target-label]))]

        (testing "Nodes are serialized"
          (is (some? (node-id->serial-id or-game-object)))
          (is (some? (node-id->serial-id component))))

        (testing "Override node-id maps to the same serial-id as the original"
          (is (= (node-id->serial-id game-object)
                 (node-id->serial-id or-game-object))))

        (testing "Properties are inherited and overridden"
          (is (= {:test-property "override-value"}
                 (copied-properties or-game-object)))
          (is (= {:id "component"}
                 (copied-properties component))))

        (testing "Connections are serialized"
          (is (copied-connection? component :node-outline or-game-object :child-outlines))
          (is (copied-connection? component :component-id or-game-object :component-id-pairs))
          (is (copied-connection? or-game-object :base-url component :base-url))
          (is (copied-connection? or-game-object :id-counts component :id-counts)))))))

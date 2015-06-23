(ns editor.injection-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer :all]
            [editor.core :as core]))

(g/defnode Receiver
  (input surname String :inject)
  (input samples g/Num :inject :array)
  (input label g/Any :inject))

(g/defnk produce-string :- String
  []
  "nachname")

(g/defnode Sender1
  (output surname String produce-string))

(g/defnk produce-sample :- Integer
  []
  42)

(g/defnode Sampler
  (output sample Integer produce-sample))

(g/defnk produce-label :- g/Keyword
  []
  :a-keyword)

(g/defnode Labeler
  (output label g/Keyword produce-label))

(deftest compatible-inputs-and-outputs
  (let [recv    (g/construct Receiver)
        sender  (g/construct Sender1)
        sampler (g/construct Sampler)
        labeler (g/construct Labeler)]
    (is (= #{[sender  :surname recv :surname]} (core/injection-candidates [recv] [sender])))
    (is (= #{[sampler :sample  recv :samples]} (core/injection-candidates [recv] [sampler])))
    (is (= #{[labeler :label   recv :label]}   (core/injection-candidates [recv] [labeler])))))

(g/defnode ValueConsumer
  (input local-names g/Str :inject :array)
  (output concatenation g/Str (g/fnk [local-names] (str/join local-names))))

(g/defnode InjectionScope
  (inherits core/Scope)
  (input local-name g/Str :inject)
  (output passthrough g/Str (g/fnk [local-name] local-name)))

(g/defnode ValueProducer
  (property value g/Str)
  (output local-name g/Str (g/fnk [value] value)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-system
      (let [[scope _] (g/tx-nodes-added
                       (g/transact
                        (g/make-nodes
                         world
                         [scope    InjectionScope
                          producer [ValueProducer :value "a known value"]]
                         (g/connect producer :self scope :nodes))))]
        (is (= "a known value" (g/node-value scope :passthrough))))))

  (testing "attach one node output to input on another node"
    (with-clean-system
      (let [[scope producer consumer]
            (g/tx-nodes-added
             (g/transact
              (g/make-nodes
               world
               [scope core/Scope
                producer [ValueProducer :value "a known value"]
                consumer ValueConsumer]
               (g/connect producer :self scope :nodes)
               (g/connect consumer :self scope :nodes))))]
        (is (= "a known value" (g/node-value consumer :concatenation))))))

  (testing "attach nodes in different transactions"
    (with-clean-system
      (let [[scope]    (tx-nodes (g/make-node world core/Scope))
            [consumer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [consumer ValueConsumer]
                          (g/connect consumer :self scope :nodes))))
            [producer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [producer [ValueProducer :value "a known value"]]
                          (g/connect producer :self scope :nodes))))]
        (is (= "a known value" (g/node-value consumer :concatenation))))))

  (testing "attach nodes in different transactions and reverse order"
    (with-clean-system
      (let [[scope]    (tx-nodes (g/make-node world core/Scope))
            [producer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [producer [ValueProducer :value "a known value"]]
                          (g/connect producer :self scope :nodes))))
            [consumer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [consumer ValueConsumer]
                          (g/connect consumer :self scope :nodes))))]
        (is (= "a known value" (g/node-value consumer :concatenation))))))

  (testing "explicitly connect nodes, see if injection also happens"
    (with-clean-system
      (let [[scope]    (tx-nodes (g/make-node world core/Scope))
            [producer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [producer [ValueProducer :value "a known value"]]
                          (g/connect producer :self scope :nodes))))
            [consumer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [consumer ValueConsumer]
                          (g/connect consumer :self scope :nodes)
                          (g/connect producer :local-name consumer :local-names))))]
        (is (= "a known value" (g/node-value consumer :concatenation)))))))

(g/defnode ReflexiveFeedback
  (property port g/Keyword (default :no))
  (input ports g/Keyword :array :inject))

(deftest reflexive-injection
  (testing "don't connect a node's own output to its input"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world ReflexiveFeedback))]
        (is (not (g/connected? (g/now) (g/node-id node) :port (g/node-id node) :ports)))))))

(g/defnode OutputProvider
  (inherits core/Scope)
  (property context g/Int (default 0)))

(g/defnode InputConsumer
  (input context g/Int :inject))

(defn- create-simulated-project [world]
  (first (tx-nodes (g/make-node world core/Scope :tag :project))))

(deftest adding-nodes-in-nested-scopes
  (testing "one consumer in a nested scope"
    (with-clean-system
      (let [project    (create-simulated-project world)
            [provider] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [provider [OutputProvider :context 119]
                           consumer InputConsumer]
                          (g/connect consumer :self provider :nodes))))]
        (is (= 1 (count (g/targets (g/now) (g/node-id provider) :context)))))))

  (testing "two consumers each in their own nested scope"
    (with-clean-system
      (let [project     (create-simulated-project world)
            [provider1] (g/tx-nodes-added
                         (g/transact
                          (g/make-nodes
                           world
                           [provider [OutputProvider :context 119]
                            consumer InputConsumer]
                           (g/connect consumer :self provider :nodes))))
            [provider2] (g/tx-nodes-added
                         (g/transact
                          (g/make-nodes
                           world
                           [provider [OutputProvider :context 113]
                            consumer InputConsumer]
                           (g/connect consumer :self provider :nodes))))]
        (is (= 1 (count (g/targets (g/now) (g/node-id provider1) :context))))
        (is (= 1 (count (g/targets (g/now) (g/node-id provider2) :context))))))))

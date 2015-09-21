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

(g/defnode Sender1
  (output surname String (g/fnk [] "nachname")))

(g/defnode Sampler
  (output sample Integer (g/fnk [] 42)))

(g/defnode Labeler
  (output label g/Keyword (g/fnk [] :a-keyword)))

(deftest compatible-inputs-and-outputs
  (with-clean-system
    (let [[recv sender sampler labeler] (tx-nodes (g/make-node world Receiver)
                                                  (g/make-node world Sender1)
                                                  (g/make-node world Sampler)
                                                  (g/make-node world Labeler))]
      (is (= #{[sender  :surname recv :surname]} (core/injection-candidates (g/now) [sender]  [recv])))
      (is (= #{[sampler :sample  recv :samples]} (core/injection-candidates (g/now) [sampler] [recv])))
      (is (= #{[labeler :label   recv :label]}   (core/injection-candidates (g/now) [labeler] [recv]))))))

(g/defnode ValueConsumer
  (input local-names g/Str :inject :array)
  (output concatenation g/Str (g/fnk [local-names] (str/join local-names))))

(g/defnode InjectionScope
  (input nodes g/Any :cascade-delete)
  (input local-name g/Str :inject)
  (output passthrough g/Str (g/fnk [local-name] local-name)))

(g/defnode ValueProducer
  (property value g/Str)
  (output local-name g/Str (g/fnk [value] value)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-system
      (let [[scope producer] (g/tx-nodes-added
                              (g/transact
                               (g/make-nodes
                                world
                                [scope    InjectionScope
                                 producer [ValueProducer :value "a known value"]])))]
        (g/transact (core/inject-dependencies [producer] [scope]))
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
                consumer ValueConsumer])))]
        (g/transact (core/inject-dependencies [scope producer] [consumer]))
        (is (= "a known value" (g/node-value consumer :concatenation))))))

  (testing "attach nodes in different transactions"
    (with-clean-system
      (let [[scope]    (tx-nodes (g/make-node world core/Scope))
            [consumer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [consumer ValueConsumer])))
            [producer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [producer [ValueProducer :value "a known value"]])))]
        (g/transact (core/inject-dependencies [scope producer] [consumer]))
        (is (= "a known value" (g/node-value consumer :concatenation))))))

  (testing "explicitly connect nodes, see if injection also happens"
    (with-clean-system
      (let [[scope]    (tx-nodes (g/make-node world core/Scope))
            [producer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [producer [ValueProducer :value "a known value"]]
                          (g/connect producer :_node-id scope :nodes))))
            [consumer] (g/tx-nodes-added
                        (g/transact
                         (g/make-nodes
                          world
                          [consumer ValueConsumer]
                          (g/connect consumer :_node-id scope :nodes)
                          (g/connect producer :local-name consumer :local-names))))]
        (g/transact (core/inject-dependencies [scope producer] [consumer]))
        (is (= "a known value" (g/node-value consumer :concatenation)))))))

(g/defnode ReflexiveFeedback
  (property port g/Keyword (default :no))
  (input ports g/Keyword :array :inject))

(deftest reflexive-injection
  (testing "don't connect a node's own output to its input"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world ReflexiveFeedback))]
        (g/transact (core/inject-dependencies [node] [node]))
        (is (not (g/connected? (g/now) node :port node :ports)))))))

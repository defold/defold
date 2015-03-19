(ns internal.injection-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [add in]]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [editor.core :as core]
            [internal.graph :as ig]
            [internal.node :as in]))

;; TODO - move this to editor. It's no longer an inherent part of the graph.

(g/defnode Receiver
  (input surname String :inject)
  (input samples [t/Num] :inject)
  (input label t/Any :inject))

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

(g/defnk produce-label :- t/Keyword
  []
  :a-keyword)

(g/defnode Labeler
  (output label t/Keyword produce-label))

(deftest compatible-inputs-and-outputs
  (let [recv    (n/construct Receiver)
        sender  (n/construct Sender1)
        sampler (n/construct Sampler)
        labeler (n/construct Labeler)]
    (is (= #{[sender  :surname recv :surname]} (n/injection-candidates [recv] [sender])))
    (is (= #{[sampler :sample  recv :samples]} (n/injection-candidates [recv] [sampler])))
    (is (= #{[labeler :label   recv :label]}   (n/injection-candidates [recv] [labeler])))))

(g/defnode ValueConsumer
  (input local-names [t/Str] :inject)
  (output concatenation t/Str (g/fnk [local-names] (str/join local-names))))

(g/defnode InjectionScope
  (inherits core/Scope)
  (input local-name t/Str :inject)
  (output passthrough t/Str (g/fnk [local-name] local-name)))

(g/defnode ValueProducer
  (property value t/Str)
  (output local-name t/Str (g/fnk [value] value)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-system
      (let [scope (g/transactional
                    (ds/in (g/add (n/construct InjectionScope))
                           (g/add (n/construct ValueProducer :value "a known value"))
                      (ds/current-scope)))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache scope :passthrough))))))

  (testing "attach one node output to input on another node"
    (with-clean-system
      (let [consumer (g/transactional
                       (ds/in (g/add (n/construct core/Scope))
                              (g/add (n/construct ValueProducer :value "a known value"))
                              (g/add (n/construct ValueConsumer))))]
        (def con* consumer)
        (def gr* (:graph @world-ref))
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "attach nodes in different transactions"
    (with-clean-system
      (let [scope (g/transactional
                    (g/add (n/construct core/Scope)))
            consumer (g/transactional
                       (ds/in scope
                         (g/add (n/construct ValueConsumer))))
            producer (g/transactional
                       (ds/in scope
                              (g/add (n/construct ValueProducer :value "a known value"))))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "attach nodes in different transactions and reverse order"
    (with-clean-system
      (let [scope (g/transactional
                    (g/add (n/construct core/Scope)))
            producer (g/transactional
                       (ds/in scope
                              (g/add (n/construct ValueProducer :value "a known value"))))
            consumer (g/transactional
                       (ds/in scope
                         (g/add (n/construct ValueConsumer))))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "explicitly connect nodes, see if injection also happens"
    (with-clean-system
      (let [scope (g/transactional
                    (g/add (n/construct core/Scope)))
            producer (g/transactional
                       (ds/in scope
                              (g/add (n/construct ValueProducer :value "a known value"))))
            consumer (g/transactional
                       (ds/in scope
                         (let [c (g/add (n/construct ValueConsumer))]
                           (ds/connect producer :local-name c :local-names)
                           c)))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation)))))))

(g/defnode ReflexiveFeedback
  (property port t/Keyword (default :no))
  (input ports [t/Keyword] :inject))

(deftest reflexive-injection
  (testing "don't connect a node's own output to its input"
    (with-clean-system
      (let [node (g/transactional (g/add (n/construct ReflexiveFeedback)))]
        (is (not (ig/connected? (-> world-ref deref :graph) (:_id node) :port (:_id node) :ports)))))))

(g/defnode OutputProvider
  (inherits core/Scope)
  (property context t/Int (default 0)))

(g/defnode InputConsumer
  (input context t/Int :inject))

(defn- create-simulated-project []
  (g/transactional (g/add (n/construct core/Scope :tag :project))))

(deftest adding-nodes-in-nested-scopes
  (testing "one consumer in a nested scope"
    (with-clean-system
     (let [project  (create-simulated-project)
           provider (g/transactional
                      (ds/in (g/add (n/construct OutputProvider :context 119))
                        (g/add (n/construct InputConsumer))
                        (ds/current-scope)))]
       (is (= 1 (count (ds/nodes-consuming provider :context)))))))

  (testing "two consumers each in their own nested scope"
    (with-clean-system
     (let [project   (create-simulated-project)
           provider1 (g/transactional
                       (ds/in (g/add (n/construct OutputProvider :context 119))
                         (g/add (n/construct InputConsumer))
                         (ds/current-scope)))
           provider2 (g/transactional
                       (ds/in (g/add (n/construct OutputProvider :context 113))
                         (g/add (n/construct InputConsumer))
                         (ds/current-scope)))]
       (is (= 1 (count (ds/nodes-consuming provider1 :context))))
       (is (= 1 (count (ds/nodes-consuming provider2 :context))))))))

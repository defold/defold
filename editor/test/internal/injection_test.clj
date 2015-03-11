(ns internal.injection-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [add in]]
            [dynamo.system.test-support :refer :all]
            [internal.graph.lgraph :as lg]
            [internal.node :as in]
            [plumbing.core :refer [defnk fnk]]
            [schema.core :as s]
            [schema.macros :as sm]))

(n/defnode Receiver
  (input surname String :inject)
  (input samples [s/Num] :inject)
  (input label s/Any :inject))

(defnk produce-string :- String
  []
  "nachname")

(n/defnode Sender1
  (output surname String produce-string))

(defnk produce-sample :- Integer
  []
  42)

(n/defnode Sampler
  (output sample Integer produce-sample))

(defnk produce-label :- s/Keyword
  []
  :a-keyword)

(n/defnode Labeler
  (output label s/Keyword produce-label))

(deftest compatible-inputs-and-outputs
  (let [recv    (n/construct Receiver)
        sender  (n/construct Sender1)
        sampler (n/construct Sampler)
        labeler (n/construct Labeler)]
    (is (= #{[sender  :surname recv :surname]} (in/injection-candidates [recv] [sender])))
    (is (= #{[sampler :sample  recv :samples]} (in/injection-candidates [recv] [sampler])))
    (is (= #{[labeler :label   recv :label]}   (in/injection-candidates [recv] [labeler])))))

(n/defnode ValueConsumer
  (input local-names [s/Str] :inject)
  (output concatenation s/Str (fnk [local-names] (str/join local-names))))

(n/defnode InjectionScope
  (inherits n/Scope)
  (input local-name s/Str :inject)
  (output passthrough s/Str (fnk [local-name] local-name)))

(n/defnode ValueProducer
  (property value s/Str)
  (output local-name s/Str (fnk [value] value)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-system
      (let [scope (g/transactional
                    (ds/in (ds/add (n/construct InjectionScope))
                           (ds/add (n/construct ValueProducer :value "a known value"))
                      (ds/current-scope)))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache scope :passthrough))))))

  (testing "attach one node output to input on another node"
    (with-clean-system
      (let [consumer (g/transactional
                       (ds/in (ds/add (n/construct n/Scope))
                              (ds/add (n/construct ValueProducer :value "a known value"))
                              (ds/add (n/construct ValueConsumer))))]
        (def con* consumer)
        (def gr* (:graph @world-ref))
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "attach nodes in different transactions"
    (with-clean-system
      (let [scope (g/transactional
                    (ds/add (n/construct n/Scope)))
            consumer (g/transactional
                       (ds/in scope
                         (ds/add (n/construct ValueConsumer))))
            producer (g/transactional
                       (ds/in scope
                              (ds/add (n/construct ValueProducer :value "a known value"))))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "attach nodes in different transactions and reverse order"
    (with-clean-system
      (let [scope (g/transactional
                    (ds/add (n/construct n/Scope)))
            producer (g/transactional
                       (ds/in scope
                              (ds/add (n/construct ValueProducer :value "a known value"))))
            consumer (g/transactional
                       (ds/in scope
                         (ds/add (n/construct ValueConsumer))))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation))))))

  (testing "explicitly connect nodes, see if injection also happens"
    (with-clean-system
      (let [scope (g/transactional
                    (ds/add (n/construct n/Scope)))
            producer (g/transactional
                       (ds/in scope
                              (ds/add (n/construct ValueProducer :value "a known value"))))
            consumer (g/transactional
                       (ds/in scope
                         (let [c (ds/add (n/construct ValueConsumer))]
                           (ds/connect producer :local-name c :local-names)
                           c)))]
        (is (= "a known value" (g/node-value (:graph @world-ref) cache consumer :concatenation)))))))

(n/defnode ReflexiveFeedback
  (property port s/Keyword (default :no))
  (input ports [s/Keyword] :inject))

(deftest reflexive-injection
  (testing "don't connect a node's own output to its input"
    (with-clean-system
      (let [node (g/transactional (ds/add (n/construct ReflexiveFeedback)))]
        (is (not (lg/connected? (-> world-ref deref :graph) (:_id node) :port (:_id node) :ports)))))))

(n/defnode OutputProvider
  (inherits n/Scope)
  (property context s/Int (default 0)))

(n/defnode InputConsumer
  (input context s/Int :inject))

(defn- create-simulated-project []
  (g/transactional (ds/add (n/construct n/Scope :tag :project))))

(deftest adding-nodes-in-nested-scopes
  (testing "one consumer in a nested scope"
    (with-clean-system
     (let [project  (create-simulated-project)
           provider (g/transactional
                      (ds/in (ds/add (n/construct OutputProvider :context 119))
                        (ds/add (n/construct InputConsumer))
                        (ds/current-scope)))]
       (is (= 1 (count (ds/nodes-consuming provider :context)))))))

  (testing "two consumers each in their own nested scope"
    (with-clean-system
     (let [project   (create-simulated-project)
           provider1 (g/transactional
                       (ds/in (ds/add (n/construct OutputProvider :context 119))
                         (ds/add (n/construct InputConsumer))
                         (ds/current-scope)))
           provider2 (g/transactional
                       (ds/in (ds/add (n/construct OutputProvider :context 113))
                         (ds/add (n/construct InputConsumer))
                         (ds/current-scope)))]
       (is (= 1 (count (ds/nodes-consuming provider1 :context))))
       (is (= 1 (count (ds/nodes-consuming provider2 :context))))))))

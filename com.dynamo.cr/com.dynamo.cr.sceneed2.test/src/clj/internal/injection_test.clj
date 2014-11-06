(ns internal.injection-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n :refer [Scope make-scope]]
            [dynamo.system :as ds :refer [transactional add in]]
            [dynamo.system.test-support :refer :all]
            [internal.node :as in]))

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
  (let [recv    (make-receiver)
        sender  (make-sender-1)
        sampler (make-sampler)
        labeler (make-labeler)]
    (is (= #{[sender  :surname recv :surname]} (in/injection-candidates [recv] [sender])))
    (is (= #{[sampler :sample  recv :samples]} (in/injection-candidates [recv] [sampler])))
    (is (= #{[labeler :label   recv :label]}   (in/injection-candidates [recv] [labeler])))))

(sm/defrecord CommonValueType
  [identifier :- String])

(defnk concat-all :- CommonValueType
  [local-names :- [CommonValueType]]
  (str/join (map :identifier local-names)))

(n/defnode ValueConsumer
  (input local-names [CommonValueType] :inject)
  (output concatenation CommonValueType concat-all))

(defnk passthrough [local-name] local-name)

(n/defnode InjectionScope
  (inherits Scope)
  (input local-name CommonValueType :inject)
  (output passthrough CommonValueType passthrough))

(n/defnode ValueProducer
  (property value {:schema CommonValueType})
  (output local-name CommonValueType [this _] (:value this)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-world
      (let [scope (ds/transactional
                   (ds/in
                    (ds/add (make-injection-scope))
                    (ds/add (make-value-producer :value (CommonValueType. "a known value")))
                    (ds/current-scope)))]
        (is (= "a known value" (-> scope (n/get-node-value :passthrough) :identifier))))))

  (testing "attach one node output to input on another node"
    (with-clean-world
      (let [consumer (ds/transactional
                      (ds/in
                       (ds/add (make-scope))
                       (ds/add (make-value-producer :value (CommonValueType. "a known value")))
                       (ds/add (make-value-consumer))))]
        (is (= "a known value" (-> consumer (n/get-node-value :concatenation)))))))

  (testing "attach nodes in different transactions"
    (with-clean-world
      (let [scope (ds/transactional
                    (ds/add (make-scope)))
            consumer (ds/transactional
                      (ds/in scope
                       (ds/add (make-value-consumer))))
            producer (ds/transactional
                       (ds/in scope
                         (ds/add (make-value-producer :value (CommonValueType. "a known value")))))]
        (is (= "a known value" (-> consumer (n/get-node-value :concatenation)))))))

  (testing "attach nodes in different transactions and reverse order"
    (with-clean-world
      (let [scope (ds/transactional
                    (ds/add (make-scope)))
            producer (ds/transactional
                       (ds/in scope
                         (ds/add (make-value-producer :value (CommonValueType. "a known value")))))
            consumer (ds/transactional
                      (ds/in scope
                       (ds/add (make-value-consumer))))]
        (is (= "a known value" (-> consumer (n/get-node-value :concatenation))))))))


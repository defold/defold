(ns internal.node-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.types :as t :refer [as-schema]]
            [dynamo.node :as n :refer [defnode]]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world]]
            [internal.node :as in :refer [deep-merge]]
            [internal.system :as is]
            [internal.query :as iq]
            [internal.transaction :as it]))

(def a-schema (as-schema {:names [java.lang.String]}))

(def m1 {:cached #{:derived}   :inputs {:simple-number 18 :another [:a] :nested ['v1] :setvalued #{:x}}})
(def m2 {:cached #{:expensive} :inputs {:simple-number 99 :another [:a] :nested ['v3] :setvalued #{:z}}})
(def expected-merge {:cached #{:expensive :derived},
                     :inputs {:simple-number 99
                              :another [:a :a],
                              :nested ['v1 'v3]
                              :setvalued #{:x :z}}})

(def s1           (assoc m1 :declared-type a-schema))
(def s2           (assoc m2 :declared-type a-schema))
(def schema-merge (assoc expected-merge :declared-type a-schema))

(deftest merging-nested-maps
  (is (= expected-merge (deep-merge m1 m2))))

(deftest merging-maps-with-schemas
  (is (= schema-merge (deep-merge s1 s2)))
  (is (:schema (meta (get (deep-merge s1 s2) :declared-type)))))

(defn simple-production-fn [this graph] "a produced value")
(defnk funky-production [this] "a funky value")

(defnode SimpleTestNode
  (property foo (t/string :default "FOO!"))

  (output an-output String simple-production-fn)
  (output inline-output String [this graph] "inlined function")
  (output symbol-param-production String funky-production))

(defnode NodeWithProtocols
  (property foo (t/string :default "the user"))

  clojure.lang.IDeref
  (deref [this] (:foo this))

  t/N2Extent
  (width [this] 800)
  (height [this] 600))

(defnode NodeWithEvents
  (on :mousedown
    (let [nn (ds/add (make-node-with-protocols :_id -1))]
      (ds/set-property {:_id -1} :foo "newly created")
      (ds/set-property self :message-processed true))))

(deftest node-definition
  (testing "properties"
    (is (= [:foo] (-> (make-simple-test-node) :descriptor :properties keys)))
    (is (contains? (make-simple-test-node) :foo)))
  (testing "property defaults"
    (is (= "FOO!" (-> (make-simple-test-node) :foo))))
  (testing "production functions"
    (is (= "a produced value" (-> (make-simple-test-node) (t/get-value nil :an-output))))
    (is (= "inlined function" (-> (make-simple-test-node) (t/get-value nil :inline-output))))
    (is (= "a funky value"    (-> (make-simple-test-node) (t/get-value nil :symbol-param-production)))))
  (testing "extending nodes with protocols"
    (is (instance? clojure.lang.IDeref (make-node-with-protocols)))
    (is (= "the user" @(make-node-with-protocols)))
    (is (satisfies? t/N2Extent (make-node-with-protocols)))
    (is (= 800 (t/width (make-node-with-protocols))))))

(deftest event-delivery
  (with-clean-world
    (let [evented (ds/transactional (ds/add (make-node-with-events :_id -1)))]
      (is (= :ok (t/process-one-event evented {:type :mousedown})))
      (is (:message-processed (iq/node-by-id world-ref (:_id evented)))))))

(deftest nodes-share-descriptors
  (is (identical? (-> (make-simple-test-node) :descriptor) (-> (make-simple-test-node) :descriptor))))

(defprotocol AProtocol
  (complainer [this]))

(definterface IInterface
  (allGood []))

(defnode MyNode
  AProtocol
  (complainer [this] :owie)
  IInterface
  (allGood [this] :ok))

(deftest node-respects-namespaces
  (testing "node can implement protocols not known/visible to internal.node"
    (is (= :owie (complainer (make-my-node)))))
  (testing "node can implement interface not known/visible to internal.node"
    (is (= :ok (.allGood (make-my-node))))))

(defnk depends-on-self [this] this)
(defnk depends-on-input [an-input] an-input)
(defnk depends-on-property [a-property] a-property)
(defnk depends-on-several [this project g an-input a-property] [this project g an-input a-property])
(defn  depends-on-default-params [this g] [this g])

(defnode DependencyTestNode
  (output depends-on-self s/Any depends-on-self)
  (output depends-on-input s/Any depends-on-input)
  (output depends-on-property s/Any depends-on-property)
  (output depends-on-several s/Any depends-on-several)
  (output depends-on-default-params s/Any depends-on-default-params))

(deftest dependency-mapping
  (testing "node reports its own dependencies"
    (let [test-node (make-dependency-test-node)
          deps      (t/output-dependencies test-node)]
      (are [input expected-deps] (and (contains? deps input) (= expected-deps (get deps input)))
           :depends-on-self           #{:this}
           :depends-on-input          #{:an-input}
           :depends-on-property       #{:a-property}
           :depends-on-several        #{:this :project :g :an-input :a-property}
           :depends-on-default-params #{:this :g})))
  (testing "node dependencies are registered on the world"
    (with-clean-world
      (let [test-node (ds/transactional
                       (ds/add (make-dependency-test-node)))
            dependency-map (:output-dependencies @world-ref)]
        (is (contains? dependency-map (:_id test-node)))
        (let [node-deps (get dependency-map (:_id test-node))]
          (are [input expected-deps] (and (contains? node-deps input) (= expected-deps (get node-deps input)))
               :depends-on-self           #{:this}
               :depends-on-input          #{:an-input}
               :depends-on-property       #{:a-property}
               :depends-on-several        #{:this :project :g :an-input :a-property}
               :depends-on-default-params #{:this :g}))))))

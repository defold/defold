(ns internal.node-test
  (:require [clojure.test :refer :all]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.types :as t :refer [as-schema]]
            [dynamo.node :as n :refer [defnode]]
            [dynamo.project.test-support :refer [clean-project]]
            [internal.node :as in :refer [deep-merge]]))

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
      (let [nn (new NodeWithProtocols :_id -1)]
        (set-property {:_id -1} :foo "newly created"))))

(deftest node-definition
  (testing "properties"
    (is (= [:foo] (-> (make-simple-test-node) :properties keys)))
    (is (contains? (make-simple-test-node) :foo)))
  (testing "property defaults"
    (is (= "FOO!" (-> (make-simple-test-node) :foo))))
  (testing "production functions"
    (is (= "a produced value" (-> (make-simple-test-node) (t/get-value nil :an-output {}))))
    (is (= "inlined function" (-> (make-simple-test-node) (t/get-value nil :inline-output {}))))
    (is (= "a funky value"    (-> (make-simple-test-node) (t/get-value nil :symbol-param-production {})))))
  (testing "extending nodes with protocols"
    (is (instance? clojure.lang.IDeref (make-node-with-protocols)))
    (is (= "the user" @(make-node-with-protocols)))
    (is (satisfies? t/N2Extent (make-node-with-protocols)))
    (is (= 800 (t/width (make-node-with-protocols)))))
  (testing "sending events to nodes"
    (is (= :ok (-> (make-node-with-events) (t/process-one-event (clean-project) {:type :mousedown}))))))

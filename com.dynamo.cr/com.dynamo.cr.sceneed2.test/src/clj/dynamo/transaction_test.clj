(ns dynamo.transaction-test
  (:require [clojure.core.async :as a]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [schema.core :as s]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.node :as n :refer [Scope]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.transaction :as it]))

(defn dummy-output [& _] :ok)

(n/defnode Resource
  (input a String)
  (output b s/Keyword dummy-output))

(n/defnode Downstream
  (input consumer String))

(def gen-node (gen/fmap (fn [n] (make-resource :_id n :original-id n)) (gen/such-that #(< % 0) gen/neg-int)))

(defspec tempids-resolve-correctly
  (prop/for-all [nn gen-node]
    (with-clean-world
      (let [before (:_id nn)]
        (= before (:original-id (first (tx-nodes world-ref nn))))))))

(deftest low-level-transactions
  (testing "one node with tempid"
    (with-clean-world
      (let [tx-result (it/transact world-ref (it/new-node (make-resource :_id -5 :a "known value")))]
        (is (= :ok (:status tx-result)))
        (is (= "known value" (:a (dg/node (:graph tx-result) (it/resolve-tempid tx-result -5))))))))
  (testing "two connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes world-ref (make-resource :_id -1) (make-downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            after                 (:graph (it/transact world-ref (it/connect resource1 :b resource2 :consumer)))]
        (is (= [id1 :b]        (first (lg/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (lg/targets after id1 :b)))))))
  (testing "disconnect two singly-connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes world-ref (make-resource :_id -1) (make-downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            tx-result             (it/transact world-ref (it/connect    resource1 :b resource2 :consumer))
            tx-result             (it/transact world-ref (it/disconnect resource1 :b resource2 :consumer))
            after                 (:graph tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= [] (lg/sources after id2 :consumer)))
        (is (= [] (lg/targets after id1 :b))))))
  (testing "simple update"
    (with-clean-world
      (let [[resource] (tx-nodes world-ref (make-resource :c 0))
            tx-result  (it/transact world-ref (it/update-node resource update-in [:c] + 42))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (:c (dg/node (:graph tx-result) (:_id resource)))))))))

(def trigger-called (atom 0))

(defn count-calls [& _]
  (swap! trigger-called inc))

(n/defnode CountingScope
  (inherits Scope)
  (property triggers {:default [#'count-calls]}))

(deftest trigger-runs-once
  (testing "attach one node output to input on another node"
    (with-clean-world
      (reset! trigger-called 0)
      (let [consumer (ds/transactional
                       (ds/in
                        (ds/add (make-counting-scope))
                        (ds/add (make-downstream))
                        (ds/add (make-resource))))]
        (is (= 1 @trigger-called))))))

(ns dynamo.project-test
  (:require [clojure.core.async :as a]
            [dynamo.project.test-support :as pst :refer :all]
            [dynamo.project :as p :refer [new-resource update-resource resolve-tempid transact connect disconnect]]
            [dynamo.node :as n]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]))

(use-fixtures :each with-clean-project)

(def labels (gen/elements [:t :u :v :w :x :y :z]))

(defn resource [id a b] {:_id id :inputs (zipmap a (repeat "")) :transforms (zipmap b (repeat ""))})

(def resources (gen/fmap (partial apply resource)
                     (gen/tuple
                       gen/neg-int
                       (gen/not-empty (gen/vector labels))
                       (gen/not-empty (gen/vector labels)))))

(defn tx-nodes [project resources]
  (let [tx-result (transact project (map new-resource resources))
        after (:graph tx-result)]
    (map #(dg/node after (resolve-tempid tx-result %)) (map :_id resources))))

(defspec tempids-resolve-correctly
  (prop/for-all [new-nodes (gen/vector resources)]
                (let [before (map #(assoc % :original-id (:_id %)) new-nodes)]
                  (= (map :_id before) (map :original-id (tx-nodes *test-project* before))))))

(def two-simple-resources
  [(resource -1 #{}     #{:next})
   (resource -2 #{:previous} #{})])

(deftest creation
  (testing "one node with tempid"
           (let [tx-result (transact *test-project* (new-resource {:_id -5 :indicator "known value"}))]
             (is (= :ok (:status tx-result)))
             (is (= "known value" (:indicator (dg/node (:graph tx-result) (resolve-tempid tx-result -5))))))))

(deftest connection
  (testing "two connected nodes"
    (let [[resource1 resource2] (tx-nodes *test-project* two-simple-resources)
          tx-result    (transact *test-project* [(connect resource1 :next resource2 :previous)])
          after        (:graph tx-result)]
      (is (= :ok (:status tx-result)))
      (is (= [(:_id resource1) :next] (first (lg/sources after (:_id resource2) :previous))))
      (is (= [(:_id resource2) :previous] (first (lg/targets after (:_id resource1) :next)))))))

(deftest disconnection
  (testing "disconnect two singly-connected nodes"
           (let [[resource1 resource2] (tx-nodes *test-project* two-simple-resources)
                 tx-result    (transact *test-project* [(connect resource1 :next resource2 :previous)])
                 tx-result    (transact *test-project* [(disconnect resource1 :next resource2 :previous)])
                 after        (:graph tx-result)]
	             (is (= :ok (:status tx-result)))
	             (is (= [] (lg/sources after (:_id resource2) :previous)))
	             (is (= [] (lg/targets after (:_id resource1) :next))))))

(deftest modifying-nodes
  (testing "simple update"
           (let [[resource] (tx-nodes *test-project* [(assoc (resource -5 #{} #{}) :counter 0)])
                 tx-result (transact *test-project* [(update-resource resource update-in [:counter] + 42)])]
             (is (= :ok (:status tx-result)))
             (is (= 42 (:counter (dg/node (:graph tx-result) (:_id resource))))))))


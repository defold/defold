(ns dynamo.project-test
  (:require [dynamo.project :as p :refer [transact new-resource resolve-tempid create-transaction commit-transaction connect]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]))

(defn with-clean-project
  [f]
  (dosync (ref-set p/project-state {:graph (dg/empty-graph)}))
  (f))

(use-fixtures :each with-clean-project)

(deftest creation
  (testing "one node with tempid"
           (let [tx-result (commit-transaction (new-resource {:_id -5 :indicator "known value"}))]
             (is (= :ok (:status tx-result)))
             (is (= "known value" (:indicator (dg/node (:graph tx-result) (resolve-tempid tx-result -5))))))))

(def labels (gen/elements [:t :u :v :w :x :y :z]))

(defn resource [id a b] {:_id id :inputs (zipmap a (repeat "")) :transforms (zipmap b (repeat ""))})

(def resources (gen/fmap (partial apply resource)
                     (gen/tuple
                       gen/neg-int
                       (gen/not-empty (gen/vector labels))
                       (gen/not-empty (gen/vector labels)))))

(defn tx-nodes [resources]
  (let [tx-result (commit-transaction (map new-resource resources))
        after (:graph tx-result)]
    (map #(dg/node after (resolve-tempid tx-result %)) (map :_id resources))))

(defspec tempids-resolve-correctly
  (prop/for-all [new-nodes (gen/vector resources)]
                (let [before (map #(assoc % :original-id (:_id %)) new-nodes)]
                  (= (map :_id before) (map :original-id (tx-nodes before))))))

(deftest connection
  (testing "two connected nodes"
	          (let [resource1 {:_id -1 :indicator "first"}
	                 resource2 {:_id -2 :indicator "second" }
	                 tx-result (commit-transaction
                                    [(new-resource resource1 #{} #{:next})
	                             (new-resource resource2 #{:previous} #{})
	                             (connect resource1 :next resource2 :previous)])
	                 resource1-id (resolve-tempid tx-result -1)
	                 resource2-id (resolve-tempid tx-result -2)
	                 after        (:graph tx-result)]
	             (is (= :ok (:status tx-result)))
	             (is (= "first"  (:indicator (dg/node after resource1-id))))
	             (is (= "second" (:indicator (dg/node after resource2-id))))
	             (is (= [resource1-id :next] (first (lg/sources after resource2-id :previous))))
	             (is (= [resource2-id :previous] (first (lg/targets after resource1-id :next)))))))

(ns atlas.core-test
  (:require [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [plumbing.fnk.pfnk :as pf]
            [dynamo.node :as dn] 
            [atlas.core :refer :all]
            [clojure.test :refer :all]
            [schema.core :as s]))

(deftest tree-outputs
  (testing "When I add a single node to the graph, it has no children in the tree"
           (let [atlas-node (make-atlas-node)
                 g (dn/add-node (dg/empty-graph) atlas-node)
                 atlas-id (dg/last-node-added g)]
             (is (= '() (:children (dn/get-value g atlas-id :tree))))))
  ;;(testing "When I add a child to an atlas node, the tree has one child")
  )

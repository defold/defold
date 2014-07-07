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
                 g          (dn/add-node (dg/empty-graph) atlas-node)
                 atlas-id   (dg/last-node g)
                 atlas-node (db/node g atlas-id)]
             (is (= '() (:children (dn/get-value g atlas-node :tree))))))
  (testing "When I add a child to an atlas node, the tree has one child"
           (let [g1 (dn/add-node (dg/empty-graph) (make-atlas-node))
                 g2 (dn/add-node g1               (make-image-node))
                 atlas-id (dg/last-node g1)
                 image-id (dg/last-node g2)
                 g3 (lg/connect g2 image-id :tree atlas-id :children)
                 children (:children (dn/get-value g3 atlas-node :tree))]

             (is (= 1 (count children)))
             (is (= image-id (:node-ref (first children)))))))

(deftest extension-is-replaced
  (is (= "foo.new-extension" (replace-extension "foo.old-extension" "new-extension")))
  (is (= "foo..less-terrible" (replace-extension "foo..terrible" "less-terrible")))
  (is (= "foo" (replace-extension "foo" "ignore-extension")))
  (is (= nil (replace-extension nil "doesnt-matter"))))

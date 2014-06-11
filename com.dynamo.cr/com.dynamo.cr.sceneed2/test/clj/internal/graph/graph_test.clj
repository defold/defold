(ns internal.graph.graph-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer [run-tests]]
            [internal.graph :refer :all]
            [internal.graph.generator :refer [graph]]))

(defn occurrences [p coll]
  (vals (frequencies (map p coll))))

(defspec no-duplicate-node-ids
  100
  (prop/for-all [g graph]
                (= 1 (apply max (occurrences id (:nodes g))))))

(defspec no-dangling-arcs
  50
  (prop/for-all [g graph]
                (every? #(and (node g (:source %)) 
                              (node g (:target %)))
                        (arcs g))))

;- Instantiate node

;; Given any graph
;;  when I create a node
;;  then I can discover the node's unique identifier

;; Given an empty graph
;;  when I create a node with a name and type
;;  then I can look up the node by its name
;;   and I can look up the node by its type

;; Given an existing graph
;;  when I create a node with a name and type
;;  then I can look up the node by its name
;;   and I can look up the node by its type

;; Given an graph that has a named node
;;  when I try to create a node with the same name
;;  then I can discover the node's unique identifier
;;   and I can look up the node by its name
;;   and I can look up the node by its identifier

;- Query for nodes matching criteria (satisfies a protocol or set of protocols, matches properties)

;; Given a graph with multiple nodes having the same name
;;  when I look up the nodes' name
;;  then I get a collection of nodes with that name

;; Given a populated graph
;;  when I query for nodes that match some criteria 
;;  then I get a possibly empty collection of all nodes matching those criteria

;; Given a graph with a node A with an output label "x"
;;   and a node B with an input label "y"
;;   and "y" can receive input from "x"
;;  when I ask to connect "x" to "y"
;;  then I can ask what the target of A's "x" output is
;;   and I can ask what the source of B's "y" input is

;; Given a graph with some nodes
;;  when I connect two nodes' outputs to one target node
;;  then each source node will have the same output
;;   and the target node will have both sources on the same input.

;; Given a graph with some nodes
;;   and I connect multiple nodes to the same input
;;  then each source node will have the same output
;;   and the target node will have them all on the same input

;; Given a graph with a node A with an output label "x"
;;   and a node B with an input label "y"
;;   and a connection from x to y
;;  when I disconnect A from B
;;  then x no longer appears in B's "y" input
;;   and A's "x" output will be empty

;; Given a graph with a node A with an output label "x"
;;   and a node B with an input label "y"
;;   and a connection from x to y
;;  when I remove A from the graph
;;  then x no longer appears in B's "y" input


;; Cases still needed for the following actions:
;- Delete node
;- Apply a function to a node
;- Traverse arcs by label
;- Transactions

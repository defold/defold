(ns internal.graph.graph-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [internal.graph.dgraph :refer :all]
            [internal.graph.generator :refer [graph names-with-repeats]]
            [internal.graph.query :refer :all]))

(defn occurrences [coll]
  (vals (frequencies coll)))

(defn with-extra-names [g ns]
  (reduce (fn [g n] (add-node g {:name n})) g ns))

(defn times-found [g n]
   (count (query g [[:name n]])) )

(defn times-expected [hist n] 
  (get hist n 0))

(defn random-graph [] (first (gen/sample graph 1)))

;- Most basic properties

(defspec no-duplicate-node-ids
  100
  (prop/for-all [g graph]
                (= 1 (apply max (occurrences (node-ids g))))))

;- Instantiate node

;; Given any graph
;;  when I create a node
;;  then I can discover the node's unique identifier

(deftest adding-node
  (let [v "Any node value at all"
        g  (add-node (random-graph) v) 
        id (last-node-added g)]
    (is (= v (node g id)))))

;; Given an empty graph
;;  when I create a node with a name
;;  then I can look up the node by its name

(defspec query-by-name
  100
  (prop/for-all [ns names-with-repeats
                 g  graph]
                (let [hist (frequencies ns)]
                  (every? #(= (times-found (with-extra-names g ns) %) (times-expected hist %)) ns))))

(deftest adding-named-node
  (testing "with unique result"
           (let [v  {:name "Atlas"}
                 g  (add-node (random-graph) v)
                 id (last-node-added g)]
             (is (= #{id} (query g [[:name "Atlas"]])))))
  (testing "with non-unique results"
           (let [v   {:name "Chronos"}
                 g   (add-node (random-graph) v)
                 id1 (last-node-added g)
                 g   (add-node g v)
                 id2 (last-node-added g)]
             (is (= #{id1 id2} (query g [[:name "Chronos"]]))))))


;; Given a graph that has a named node
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


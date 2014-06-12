(ns internal.graph.graph-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [internal.graph.dgraph :refer :all]
            [internal.graph.lgraph :refer :all]
            [internal.graph.generator :refer [graph names-with-repeats maybe-with-protocols ProtocolA ProtocolB]]
            [internal.graph.query :refer :all]))

(defn occurrences [coll]
  (vals (frequencies coll)))

(defn with-nodes [g vs]
  (for-graph g [v vs]
             (add-node g v)))

(defn with-extra-names [g ns]
  (with-nodes g (map (fn [n] {:name n}) ns)))

(defn times-found [g q]
   (count (query g q)))

(defn times-expected [hist n] 
  (get hist n 0))

(defn random-graph [] (first (gen/sample graph 1)))

;- Most basic properties

(defspec no-duplicate-node-ids
  100
  (prop/for-all [g graph]
                (= 1 (apply max (occurrences (node-ids g))))))

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
                  (every? #(= (times-found (with-extra-names g ns) [[:name %]]) (times-expected hist %)) ns))))

(defn- count-protocols
  [prots vs]
  (count (filter (fn [v] (every? #(satisfies? % v) prots)) vs)))

(defn- protocol-count-correct?
  [vs g & prots]
  (= (count-protocols prots vs)           
     (times-found g (mapv #(list 'protocol %) prots))))

;- Query for nodes matching criteria (satisfies a protocol or set of protocols, matches properties)

(defspec query-by-protocol
  100
  (prop/for-all [vs maybe-with-protocols
                 g graph]
                (let [g (with-nodes g vs)]
                  (and (protocol-count-correct? vs g ProtocolA)
                       (protocol-count-correct? vs g ProtocolB)
                       (protocol-count-correct? vs g ProtocolA ProtocolB)))))

(defn- arcs-are-reflexive
  [g]
  (and (every? #(= true %)
               (for [source       (node-ids g)
                     source-label (outputs source)
                     [target target-label] (targets g source source-label)]
                 (some #(= % [source source-label]) (sources g target target-label))))
       (every? #(= true %)
               (for [target       (node-ids g)
                     target-label (inputs target)
                     [source source-label] (sources g target target-label)]
                 (some #(= % [target target-label]) (targets g source source-label))))))

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


(defspec reflexivity
  (prop/for-all [g graph]
                (arcs-are-reflexive g)))

(deftest transformable
  (let [g  (add-node (random-graph) {:number 0})
        n  (last-node-added g)
        g' (transform-node g n update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (node g' n))))
    (is (= 0 (:number (node g n))))))

(run-tests)



;; Cases still needed for the following actions:
;- Traverse arcs by label
;- Transactions


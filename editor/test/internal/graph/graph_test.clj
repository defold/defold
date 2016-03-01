(ns internal.graph.graph-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.graph.generator :as ggen]
            [internal.graph.types :as gt]
            [schema.core :as s]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(defn occurrences [coll]
  (vals (frequencies coll)))

(defn random-graph
  []
  ((ggen/make-random-graph-builder)))

(deftest no-duplicate-node-ids
  (let [g (random-graph)]
    (is (= 1 (apply max (occurrences (ig/node-ids g)))))))

(deftest removing-node
  (let [v      "Any ig/node value"
        g      (random-graph)
        id     (inc (count (:nodes g)))
        g      (ig/add-node g id v)
        g      (ig/remove-node g id nil)]
    (is (nil? (ig/node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

(defn targets [g n l] (map gt/tail (filter #(= (.sourceLabel %) l) (get-in g [:sarcs n]))))
(defn sources [g n l] (map gt/head (filter #(= (.targetLabel %) l) (get-in g [:tarcs n]))))

(defn- source-arcs-without-targets
  [g]
  (for [source                (ig/node-ids g)
        source-label          (-> (ig/node g source) g/node-type gt/output-labels)
        [target target-label] (targets g source source-label)
        :when                 (not (some #(= % [source source-label]) (sources g target target-label)))]
    [source source-label]))

(defn- target-arcs-without-sources
  [g]
  (for [target                (ig/node-ids g)
        target-label          (-> (ig/node g target) g/node-type gt/input-labels)
        [source source-label] (sources g target target-label)
        :when                 (not (some #(= % [target target-label]) (targets g source source-label)))]
    [target target-label]))

(defn- arcs-are-reflexive?
  [g]
  (and (empty? (source-arcs-without-targets g))
       (empty? (target-arcs-without-sources g))))

(deftest reflexivity
  (let [g (random-graph)]
    (is (arcs-are-reflexive? g))))

(deftest transformable
  (let [g      (random-graph)
        id     (inc (count (:nodes g)))
        g      (ig/add-node g id {:number 0})
        g'     (ig/transform-node g id update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (ig/node g' id))))
    (is (= 0 (:number (ig/node g id))))))


(s/defrecord T1 [ident :- String])
(s/defrecord T2 [value :- Integer])

(deftest type-compatibility
  (are [first second compatible?]
    (= compatible? (g/type-compatible? first second))
    T1        T1           true
    T1        T2           false
    [T1]      [T1]         true
    [T1]      [T2]         false
    T1        [T2]         false
    [T1]      T2           false
    String    String       true
    String    [String]     false
    [String]  String       false
    [String]  [String]     true
    Integer   Number       true
    Integer   g/Num        true
    [Integer] [Number]     true
    [Number]  [Integer]    false
    T1        g/Any        true
    T1        [g/Any]      false
    [T1]      [g/Any]      true
    [T1]      g/Any        true
    String    g/Any        true
    String    [g/Any]      false
    [String]  g/Any        true
    [String]  [g/Any]      true))

(g/defnode TestNode
  (property val g/Str))

(deftest graph-override-cleanup
  (with-clean-system
    (let [[original override] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]
                                                      (:tx-data (g/override n))))
          basis (is/basis system)]
      (is (= override (first (ig/overrides basis original))))
      (g/transact (g/delete-node original))
      (let [basis (is/basis system)]
        (is (empty? (ig/overrides basis original)))
        (is (empty? (get-in basis [:graphs world :overrides])))))))

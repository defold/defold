(ns internal.graph.graph-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.graph.generator :as ggen]
            [internal.graph.types :as gt]
            [internal.node :as in]
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
    (is (nil? (ig/graph->node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

(defn targets [g n l] (map gt/tail (get-in g [:sarcs n l])))
(defn sources [g n l] (map gt/head (get-in g [:tarcs n l])))

(defn- source-arcs-without-targets
  [g]
  (for [source                (ig/node-ids g)
        source-label          (-> (ig/graph->node g source) g/node-type in/output-labels)
        [target target-label] (targets g source source-label)
        :when                 (not (some #(= % [source source-label]) (sources g target target-label)))]
    [source source-label]))

(defn- target-arcs-without-sources
  [g]
  (for [target                (ig/node-ids g)
        target-label          (-> (ig/graph->node g target) g/node-type in/input-labels)
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
    (is (= 1 (:number (ig/graph->node g' id))))
    (is (= 0 (:number (ig/graph->node g id))))))

(g/deftype T1        String)
(g/deftype T1-array  [String])
(g/deftype T2        Integer)
(g/deftype T2-array  [Integer])
(g/deftype Num       Number)
(g/deftype Num-array [Number])
(g/deftype Any       s/Any)
(g/deftype Any-array [s/Any])

(g/type-compatible? T1-array Any-array)

(deftest type-compatibility
  (are [first second compatible?] (= compatible? (g/type-compatible? first second))
    T1        T1         true
    T1        T2         false
    T1        T1-array   false
    T1-array  T1         false
    T1-array  T1-array   true
    T1-array  T2-array   false
    T1        T2-array   false
    T1-array  T2         false
    T2        Num        true
    T2-array  Num-array  true
    Num-array T2-array   false
    T1        Any        true
    T1        Any-array  false
    T1-array  Any-array  true
    T1-array  g/Any      true))

(g/defnode TestNode
  (property val g/Str))

(deftest graph-override-cleanup
  (with-clean-system
    (let [[original override] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]
                                                      (:tx-data (g/override n))))
          basis (is/basis system)]
      (is (= override (first (ig/get-overrides basis original))))
      (g/transact (g/delete-node original))
      (let [basis (is/basis system)]
        (is (empty? (ig/get-overrides basis original)))
        (is (empty? (get-in basis [:graphs world :overrides])))))))

(deftest graph-values
  (with-clean-system
    (g/set-graph-value! world :things {})
    (is (= {} (g/graph-value world :things)))
    (g/transact (g/update-graph-value world :things assoc :a 1))
    (is (= {:a 1} (g/graph-value world :things)))))

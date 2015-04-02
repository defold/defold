(ns internal.graph.graph-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [internal.graph :as ig]
            [internal.graph.generator :as ggen]
            [internal.graph.types :as gt]))

(defn occurrences [coll]
  (vals (frequencies coll)))

(defn with-nodes [g vs]
  (ig/for-graph g [v vs]
             (ig/add-node g v)))

(defn with-extra-names [g ns]
  (with-nodes g (map (fn [n] {:name n}) ns)))

(defn times-found [g q]
   (count (ig/query g q)))

(defn times-expected [hist n]
  (get hist n 0))

(defn random-graph
  []
  ((ggen/make-random-graph-builder)))

(deftest no-duplicate-node-ids
  (let [g (random-graph)]
    (is (= 1 (apply max (occurrences (ig/node-ids g)))))))

(defn sarc-reciprocated? [g source-arc]
  (some #{(gt/head source-arc)} (apply ig/sources g (gt/tail source-arc))))

(defn tarc-reciprocated? [g target-arc]
  (some #{(gt/tail target-arc)} (apply ig/targets g (gt/head target-arc))))

(deftest no-dangling-arcs
  (let [g (random-graph)]
    (is (every? #(sarc-reciprocated? g %) (:sarcs g)))
    (is (every? #(tarc-reciprocated? g %) (:tarcs g)))))

(deftest adding-node
  (let [v       "Any ig/node value at all"
        g       (random-graph)
        [g' id] (ig/claim-id g)
        g       (ig/add-node g' id v)]
    (is (= v (ig/node g id)))))

(deftest removing-node
  (let [v      "Any ig/node value"
        [g id] (ig/claim-id (random-graph))
        g      (ig/add-node g id v)
        g      (ig/remove-node g id)]
    (is (nil? (ig/node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

(defn- arcs-are-reflexive?
  [g]
  (and (every? #(= true %)
               (for [source       (ig/node-ids g)
                     source-label (gt/outputs (ig/node g source))
                     [target target-label] (ig/targets g source source-label)]
                 (some #(= % [source source-label]) (ig/sources g target target-label))))
       (every? #(= true %)
               (for [target       (ig/node-ids g)
                     target-label (gt/inputs (ig/node g target))
                     [source source-label] (ig/sources g target target-label)]
                 (some #(= % [target target-label]) (ig/targets g source source-label))))))

(deftest reflexivity
  (let [g (random-graph)]
    (is (arcs-are-reflexive? g))))

(deftest transformable
  (let [[g id] (ig/claim-id  (random-graph))
        g      (ig/add-node g id {:number 0})
        g'     (ig/transform-node g id update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (ig/node g' id))))
    (is (= 0 (:number (ig/node g id))))))

(defn- add-child
  [g parent-id child-node]
  (let [[g id] (ig/claim-id g)
        g      (ig/add-node g id child-node)]
    (-> g
        (ig/connect-source id :parent parent-id :children)
        (ig/connect-target id :parent parent-id :children))))

(defprotocol ProtocolA)
(defprotocol ProtocolB)

(defrecord HierarchyNode []
  gt/Node
  (inputs [_] #{:children})
  (outputs [_] #{:parent})
  ProtocolA)

(defrecord NamedItem [name]
  gt/Node
  (inputs [_] #{})
  (outputs [_] #{:parent})
  ProtocolB)

(defrecord Atlas [name]
  gt/Node
  (inputs [_] #{:children})
  (outputs [_] #{:textureset}))

(defn- nodeset [g id-coll] (into #{} (map #(ig/node g %) id-coll)))

(deftest query-by-arc-label
  (let [kids          #{(NamedItem. "image1") (NamedItem. "image2") (NamedItem. "image3")}
        parent        (Atlas. "Atlas")
        [g id]        (ig/claim-id (ig/empty-graph))
        g             (ig/add-node g id parent)
        g             (reduce (fn [g child] (add-child g id child)) g kids)
        query-kids    (ig/query g '[[:name "Atlas"]  (input :children)])
        query-parents (ig/query g '[[:name "image1"] (output :parent)])]
    (def g* g)
    (is (= kids      (nodeset g query-kids)))
    (is (= #{parent} (nodeset g query-parents)))))

(deftest query-by-protocol
  (let [hn   (HierarchyNode.)
        ni   (NamedItem. "an-item")
        junk (repeatedly 100 #(Atlas. (str (gensym))))
        g    (ig/empty-graph)
        g    (reduce
              (fn [g node]
                (let [[g id] (ig/claim-id g)]
                  (ig/add-node g id node)))
              g (list* hn ni junk))]
    (is (= #{hn} (nodeset g (ig/query g '[(protocol internal.graph.graph-test/ProtocolA)]))))
    (is (= #{ni} (nodeset g (ig/query g '[(protocol internal.graph.graph-test/ProtocolB)]))))))

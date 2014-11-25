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

(deftest adding-node
  (let [v "Any node value at all"
        g  (add-node (random-graph) v)
        id (last-node g)]
    (is (= v (node g id)))))

(deftest removing-node
  (let [v "Any node value"
        g (add-node (random-graph) v)
        id (last-node g)
        g (remove-node g id)]
    (is (nil? (node g id)))
    (is (empty? (filter #(= "Any node value" %) (node-values g))))))

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


(defspec reflexivity
  (prop/for-all [g graph]
                (arcs-are-reflexive g)))

(deftest transformable
  (let [g  (add-node (random-graph) {:number 0})
        n  (last-node g)
        g' (transform-node g n update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (node g' n))))
    (is (= 0 (:number (node g n))))))

(defn- add-child
  [g pnode cld-val]
  (let [g' (add-labeled-node g #{} #{:parent} cld-val)]
    (connect g' (last-node g') :parent pnode :children)))

(deftest query-by-arc-label
  (let [vs      #{"image1" "image2" "image3"}
        g       (add-labeled-node (empty-graph) #{:children} #{:textureset} {:name "Atlas"})
        p       (last-node g)
        g       (for-graph g [n vs] (add-child g p {:name n}))
        kids    (query g '[[:name "Atlas"] (input :children)])
        parent  (query g '[[:name "image1"] (output :parent)])]
    (is (= vs (into #{} (map #(:name (node g %)) kids))))
    (is (= #{p} parent))))

(deftest arcs-is-non-lazy
  (testing "empty"
    (is (vector? (arcs (empty-graph)))))
  (testing "after add node"
    (is (vector? (arcs (-> (empty-graph) (add-node :node1)))))
    (is (vector? (arcs (-> (empty-graph) (add-node :node1) (add-node :node2))))))
  (testing "after remove node"
    (let [g (add-node (empty-graph) :node1)]
      (is (vector? (arcs (remove-node g (last-node g)))))
      (is (vector? (arcs (remove-node g (next-node g)))))))
  (testing "after add arc"
    (let [g     (empty-graph)
          g     (add-node g :node1)
          node1 (last-node g)
          g     (add-node g :node2)
          node2 (last-node g)]
      (is (vector? (arcs (-> g (add-arc node1 :from  node1 :to)))))
      (is (vector? (arcs (-> g (add-arc node1 :from  node2 :to)))))
      (is (vector? (arcs (-> g (add-arc node1 :from  node2 :to)
                               (add-arc node1 :from  node2 :to)))))
      (is (vector? (arcs (-> g (add-arc node1 :from1 node2 :to1)
                               (add-arc node1 :from2 node2 :to2)))))))
  (testing "after remove arc"
    (let [g     (empty-graph)
          g     (add-node g :node1)
          node1 (last-node g)
          g     (add-node g :node2)
          node2 (last-node g)
          g     (add-arc g node1 :from node1 :to)
          g     (add-arc g node1 :from node2 :to)]
      (is (vector? (arcs (remove-arc g node1         :from     node1         :to))))
      (is (vector? (arcs (remove-arc g node1         :from     node2         :to))))
      (is (vector? (arcs (remove-arc g node1         :not-from node2         :to))))
      (is (vector? (arcs (remove-arc g node1         :from     node2         :not-to))))
      (is (vector? (arcs (remove-arc g node1         :from     (next-node g) :to))))
      (is (vector? (arcs (remove-arc g (next-node g) :from     node1         :to))))
      (is (vector? (arcs (remove-arc g (next-node g) :from     (next-node g) :to))))))
  (testing "after remove node with arcs"
    (let [g     (empty-graph)
          g     (add-node g :node1)
          node1 (last-node g)
          g     (add-node g :node2)
          node2 (last-node g)
          g     (add-arc g node1 :from node1 :to)
          g     (add-arc g node1 :from node2 :to)]
      (is (vector? (arcs (remove-node g node1))))
      (is (vector? (arcs (remove-node g node2)))))))

(deftest checking-arcs
  (let [g           (empty-graph)
        g           (add-labeled-node g #{:textureset} #{:datafile} {:name "AtlasSaver"})
        grandparent (last-node g)
        g           (add-labeled-node g #{:children} #{:textureset} {:name "Atlas"})
        parent      (last-node g)
        g           (connect g parent :textureset grandparent :textureset)
        g           (add-labeled-node g #{} #{:parent} {:name "image1"})
        img1        (last-node g)
        g           (connect g img1 :parent parent :children)
        g           (add-labeled-node g #{} #{:parent} {:name "image2"})
        img2        (last-node g)
        g           (connect g img2 :parent parent :children)
        g           (add-labeled-node g #{} #{:parent} {:name "image3"})
        img3        (last-node g)
        g           (connect g img3 :parent parent :children)
        g           (add-labeled-node g #{:image} #{} {:name "sprite"})
        sprite      (last-node g)
        g           (connect g img3 :parent sprite :image)]
    (is (= [{:source parent :source-attributes {:label :textureset}
             :target grandparent :target-attributes {:label :textureset}}]
           (arcs-from-to g parent grandparent)))))

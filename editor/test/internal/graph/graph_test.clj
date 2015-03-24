(ns internal.graph.graph-test
  (:require [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [internal.graph :as ig]
            [internal.graph.generator :refer [graph names-with-repeats maybe-with-protocols ProtocolA ProtocolB]]))

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

(defn random-graph [] (first (gen/sample graph 1)))

;- Most basic properties

(defspec no-duplicate-node-ids
  100
  (prop/for-all [g graph]
                (= 1 (apply max (occurrences (ig/node-ids g))))))

(defspec no-dangling-arcs
  50
  (prop/for-all [g graph]
                (every? #(and (ig/node g (:source %))
                              (ig/node g (:target %)))
                        (ig/arcs g))))

;- Instantiate ig/node

(deftest adding-node
  (let [v "Any ig/node value at all"
        g  (ig/add-node (random-graph) v)
        id (ig/last-node g)]
    (is (= v (ig/node g id)))))

(deftest removing-node
  (let [v "Any ig/node value"
        g (ig/add-node (random-graph) v)
        id (ig/last-node g)
        g (ig/remove-node g id)]
    (is (nil? (ig/node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

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
               (for [source       (ig/node-ids g)
                     source-label (ig/outputs source)
                     [target target-label] (ig/targets g source source-label)]
                 (some #(= % [source source-label]) (ig/sources g target target-label))))
       (every? #(= true %)
               (for [target       (ig/node-ids g)
                     target-label (ig/inputs target)
                     [source source-label] (ig/sources g target target-label)]
                 (some #(= % [target target-label]) (ig/targets g source source-label))))))


(defspec reflexivity
  (prop/for-all [g graph]
                (arcs-are-reflexive g)))

(deftest transformable
  (let [g  (ig/add-node (random-graph) {:number 0})
        n  (ig/last-node g)
        g' (ig/transform-node g n update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (ig/node g' n))))
    (is (= 0 (:number (ig/node g n))))))

(defn- add-child
  [g pnode cld-val]
  (let [g' (ig/add-labeled-node g #{} #{:parent} cld-val)]
    (ig/connect g' (ig/last-node g') :parent pnode :children)))

(deftest query-by-arc-label
  (let [vs      #{"image1" "image2" "image3"}
        g       (ig/add-labeled-node (ig/empty-graph) #{:children} #{:textureset} {:name "Atlas"})
        p       (ig/last-node g)
        g       (ig/for-graph g [n vs] (add-child g p {:name n}))
        kids    (ig/query g '[[:name "Atlas"] (input :children)])
        parent  (ig/query g '[[:name "image1"] (output :parent)])]
    (is (= vs (into #{} (map #(:name (ig/node g %)) kids))))
    (is (= #{p} parent))))

(deftest arcs-is-non-lazy
  (testing "empty"
    (is (vector? (ig/arcs (ig/empty-graph)))))
  (testing "after add ig/node"
    (is (vector? (ig/arcs (-> (ig/empty-graph) (ig/add-node :node1)))))
    (is (vector? (ig/arcs (-> (ig/empty-graph) (ig/add-node :node1) (ig/add-node :node2))))))
  (testing "after remove ig/node"
    (let [g (ig/add-node (ig/empty-graph) :node1)]
      (is (vector? (ig/arcs (ig/remove-node g (ig/last-node g)))))
      (is (vector? (ig/arcs (ig/remove-node g (ig/next-node g)))))))
  (testing "after add arc"
    (let [g     (ig/empty-graph)
          g     (ig/add-node g :node1)
          node1 (ig/last-node g)
          g     (ig/add-node g :node2)
          node2 (ig/last-node g)]
      (is (vector? (ig/arcs (-> g (ig/add-arc node1 :from  node1 :to)))))
      (is (vector? (ig/arcs (-> g (ig/add-arc node1 :from  node2 :to)))))
      (is (vector? (ig/arcs (-> g (ig/add-arc node1 :from  node2 :to)
                               (ig/add-arc node1 :from  node2 :to)))))
      (is (vector? (ig/arcs (-> g (ig/add-arc node1 :from1 node2 :to1)
                               (ig/add-arc node1 :from2 node2 :to2)))))))
  (testing "after remove arc"
    (let [g     (ig/empty-graph)
          g     (ig/add-node g :node1)
          node1 (ig/last-node g)
          g     (ig/add-node g :node2)
          node2 (ig/last-node g)
          g     (ig/add-arc g node1 :from node1 :to)
          g     (ig/add-arc g node1 :from node2 :to)]
      (is (vector? (ig/arcs (ig/remove-arc g node1         :from     node1         :to))))
      (is (vector? (ig/arcs (ig/remove-arc g node1         :from     node2         :to))))
      (is (vector? (ig/arcs (ig/remove-arc g node1         :not-from node2         :to))))
      (is (vector? (ig/arcs (ig/remove-arc g node1         :from     node2         :not-to))))
      (is (vector? (ig/arcs (ig/remove-arc g node1         :from     (ig/next-node g) :to))))
      (is (vector? (ig/arcs (ig/remove-arc g (ig/next-node g) :from     node1         :to))))
      (is (vector? (ig/arcs (ig/remove-arc g (ig/next-node g) :from     (ig/next-node g) :to))))))
  (testing "after remove ig/node with ig/arcs"
    (let [g     (ig/empty-graph)
          g     (ig/add-node g :node1)
          node1 (ig/last-node g)
          g     (ig/add-node g :node2)
          node2 (ig/last-node g)
          g     (ig/add-arc g node1 :from node1 :to)
          g     (ig/add-arc g node1 :from node2 :to)]
      (is (vector? (ig/arcs (ig/remove-node g node1))))
      (is (vector? (ig/arcs (ig/remove-node g node2)))))))

(deftest checking-arcs
  (let [g           (ig/empty-graph)
        g           (ig/add-labeled-node g #{:textureset} #{:datafile} {:name "AtlasSaver"})
        grandparent (ig/last-node g)
        g           (ig/add-labeled-node g #{:children} #{:textureset} {:name "Atlas"})
        parent      (ig/last-node g)
        g           (ig/connect g parent :textureset grandparent :textureset)
        g           (ig/add-labeled-node g #{} #{:parent} {:name "image1"})
        img1        (ig/last-node g)
        g           (ig/connect g img1 :parent parent :children)
        g           (ig/add-labeled-node g #{} #{:parent} {:name "image2"})
        img2        (ig/last-node g)
        g           (ig/connect g img2 :parent parent :children)
        g           (ig/add-labeled-node g #{} #{:parent} {:name "image3"})
        img3        (ig/last-node g)
        g           (ig/connect g img3 :parent parent :children)
        g           (ig/add-labeled-node g #{:image} #{} {:name "sprite"})
        sprite      (ig/last-node g)
        g           (ig/connect g img3 :parent sprite :image)]
    (is (= [{:source parent :source-attributes {:label :textureset}
             :target grandparent :target-attributes {:label :textureset}}]
           (ig/arcs-from-to g parent grandparent)))))

(deftest multiple-arcs
  (let [g    (ig/empty-graph)
        g    (ig/add-node g :anim)
        anim (ig/last-node g)
        g    (ig/add-node g :img1)
        img1 (ig/last-node g)
        g    (ig/add-node g :img2)
        img2 (ig/last-node g)
        ops  [#(ig/add-arc    % img1 :out anim :in)
              #(ig/add-arc    % img2 :out anim :in)
              #(ig/add-arc    % img1 :out anim :in)
              #(ig/remove-arc % img1 :out anim :in)
              #(ig/remove-arc % img1 :out anim :in)
              #(ig/remove-arc % img1 :out anim :in)]
        gs   (reductions (fn [g op] (op g)) g ops)
        arcs (->> gs (map #(ig/arcs-to % anim)) (map #(map :source %)))]
    (is (= [[]
            [img1]
            [img1 img2]
            [img1 img2 img1]
            [img1 img2]
            [img2]
            [img2]]
           arcs))))

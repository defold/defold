(ns internal.graph.generator
  "test.check generator to create a randomly populated graph"
  (:require [clojure.test.check.generators :as gen]
            [internal.graph :as ig]))

(def names-with-repeats
  (gen/vector
    (gen/frequency [[9 (gen/not-empty gen/string-alpha-numeric)]
                    [1 (gen/return "A common name")]])))

(defprotocol ProtocolA)
(defprotocol ProtocolB)

(deftype TypeA [] ProtocolA)
(deftype TypeB [] ProtocolB)
(deftype TypeAB [] ProtocolA ProtocolB)

(def maybe-with-protocols
  (gen/vector
    (gen/frequency [[1 (gen/return {})]
                    [1 (gen/return (TypeA.))]
                    [1 (gen/return (TypeB.))]
                    [1 (gen/return (TypeAB.))]])))

(def min-node-count 20)
(def max-node-count 80)
(def min-arc-count  10)
(def max-arc-count  50)

(def labels (gen/elements [:t :u :v :w :x :y :z]))

(defn node [id a b] {:_id id :inputs a :outputs b})

(defn pair
  [m n]
  (gen/tuple (gen/elements m) (gen/elements n)))

(defn graph-endpoints
  [g efn]
  (for [id    (ig/node-ids g)
        input (efn (ig/node g id))]
    [id input]))

(defn arcs
  [g]
  (when (or (empty? (graph-endpoints g ig/outputs) ) (empty? (graph-endpoints g ig/inputs)))
    (prn "empty collection of endpoints on " g))
  (pair (graph-endpoints g ig/outputs) (graph-endpoints g ig/inputs)))

(defn selected-nodes
  [g]
  (gen/not-empty (gen/vector (gen/elements (ig/node-ids g)))))

(def nodes (gen/fmap (partial apply node)
                     (gen/tuple
                       gen/neg-int
                       (gen/not-empty (gen/vector labels))
                       (gen/not-empty (gen/vector labels)))))

(defn- add-arcs
  [g arcs]
  (ig/for-graph g [a arcs]
             (apply ig/connect g (flatten a))))

(defn- remove-arcs
  [g arcs]
  (ig/for-graph g [a arcs]
             (apply ig/disconnect g (flatten a))))

(defn populate-graph
  [g nodes]
  (ig/for-graph g [n nodes]
             (ig/add-labeled-node g (:inputs n) (:outputs n) {})))

(defn remove-nodes
  [g nodes]
  (ig/for-graph g [n nodes]
             (ig/remove-node g n)))

(def disconnected-graph
  (gen/bind (gen/resize max-node-count gen/s-pos-int)
            (fn [node-count]
              (gen/fmap #(populate-graph (ig/empty-graph) %) (gen/vector nodes min-node-count max-node-count)))))

(def connected-graph
  (gen/bind disconnected-graph
            (fn [g]
              (gen/fmap (partial add-arcs g)
                        (gen/vector (arcs g) min-arc-count max-arc-count)))))


(def decimated-graph
  (gen/bind connected-graph
            (fn [g]
              (gen/fmap (partial remove-nodes g)
                        (gen/resize (/ (count (ig/node-ids g)) 4) (selected-nodes g))))))

(def graph
  (gen/bind decimated-graph
            (fn [g]
              (gen/fmap (partial remove-arcs g)
                        (gen/vector (arcs g) min-arc-count (/ max-arc-count 2))))))

;(first (gen/sample (gen/resize 100 graph) 1))
;(gen/sample (selected-nodes (first (gen/sample (gen/resize 50 connected-graph) 1))) 10)

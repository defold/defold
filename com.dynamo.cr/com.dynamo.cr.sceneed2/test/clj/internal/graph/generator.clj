(ns internal.graph.generator
  "test.check generator to create a randomly populated graph"
  (:require [clojure.test.check.generators :as gen]
            [internal.graph.dgraph :as g]
            [internal.graph.lgraph :as lg]))

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

(defn node [a b] {:inputs (set a) :outputs (set b)})

(defn pair
  [m n]
  (gen/tuple (gen/elements m) (gen/elements n)))

(defn graph-endpoints 
  [g efn]
  (for [id    (g/node-ids g)
        input (efn (g/node g id))]
    [id input]))

(defn arcs
  [g]
  (when (or (empty? (graph-endpoints g lg/outputs) ) (empty? (graph-endpoints g lg/inputs)))
    (prn "empty collection of endpoints on " g))
  (pair (graph-endpoints g lg/outputs) (graph-endpoints g lg/inputs)))

(defn selected-nodes
  [g]
  (gen/not-empty (gen/vector (gen/elements (g/node-ids g)))))

(def nodes (gen/fmap (partial apply node)
                     (gen/tuple (gen/not-empty (gen/vector labels)) 
                                (gen/not-empty (gen/vector labels)))))

(defn- add-arcs
  [g arcs]
  (for-graph g [a arcs]
             (apply lg/connect g (flatten a))))

(defn- remove-arcs
  [g arcs]
  (for-graph g [a arcs]
             (apply lg/disconnect g (flatten a))))

(defn populate-graph
  [g nodes]
  (for-graph g [n nodes]
             (lg/add-labeled-node g (:inputs n) (:outputs n) {})))

(defn remove-nodes
  [g nodes]
  (for-graph g [n nodes]
             (g/remove-node g n)))

(def disconnected-graph
  (gen/bind (gen/resize max-node-count gen/s-pos-int)
            (fn [node-count]
              (gen/fmap #(populate-graph (g/empty-graph) %) (gen/vector nodes min-node-count max-node-count)))))

(def connected-graph
  (gen/bind disconnected-graph
            (fn [g]
              (gen/fmap (partial add-arcs g) 
                        (gen/vector (arcs g) min-arc-count max-arc-count)))))


(def decimated-graph
  (gen/bind connected-graph
            (fn [g]
              (gen/fmap (partial remove-nodes g) 
                        (gen/resize (/ (count (g/node-ids g)) 4) (selected-nodes g))))))

(def graph
  (gen/bind decimated-graph
            (fn [g]
              (gen/fmap (partial remove-arcs g)
                        (gen/vector (arcs g) min-arc-count (/ max-arc-count 2))))))

;(first (gen/sample (gen/resize 100 graph) 1))
;(gen/sample (selected-nodes (first (gen/sample (gen/resize 50 connected-graph) 1))) 10)
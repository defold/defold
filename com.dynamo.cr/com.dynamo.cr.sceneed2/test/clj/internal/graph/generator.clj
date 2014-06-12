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

(def max-node-count 40)
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
  (pair (graph-endpoints g lg/inputs) (graph-endpoints g lg/outputs)))

(def nodes (gen/fmap (partial apply node)
                     (gen/tuple (gen/such-that not-empty (gen/vector labels)) 
                                (gen/such-that not-empty (gen/vector labels)))))

(defn add-arcs
  [g arcs]
  (reduce (fn [g arc]
            (apply g/add-arc g (flatten arc)))
          g arcs))

(defn build-graph
  [nodes]
  (reduce (fn [g v]
            (lg/add-labeled-node g (:inputs v) (:outputs v) {})) 
          (g/empty-graph) 
          nodes))

(def disconnected-graph (gen/bind (gen/resize max-node-count gen/s-pos-int)
                                  (fn [node-count]
                                    (gen/fmap build-graph (gen/vector nodes 1 node-count)))))

(def graph (gen/bind disconnected-graph
                     (fn [g] (gen/fmap (partial add-arcs g) 
                                       (gen/vector (arcs g) min-arc-count max-arc-count)))))

(ns internal.graph.generator
  (:require [clojure.test.check.generators :as gen]
            [internal.graph :as g]))

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
  (pair (graph-endpoints g :inputs) (graph-endpoints g :outputs)))

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
            (g/add-node g v)) 
          (g/empty-graph) 
          nodes))

(def disconnected-graph (gen/bind (gen/resize max-node-count gen/s-pos-int)
                                  (fn [node-count]
                                    (gen/fmap build-graph (gen/vector nodes 1 node-count)))))

(def graph (gen/bind disconnected-graph
                     (fn [g] (gen/fmap (partial add-arcs g) 
                                       (gen/vector (arcs g) min-arc-count max-arc-count)))))

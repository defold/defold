(ns internal.graph.dgraph
  "Directed graph implementation. Nodes are simply a map from id to value.
   Arcs are a relation over node IDs."
  (:require [clojure.set :as set]
            [dynamo.util :refer [removev]]))

;;; Implementation details

(defn empty-graph
  []
  {:nodes {}
   :last-node 0
   :arcs []})


;;; Published API
(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))
(defn arcs        [g] (:arcs g))
(defn last-node   [g] (:last-node g))

(defn next-node   [g] (inc (last-node g)))
(defn- claim-node [g] (assoc-in g [:last-node] (next-node g)))

(defn node        [g id] (get-in g [:nodes id]))

(defn add-node
  [g v]
  (let [g' (claim-node g)]
    (assoc-in g' [:nodes (last-node g')] v)))

(defn remove-node
  [g n]
  (-> g
    (update-in [:nodes] dissoc n)
    (update-in [:arcs] (fn [arcs] (removev #(or (= n (:source %))
                                                (= n (:target %))) arcs)))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn add-arc
  [g source source-attributes target target-attributes]
  (assert (node g source) (str source " does not exist in graph"))
  (assert (node g target) (str target " does not exist in graph"))
  (update-in g [:arcs] conj {:source source :source-attributes source-attributes :target target :target-attributes target-attributes}))

(defn remove-arc
  [g source source-attributes target target-attributes]
  (update-in g [:arcs]
    (fn [arcs]
      (if-let [last-index (->> arcs (keep-indexed (fn [i a] (when (= {:source source :source-attributes source-attributes :target target :target-attributes target-attributes} a) i))) last)]
        (let [[n m] (split-at last-index arcs)] (into (vec n) (rest m)))
        arcs))))

(defn arcs-from-to [g source target]
  (filter #(and (= source (:source %)) (= target (:target %))) (:arcs g)))

(defn arcs-from [g node]
  (filter #(= node (:source %)) (:arcs g)))

(defn arcs-to [g node]
  (filter #(= node (:target %)) (:arcs g)))

(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

(defn map-nodes
  [f g]
  (update-in g [:nodes]
             #(persistent!
               (reduce-kv
                (fn [nodes id v] (assoc! nodes id (f v)))
                (transient {})
                %))))

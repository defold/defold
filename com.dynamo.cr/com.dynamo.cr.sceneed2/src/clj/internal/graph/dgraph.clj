(ns internal.graph.dgraph
  "Directed graph implementation. Nodes are simply a map from id to value.
   Arcs are a relation over node IDs.")

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
    (update-in [:arcs] (fn [arcs] (remove #(or (= n (:source %))
                                               (= n (:target %))) arcs)))))

(defn transform-node
  [g n f & args]
  (assert (node g n))
  (update-in g [:nodes n] #(apply f % args)))

(defn add-arc
  [g source source-attributes target target-attributes]
  (assert (node g source) (str source " does not exist in graph"))
  (assert (node g target) (str target " does not exist in graph"))
  (update-in g [:arcs] conj {:source source :source-attributes source-attributes :target target :target-attributes target-attributes}))

(defn remove-arc
  [g source source-attributes target target-attributes]
  (update-in g [:arcs] (fn [arcs] (remove #(= % {:source source :source-attributes source-attributes :target target :target-attributes target-attributes}) arcs))))

(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

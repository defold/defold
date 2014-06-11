(ns internal.graph)

(defn empty-graph
  []
  {:nodes {}
   :next-node-id 0
   :arcs []})

(defn- next-node-id
  [g]
  (:next-node-id g))

(defn- claim-node-id
  [g]
  (update-in g [:next-node-id] inc))

(defn id [node] (:id node))

(defn node-ids
  [g]
  (keys (:nodes g)))

(defn node-values
  [g]
  (vals (:nodes g)))

(defn arcs
  [g]
  (:arcs g))

(defn last-node-added
  [g]
  (:last-node-added g))

(defn node 
  [g id]
  (get-in g [:nodes id]))

(defn add-node
  [g v]
  (-> (claim-node-id g)
    (assoc-in [:nodes (next-node-id g)] v)
    (assoc :last-node-added (next-node-id g))))

(defn add-arc
  [g source source-attributes target target-attributes]
  (update-in g [:arcs] conj {:source source :source-attributes source-attributes :target target :target-attributes target-attributes}))
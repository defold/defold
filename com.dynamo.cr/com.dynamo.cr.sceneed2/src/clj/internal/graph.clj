(ns internal.graph)

(defn empty-graph
  []
  {:nodes []
   :next-node-id 0
   :arcs []})

(defn- next-node-id
  [g]
  (:next-node-id g))

(defn- claim-node-id
  [g]
  (update-in g [:next-node-id] inc))

(defn- make-node
  [g inputs outputs]
  {:id (next-node-id g)
   :inputs inputs
   :outputs outputs})

(defn id [node]      (:id node))
(defn inputs [node]  (:inputs node))
(defn outputs [node] (:outputs node))

(defn nodes
  [g]
  (:nodes g))

(defn arcs
  [g]
  (:arcs g))

(defn node 
  [g id]
  (get-in g [:nodes id]))

(defn add-node
  [g inputs outputs]
  (let [node (make-node g inputs outputs)]
    (-> g
      (assoc-in [:nodes (id node)] node)
      (claim-node-id))))

(defn add-arc
  [g source source-label target target-label]
  (update-in g [:arcs] conj {:source source :source-label source-label :target target :target-label target-label})
  )
(ns internal.graph
  (:require [clojure.set :as set]
            [dynamo.util :refer [removev]]
            [internal.graph.types :as gt]))

;; ----------------------------------------
;; Directed graph
;; ----------------------------------------
(defn empty-graph
  []
  {:nodes     {}
   :last-node 0
   :arcs      []
   :tx-id     0})

;;; Published API
(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))
(defn arcs        [g] (:arcs g))
(defn last-node   [g] (:last-node g))

(defn next-node   [g] (inc (last-node g)))
(defn claim-node  [g] (assoc-in g [:last-node] (next-node g)))

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

(defn replace-node
  [g n r]
  (assoc-in g [:nodes n] r))

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

;; ----------------------------------------
;; Labeled graph
;; ----------------------------------------
(defn add-labeled-node
  [g inputs outputs ^clojure.lang.Associative v]
  (add-node g (assoc v ::inputs inputs ::outputs outputs)))

(defn inputs [node]  (::inputs node))
(defn outputs [node] (::outputs node))

(defn targets [g node label]
  (for [a     (arcs-from g node)
        :when (= label (get-in a [:source-attributes :label]))]
    [(:target a) (get-in a [:target-attributes :label])]))

(defn sources [g node label]
  (for [a     (arcs-to g node)
        :when (= label (get-in a [:target-attributes :label]))]
    [(:source a) (get-in a [:source-attributes :label])]))

(defn source-labels [g target-node label source-node]
  (map second (filter #(= source-node (first %)) (sources g target-node label))))

(defn connect
  [g source source-label target target-label]
  (let [from (node g source)
        to   (node g target)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{source-label} (outputs from)) (str "No label " source-label " exists on node " from))
    (assert (some #{target-label} (inputs to))    (str "No label " target-label " exists on node " to))
    (add-arc g source {:label source-label} target {:label target-label})))

(defn disconnect
  [g source source-label target target-label]
  (remove-arc g source {:label source-label} target {:label target-label}))

(defn connected?
  [g source source-label target target-label]
  (not
   (empty?
    (filter
     #(and (= source-label (-> % :source-attributes :label))
           (= target-label (-> % :target-attributes :label)))
     (arcs-from-to g source target)))))

;; ----------------------------------------
;; Simplistic query engine
;; ----------------------------------------

;; enhancements:
;;  - additive matches for first clause, subtractive for rest.
;;  - use indexes for commonly-queries nodes
;;  - cache built queries in the graph itself.
;;  - hash join?
(defprotocol Clause
  (bind [this next graph candidates] "Match against a graph structure, passing the matching subset on to the continuation"))

(deftype ScanningClause [attr value]
  Clause
  (bind [this next graph candidates]
    (let [rfn (fn [matches candidate-id]
                (let [node-value (node graph candidate-id)]
                  (if (= value (get node-value attr))
                    (conj matches candidate-id)
                    matches)))]
      #(next graph (reduce rfn #{} candidates)))))


;;   "given the name of the protocol p as a symbol, "
(deftype ProtocolClause [p]
  Clause
  (bind [this next graph candidates]
    (let [rfn (fn [matches candidate-id]
                (let [node-value (node graph candidate-id)]
                  (if (satisfies? p node-value)
                    (conj matches candidate-id)
                    matches)))]
      #(next graph (reduce rfn #{} candidates)))))


(deftype NeighborsClause [dir-fn label]
  Clause
  (bind [this next graph candidates]
    (let [rfn (fn [matches candidate-id]
                (->> (dir-fn graph candidate-id label)
                  (map first)
                  (into matches)))]
      #(next graph (reduce rfn #{} candidates)))))

(defn- bomb
  [& info]
  (throw (ex-info (apply str info) {})))

(defn- make-protocol-clause
  [clause]
  (let [prot (second clause)]
    (if-let [prot (if (:on-interface prot) prot (var-get (resolve (second clause))))]
      (ProtocolClause. prot)
      (bomb "Cannot resolve " (second clause)))))

(defn- make-neighbors-clause
  [dir-fn label]
  (NeighborsClause. dir-fn label))

(defn- clause-instance
  [clause]
  (cond
    (vector? clause)  (ScanningClause. (first clause) (second clause))
    (list? clause)    (let [directive (first clause)]
                        (cond
                         (= directive 'protocol) (make-protocol-clause clause)
                         (= directive 'input)    (make-neighbors-clause sources (second clause))
                         (= directive 'output)   (make-neighbors-clause targets (second clause))
                         :else                   (bomb "Unrecognized query function: " clause)))
    :else             (bomb "Unrecognized clause: " clause)))

(defn- add-clause
  [ls clause]
  (fn [graph candidates]
    (bind (clause-instance clause) ls graph candidates)))

(defn- tail
  [graph candidates]
  candidates)

;; ProtocolClause - (protocol protocol-symbol)
;; ScanningClause - [:attr value]
;; Both (any order):
;; [(protocol symbol) [:attr value]]

(defn query
  [g clauses]
  (let [qfn (reduce add-clause tail (reverse clauses))]
    (trampoline qfn g (into #{} (node-ids g)))))

;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------
(def maximum-graph-coloring-recursion 100)

(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- marked-outputs
  [basis target-node target-label]
  (get (gt/input-dependencies (gt/node-by-id basis target-node)) target-label))

(defn- marked-downstream-nodes
  [basis node-id output-label]
  (for [[target-node target-input] (gt/targets basis node-id output-label)
        affected-outputs           (marked-outputs basis target-node target-input)]
    [target-node affected-outputs]))

;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(definline nref->graph  [gs node-id] `(get ~gs (gt/nref->gid ~node-id)))

(declare multigraph-basis)

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-id
    [_ node-id]
    (node (nref->graph graphs node-id) (gt/nref->nid node-id)))
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))
  (sources
    [_ node-id label]
    (sources (nref->graph graphs node-id) (gt/nref->nid node-id) label))
  (targets
    [_ node-id label]
    (targets (nref->graph graphs node-id) (gt/nref->nid node-id) label))
  (add-node
    [this tempid value]
    (let [gid     (gt/nref->gid tempid)
          graph   (get graphs gid)
          nid     (inc (last-node graph))
          node-id (gt/make-nref gid nid)
          node    (assoc value :_id node-id)
          graph   (add-labeled-node graph (gt/inputs node) (gt/outputs node) node)]
      [(update-in this [:graphs] assoc gid graph)
       node-id
       node]))
  (delete-node
    [this node-id]
    (let [node (gt/node-by-id this node-id)
          gid  (gt/nref->gid node-id)
          nid  (gt/nref->nid node-id)
          graph (remove-node (nref->graph graphs node-id) nid)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (replace-node
    [this node-id new-node]
    (let [gid   (gt/nref->gid node-id)
          nid   (gt/nref->nid node-id)
          graph (replace-node (nref->graph graphs node-id) nid (assoc new-node :_id node-id))
          node  (node graph nid)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (update-property
    [this node-id property f args]
    (let [gid   (gt/nref->gid node-id)
          nid   (gt/nref->nid node-id)
          graph (apply transform-node (nref->graph graphs node-id) nid update-in [property] f args)
          node  (node graph nid)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/nref->gid src-id)
          src-nid   (gt/nref->nid src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/nref->gid tgt-id)
          tgt-nid   (gt/nref->nid tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (update-in this [:graphs] assoc
                 tgt-gid (if (= src-gid tgt-gid)
                           tgt-graph
                           (connect tgt-graph src-nid src-label tgt-nid tgt-label))
                 src-gid (connect src-graph src-nid src-label tgt-nid tgt-label))))
  (disconnect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/nref->gid src-id)
          src-nid   (gt/nref->nid src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/nref->gid tgt-id)
          tgt-nid   (gt/nref->nid tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (update-in this [:graphs] assoc
                 tgt-gid (if (= src-gid tgt-gid)
                           tgt-graph
                           (disconnect tgt-graph src-nid src-label tgt-nid tgt-label))
                 src-gid (disconnect src-graph src-nid src-label tgt-nid tgt-label))))
  (connected?
    [this src-id src-label tgt-id tgt-label]
    (let [src-nid   (gt/nref->nid src-id)
          src-graph (nref->graph graphs src-id)
          tgt-nid   (gt/nref->nid tgt-id)
          tgt-graph (nref->graph graphs tgt-id)]
      (or (connected? src-graph src-nid src-label tgt-nid tgt-label)
          (connected? tgt-graph src-nid src-label tgt-nid tgt-label))))
  (query
    [this clauses]
    (let [fan-out (map #(query % clauses) (vals graphs))
          uniq    (distinct (apply concat fan-out))]
      (map #(gt/node-by-id this %) uniq))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

;; ---------------------------------------------------------------------------
;; API
;; ---------------------------------------------------------------------------
(defn trace-dependencies
  "Follow arcs through the graph, from outputs to the inputs connected
  to them, and from those inputs to the downstream outputs that use
  them, and so on. Continue following links until all reachable
  outputs are found.

  Returns a collection of [node-id output-label] pairs."
  ([basis to-be-marked]
   (trace-dependencies basis #{} to-be-marked maximum-graph-coloring-recursion))
  ([basis already-marked to-be-marked iterations-remaining]
   (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
   (if (empty? to-be-marked)
     already-marked
     (let [next-wave (mapcat #(apply marked-downstream-nodes basis %)  to-be-marked)]
       (recur basis (into already-marked to-be-marked) next-wave (dec iterations-remaining))))))

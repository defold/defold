(ns internal.graph
  (:require [dynamo.util :refer [removev]]
            [internal.graph.types :as gt]))

(deftype ArcBase [source target sourceLabel targetLabel]
  gt/Arc
  (head [_] [source sourceLabel])
  (tail [_] [target targetLabel])

  Object
  (toString [_]
    (str "[[" source sourceLabel "] -> [" target targetLabel "]]"))
  (equals [this that]
    (and (satisfies? gt/Arc that)
         (= source      (.source that))
         (= target      (.target that))
         (= sourceLabel (.sourceLabel that))
         (= targetLabel (.targetLabel that))))
  (hashCode [this]
    (+ (.hashCode source)      (mod (* 13 (.hashCode target)) 7)
       (.hashCode sourceLabel) (mod (* 19 (.hashCode targetLabel)) 23))))

(definline ^:private arc
  [source target source-label target-label]
  `(ArcBase. ~source ~target ~source-label ~target-label))

(defn empty-graph
  []
  {:nodes   {}
   :last-id 0
   :sarcs   []
   :tarcs   []
   :tx-id   0})

(defn- remove-arc
  [g key arc]
  (assoc g key (removev #(= arc %) (get g key))))

(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))

(defn- last-id     [g] (:last-id g))
(defn- next-id     [g] (inc (last-id g)))

(defn claim-id
  [g]
  (let [id (next-id g)]
    [(assoc-in g [:last-id] (next-id g)) id]))

(defn node        [g id]   (get-in g [:nodes id]))
(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(defn remove-node
  [g n]
  (-> g
      (update :nodes dissoc n)
      (update :sarcs (fn [s] (removev #(or (= n (.source %)) (= n (.target %))) s)))
      (update :tarcs (fn [s] (removev #(or (= n (.source %)) (= n (.target %))) s)))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn replace-node
  [g n r]
  (assoc-in g [:nodes n] r))

(defn targets
  [g node label]
  (for [arc (:sarcs g)
        :when (= [node label] (gt/head arc))]
    (gt/tail arc)))

(defn connect-source
  [g source source-label target target-label]
  (let [from (node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{source-label} (gt/outputs from)) (str "No label " source-label " exists on node " from))
    (update g :sarcs conj (arc source target source-label target-label))))

(defn source-connected?
  [g source source-label target target-label]
  (some #{[target target-label]} (targets g source source-label)))

(defn disconnect-source
  [g source source-label target target-label]
  (remove-arc g :sarcs (arc source target source-label target-label)))

(defn sources
  [g node label]
  (for [arc   (:tarcs g)
        :when (= [node label] (gt/tail arc))]
    (gt/head arc)))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{target-label} (gt/inputs to))    (str "No label " target-label " exists on node " to))
    (update g :tarcs conj (arc source target source-label target-label))))

(defn target-connected?
  [g source source-label target target-label]
  (some #{[source source-label]} (sources g target target-label)))

(defn disconnect-target
  [g source source-label target target-label]
  (remove-arc g :tarcs (arc source target source-label target-label)))


(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

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
                (->> label
                     (dir-fn graph candidate-id)
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

(definline nref->graph  [gs node-id] `(get ~gs (gt/nref->gid ~node-id)))

;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(declare multigraph-basis)

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-id
    [_ node-id]
    (node (nref->graph graphs node-id) node-id))
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))
  (sources
    [this node-id label]
    (filter #(gt/node-by-id this (first %)) (sources (nref->graph graphs node-id) node-id label)))
  (targets
    [this node-id label]
    (filter #(gt/node-by-id this (first %)) (targets (nref->graph graphs node-id) node-id label)))
  (add-node
    [this tempid value]
    (let [gid         (gt/nref->gid tempid)
          [graph nid] (claim-id (get graphs gid))
          node-id     (gt/make-nref gid nid)
          node        (assoc value :_id node-id)
          graph       (add-node graph node-id node)]
      [(update-in this [:graphs] assoc gid graph)
       node-id
       node]))
  (delete-node
    [this node-id]
    (let [node (gt/node-by-id this node-id)
          gid  (gt/nref->gid node-id)
          graph (remove-node (nref->graph graphs node-id) node-id)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (replace-node
    [this node-id new-node]
    (let [gid   (gt/nref->gid node-id)
          graph (replace-node (nref->graph graphs node-id) node-id (assoc new-node :_id node-id))
          node  (node graph node-id)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (update-property
    [this node-id property f args]
    (let [gid   (gt/nref->gid node-id)
          graph (apply transform-node (nref->graph graphs node-id) node-id update-in [property] f args)
          node  (node graph node-id)]
      [(update-in this [:graphs] assoc gid graph)
       node]))
  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/nref->gid src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/nref->gid tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (if (= src-gid tgt-gid)
        (update this :graphs assoc src-gid
                (-> src-graph
                    (connect-target src-id src-label tgt-id tgt-label)
                    (connect-source src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                tgt-gid (connect-target tgt-graph src-id src-label tgt-id tgt-label)
                src-gid (connect-source src-graph src-id src-label tgt-id tgt-label)))))
  (disconnect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/nref->gid src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/nref->gid tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (if (= src-gid tgt-gid)
        (update this :graphs assoc src-gid
                (-> src-graph
                    (disconnect-target src-id src-label tgt-id tgt-label)
                    (disconnect-source src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                tgt-gid (disconnect-target tgt-graph src-id src-label tgt-id tgt-label)
                src-gid (disconnect-source src-graph src-id src-label tgt-id tgt-label)))))
  (connected?
    [this src-id src-label tgt-id tgt-label]
    (let [src-graph (nref->graph graphs src-id)
          tgt-graph (nref->graph graphs tgt-id)]
      (or (source-connected? src-graph src-id src-label tgt-id tgt-label)
          (target-connected? tgt-graph src-id src-label tgt-id tgt-label))))

  (dependencies
    [this to-be-marked]
    (loop [already-marked       #{}
           to-be-marked         to-be-marked
           iterations-remaining maximum-graph-coloring-recursion]
      (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
      (if (empty? to-be-marked)
        already-marked
        (let [next-wave (mapcat #(apply marked-downstream-nodes this %)  to-be-marked)]
          (recur (into already-marked to-be-marked) next-wave (dec iterations-remaining))))))

  (query
    [this clauses]
    (let [fan-out (map #(query % clauses) (vals graphs))
          uniq    (distinct (apply concat fan-out))]
      (map #(gt/node-by-id this %) uniq))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

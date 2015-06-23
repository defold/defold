(ns internal.graph
  (:require [clojure.set :as set]
            [dynamo.util :refer [removev map-vals stackify]]
            [internal.graph.types :as gt]
            [schema.core :as s]))

(set! *warn-on-reflection* true)

(deftype ArcBase [source target sourceLabel targetLabel]
  gt/Arc
  (head [_] [source sourceLabel])
  (tail [_] [target targetLabel])

  Object
  (toString [_]
    (str "[[" source sourceLabel "] -> [" target targetLabel "]]"))
  (equals [this that]
    (and (satisfies? gt/Arc that)
         (let [^ArcBase that that ]
           (= source      (.source that))
           (= target      (.target that))
           (= sourceLabel (.sourceLabel that))
           (= targetLabel (.targetLabel that)))))
  (hashCode [this]
    (+ (.hashCode source)      (mod (* 13 (.hashCode target)) 7)
       (.hashCode sourceLabel) (mod (* 19 (.hashCode targetLabel)) 23))))

(definline ^:private arc
  [source target source-label target-label]
  `(ArcBase. ~source ~target ~source-label ~target-label))

(defn- conjv
  [coll x]
  (conj (or coll []) x))

(defn- rebuild-sarcs
  [basis graph-state]
  (let [gid      (:_gid graph-state)
        all-other-graphs (vals (assoc (:graphs basis) gid graph-state))
        all-arcs (flatten (mapcat (comp vals :tarcs) all-other-graphs))
        all-arcs-filtered (filter (fn [^ArcBase arc] (= (gt/node-id->graph-id (.source arc)) gid)) all-arcs)]
    (reduce
     (fn [sarcs arc] (update sarcs (.source ^ArcBase arc) conjv arc))
     {}
     all-arcs-filtered)))

(defn empty-graph
  []
  {:nodes      {}
   :sarcs      {}
   :successors {}
   :tarcs      {}
   :tx-id      0})

(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))

(defn node        [g id]   (get-in g [:nodes id]))
(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(defn remove-node
  [g n]
  (-> g
      (update :nodes dissoc n)
      (update :sarcs dissoc n)
      (update :sarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.target arc))) arcs)) %))
      (update :tarcs dissoc n)
      (update :tarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.source arc))) arcs)) %))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn replace-node
  [g n r]
  (assoc-in g [:nodes n] r))

(defn connect-source
  [g source source-label target target-label]
  (let [from (node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:sarcs source] conjv (arc source target source-label target-label))))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{target-label} (-> to gt/node-type gt/input-labels)) (str "No label " target-label " exists on node " to))
    (update-in g [:tarcs target] conjv (arc source target source-label target-label))))

(defn source-connected?
  [g source source-label target target-label]
  (not
   (empty?
    (filter (fn [^ArcBase arc]
              (and (= source-label (.sourceLabel arc))
                   (= target-label (.targetLabel arc))
                   (= target       (.target arc))))
            (get-in g [:sarcs source])))))

(defn disconnect-source
  [g source source-label target target-label]
  (update-in g [:sarcs source]
          (fn [arcs]
            (removev
             (fn [^ArcBase arc]
               (and (= source       (.source arc))
                    (= target       (.target arc))
                    (= source-label (.sourceLabel arc))
                    (= target-label (.targetLabel arc))))
             arcs))))

(defn disconnect-target
  [g source source-label target target-label]
  (update-in g [:tarcs target]
          (fn [arcs]
            (removev
             (fn [^ArcBase arc]
               (and (= source       (.source arc))
                    (= target       (.target arc))
                    (= source-label (.sourceLabel arc))
                    (= target-label (.targetLabel arc))))
             arcs))))

(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

(definline node-id->graph  [gs node-id] `(get ~gs (gt/node-id->graph-id ~node-id)))

(defn node-by-id-at
  [basis node-id]
  (node (node-id->graph (:graphs basis) node-id) node-id))

;; ---------------------------------------------------------------------------
;; Type checking
;; ---------------------------------------------------------------------------

(defn- check-single-type
  [out in]
  (or
   (= s/Any in)
   (= out in)
   (and (class? in) (class? out) (.isAssignableFrom ^Class in out))))

(defn type-compatible?
  [output-schema input-schema]
  (let [out-t-pl? (coll? output-schema)
        in-t-pl?  (coll? input-schema)]
    (or
     (= s/Any input-schema)
     (and out-t-pl? (= [s/Any] input-schema))
     (and (= out-t-pl? in-t-pl? true) (check-single-type (first output-schema) (first input-schema)))
     (and (= out-t-pl? in-t-pl? false) (check-single-type output-schema input-schema)))))

;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(declare multigraph-basis)

(defn- assert-type-compatible
  [basis src-id src-label tgt-id tgt-label]
  (let [output-type   (gt/node-type (node-by-id-at basis src-id))
        output-schema (gt/output-type output-type src-label)
        input-type    (gt/node-type (node-by-id-at basis tgt-id))
        input-schema  (gt/input-type input-type tgt-label)]
    (assert output-schema
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an output, input, or property named %s"
                    src-id (:name output-type) src-label
                    tgt-id (:name input-type) tgt-label
                    (:name output-type) src-label))
    (assert input-schema
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an input named %s"
                    src-id (:name output-type) src-label
                    tgt-id (:name input-type) tgt-label
                    (:name input-type) tgt-label))
    (assert (type-compatible? output-schema input-schema)
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s and %s do not have compatible types."
                    src-id (:name output-type) src-label
                    tgt-id (:name input-type) tgt-label
                    output-schema input-schema))))


;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------
(defn index-successors
  [basis [node-id output-label]]
  (let [target-inputs (gt/targets basis node-id output-label)]
    (apply set/union
           (mapv (fn [[node-id input-label]]
                  (let [set-of-output-labels (-> (node-by-id-at basis node-id)
                                                 gt/node-type
                                                 gt/input-dependencies
                                                 (get input-label))
                        node-label-pairs (mapv #(vector node-id %) set-of-output-labels)]
                    node-label-pairs))
                target-inputs))))

(defn- successors
  [basis [node-id output-label]]
  (get-in basis [:graphs (gt/node-id->graph-id node-id) :successors [node-id output-label]] #{}))

(defn pre-traverse
  "Traverses a graph depth-first preorder from start, successors being
  a function that returns direct successors for the node. Returns a
  lazy seq of nodes."
  [basis start succ & {:keys [seen] :or {seen #{}}}]
  (loop [stack  start
         seen   seen
         result (transient [])]
    (if-let [nxt (peek stack)]
      (if (contains? seen nxt)
        (recur (pop stack) seen result)
        (let [seen (conj seen nxt)
              nbrs (remove seen (succ basis nxt))]
          (recur (into (pop stack) nbrs) seen (conj! result nxt))))
      (persistent! result))))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (arcs-by-tail
    [this node-id]
    (get-in (node-id->graph graphs node-id) [:tarcs node-id]))

  (arcs-by-head
    [this node-id]
    (get-in (node-id->graph graphs node-id) [:sarcs node-id]))

  (sources
    [this node-id]
    (map gt/head
         (filter (fn [^ArcBase arc]
                   (node-by-id-at this (.source arc)))
                 (gt/arcs-by-tail this node-id))))

  (sources
    [this node-id label]
    (map gt/head
         (filter (fn [^ArcBase arc]
                   (and (= label (.targetLabel arc))
                        (node-by-id-at this (.source arc))))
                 (gt/arcs-by-tail this node-id))))

  (targets
    [this node-id]
    (map gt/tail
         (filter (fn [^ArcBase arc]
                   (node-by-id-at this (.target arc)))
                 (gt/arcs-by-head this node-id))))

  (targets
    [this node-id label]
    (map gt/tail
         (filter (fn [^ArcBase arc]
                   (and (= label (.sourceLabel arc))
                        (node-by-id-at this (.target arc))))
                 (gt/arcs-by-head this node-id))))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          gid     (gt/node-id->graph-id node-id)
          graph   (add-node (get graphs gid) node-id node)]
      [(update this :graphs assoc gid graph) node]))

  (delete-node
    [this node-id]
    (let [node  (node-by-id-at this node-id)
          gid   (gt/node-id->graph-id node-id)
          graph (remove-node (node-id->graph graphs node-id) node-id)]
      [(update this :graphs assoc gid graph) node]))

  (replace-node
    [this node-id new-node]
    (let [gid   (gt/node-id->graph-id node-id)
          graph (replace-node (node-id->graph graphs node-id) node-id (assoc new-node :_id node-id))
          node  (node graph node-id)]
      [(update this :graphs assoc gid graph) node]))

  (update-property
    [this node-id property f args]
    (let [gid   (gt/node-id->graph-id node-id)
          graph (apply transform-node (node-id->graph graphs node-id) node-id update-in [property] f args)
          node  (node graph node-id)]
      [(update this :graphs assoc gid graph) node]))

  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid       (gt/node-id->graph-id src-id)
          src-graph     (get graphs src-gid)
          tgt-gid       (gt/node-id->graph-id tgt-id)
          tgt-graph     (get graphs tgt-gid)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert-type-compatible this src-id src-label tgt-id tgt-label)
      (if (= src-gid tgt-gid)
        (update this :graphs assoc
                src-gid (-> src-graph
                            (connect-target src-id src-label tgt-id tgt-label)
                            (connect-source src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                src-gid (connect-source src-graph src-id src-label tgt-id tgt-label)
                tgt-gid (connect-target tgt-graph src-id src-label tgt-id tgt-label)))))

  (disconnect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/node-id->graph-id src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/node-id->graph-id tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (if (= src-gid tgt-gid)
        (update this :graphs assoc
                src-gid (-> src-graph
                            (disconnect-source src-id src-label tgt-id tgt-label)
                            (disconnect-target src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                src-gid (disconnect-source src-graph src-id src-label tgt-id tgt-label)
                tgt-gid (disconnect-target tgt-graph src-id src-label tgt-id tgt-label)))))

  (connected?
    [this src-id src-label tgt-id tgt-label]
    (let [src-graph (node-id->graph graphs src-id)]
      (source-connected? src-graph src-id src-label tgt-id tgt-label)))

  (dependencies
    [this to-be-marked]
    (pre-traverse this (stackify to-be-marked) successors)))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

(defn hydrate-after-undo
  [basis graph-state]
  (let [sarcs (rebuild-sarcs basis graph-state)
        gid (:_gid graph-state)
        hydrated-graph (assoc graph-state :sarcs sarcs)
        new-basis (assoc-in basis [:graphs gid] hydrated-graph)]
    new-basis))

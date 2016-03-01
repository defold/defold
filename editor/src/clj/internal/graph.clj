(ns internal.graph
  (:require [clojure.set :as set]
            [internal.util :refer [removev map-vals stackify project]]
            [internal.graph.types :as gt]
            [schema.core :as s])
  (:import [schema.core Maybe Either]))

(set! *warn-on-reflection* true)

(defrecord ArcBase [source target sourceLabel targetLabel]
  gt/Arc
  (head [_] [source sourceLabel])
  (tail [_] [target targetLabel]))

(definline ^:private arc
  [source target source-label target-label]
  `(ArcBase. ~source ~target ~source-label ~target-label))

(defn arc-endpoints-p [p arc]
  (and (p (.source ^ArcBase arc)) (p (.target ^ArcBase arc))))

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

(defn remove-override-node [g n original]
  (if original
    (let [override-id (gt/override-id (get-in g [:nodes n]))
          override (get-in g [:overrides override-id])]
      (cond-> g
        true (update-in [:node->overrides original] (partial remove #{n}))
        (= original (:root-id override)) (update :overrides dissoc override-id)))
    g))

(defn remove-node
  ([g n]
    (remove-node g n (some-> (get-in g [:nodes n]) gt/original)))
  ([g n original]
    (-> g
        (remove-override-node n original)
        (update :nodes dissoc n)
        (update :node->overrides dissoc n)
        (update :sarcs dissoc n)
        (update :sarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.target arc))) arcs)) %))
        (update :tarcs dissoc n)
        (update :tarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.source arc))) arcs)) %)))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn connect-source
  [g source source-label target target-label]
  (let [from (node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:sarcs source] conjv (arc source target source-label target-label))))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
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
     (and (= out-t-pl? in-t-pl? false) (check-single-type output-schema input-schema))
     (and (instance? Maybe input-schema) (type-compatible? output-schema (:schema input-schema)))
     (and (instance? Either input-schema) (some #(type-compatible? output-schema %) (:schemas input-schema))))))

;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(declare multigraph-basis)

(defn- assert-type-compatible
  [basis src-id src-label tgt-id tgt-label]
  (let [output-type   (gt/node-type (node-by-id-at basis src-id) basis)
        output-schema (gt/output-type output-type src-label)
        input-type    (gt/node-type (node-by-id-at basis tgt-id) basis)
        input-schema  (gt/input-type input-type tgt-label)]
    (assert output-schema
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an output or property named %s"
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
(defn overrides [basis node-id]
  (let [gid (gt/node-id->graph-id node-id)]
    (get-in basis [:graphs gid :node->overrides node-id])))

(defn index-successors
  [basis node-id-output-pair]
  (let [gather-node-dependencies-fn (fn [[id label]]
                                      (some-> (node-by-id-at basis id)
                                        (gt/node-type basis)
                                        gt/input-dependencies
                                        (get label)
                                        (->> (mapv (partial vector id)))))
        same-node-dep-pairs         (conj (gather-node-dependencies-fn node-id-output-pair) node-id-output-pair)
        override-pairs              (for [[id label] same-node-dep-pairs
                                          override (overrides basis id)]
                                      [override label])
        target-inputs               (mapcat (fn [[id label]] (gt/targets basis id label)) same-node-dep-pairs)]
    (set (concat
           same-node-dep-pairs
           override-pairs
           (mapcat gather-node-dependencies-fn target-inputs)))))

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

(defn- arcs->tuples
  [arcs]
  (project arcs [:source :sourceLabel :target :targetLabel]))

(def ^:private sources-of (comp arcs->tuples gt/arcs-by-tail))

(defn- successors-sources [pred basis node-id]
  (mapv first (filter pred (sources-of basis node-id))))

(defn pre-traverse-sources
  [basis start traverse?]
  (pre-traverse basis start (partial successors-sources traverse?)))

(defn- override-of [basis node-id override-id]
  (first (filter #(= override-id (gt/override-id (node-by-id-at basis %))) (overrides basis node-id))))

(defn- find-arcs [basis node-id graph-field group-fn primary secondary]
  (let [graphs (:graphs basis)
        arcs (get-in (node-id->graph graphs node-id) [graph-field node-id])]
    (if-let [original (gt/original-node basis node-id)]
      (let [node (node-by-id-at basis node-id)
            override-id (gt/override-id node)
            arcs (loop [arcs (group-fn arcs)
                        original original]
                   (if original
                     (recur (merge (->> (get-in (node-id->graph graphs original) [graph-field original])
                                     (map (fn [arc]
                                            (-> arc
                                              (assoc secondary (or (override-of basis (get arc secondary) override-id) (get arc secondary)))
                                              (assoc primary node-id))))
                                     group-fn)
                                   arcs)
                            (gt/original-node basis original))
                     arcs))]
        (mapcat second arcs))
      arcs)))

(defn- group-arcs-by-source [arcs]
  (group-by :sourceLabel arcs))

(defn- group-arcs-by-target [arcs]
  (group-by :targetLabel arcs))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (arcs-by-tail
    [this node-id]
    (find-arcs this node-id :tarcs group-arcs-by-target :target :source))

  (arcs-by-head
    [this node-id]
    (find-arcs this node-id :sarcs group-arcs-by-source :source :target))

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
          graph (remove-node (node-id->graph graphs node-id) node-id (gt/original node))]
      [(update this :graphs assoc gid graph) node]))

  (replace-node
    [this node-id new-node]
    (let [gid      (gt/node-id->graph-id node-id)
          new-node (assoc new-node :_node-id node-id)
          graph    (assoc-in (get graphs gid) [:nodes node-id] new-node)]
      [(update this :graphs assoc gid graph) new-node]))

  (override-node
    [this original-node-id override-node-id]
    (let [gid      (gt/node-id->graph-id override-node-id)]
      (update-in this [:graphs gid :node->overrides original-node-id] conj override-node-id)))

  (add-override
    [this override-id override]
    (let [gid (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] assoc override-id override)))

  (delete-override
    [this override-id]
    (let [gid (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] dissoc override-id)))

  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid       (gt/node-id->graph-id src-id)
          src-graph     (get graphs src-gid)
          tgt-gid       (gt/node-id->graph-id tgt-id)
          tgt-graph     (get graphs tgt-gid)
          tgt-node      (node tgt-graph tgt-id)
          tgt-type      (gt/node-type tgt-node this)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert-type-compatible this src-id src-label tgt-id tgt-label)
      (assert (some #{tgt-label} (-> tgt-type gt/input-labels)) (str "No label " tgt-label " exists on node " tgt-node))
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
    (pre-traverse this (stackify to-be-marked) successors))

  (original-node [this node-id]
    (when-let [node (node-by-id-at this node-id)]
      (gt/original node))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

(defn make-override [root-id traverse-fn]
  {:root-id root-id
   :traverse-fn traverse-fn})

(defn override-traverse-fn [basis override-id]
  (let [gid (gt/override-id->graph-id override-id)]
    (get-in basis [:graphs gid :overrides override-id :traverse-fn])))

(defn hydrate-after-undo
  [basis graph-state]
  (let [sarcs (rebuild-sarcs basis graph-state)
        gid (:_gid graph-state)
        hydrated-graph (assoc graph-state :sarcs sarcs)
        new-basis (assoc-in basis [:graphs gid] hydrated-graph)]
    new-basis))

(defn update-successors
  [basis changes]
  (if-let [change (first changes)]
    (let [new-successors (index-successors basis change)
          gid            (gt/node-id->graph-id (first change))]
      (recur (assoc-in basis [:graphs gid :successors change] new-successors) (next changes)))
    basis))

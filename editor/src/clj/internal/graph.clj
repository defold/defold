(ns internal.graph
  (:require [clojure.set :as set]
            [internal.util :refer [removev conjv map-vals stackify project]]
            [internal.graph.types :as gt]
            [schema.core :as s]
            [internal.util :as util]))

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
        true (update-in [:node->overrides original] (partial removev #{n}))
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

(defn override-by-id
  [basis override-id]
  (get-in basis [:graphs (gt/override-id->graph-id override-id) :overrides override-id]))


;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(declare multigraph-basis)

;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------
(defn overrides [basis node-id]
  (let [gid (gt/node-id->graph-id node-id)]
    (get-in basis [:graphs gid :node->overrides node-id])))

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
  (mapv first (filter (partial pred basis) (sources-of basis node-id))))

(defn pre-traverse-sources
  [basis start traverse?]
  (pre-traverse basis start (partial successors-sources traverse?)))

(defn- override-of [basis node-id or-path]
  (if-let [or-id (first or-path)]
    (if-let [or-node-id (first (filter #(= or-id (gt/override-id (node-by-id-at basis %))) (overrides basis node-id)))]
      (override-of basis or-node-id (next or-path))
      node-id)
    node-id))

(defn- group-arcs-by-source [arcs]
  (group-by :sourceLabel arcs))

(defn- group-arcs-by-target [arcs]
  (group-by :targetLabel arcs))

(defn- explicit-arcs-by-source
  ([basis src-id]
    (get-in basis [:graphs (gt/node-id->graph-id src-id) :sarcs src-id]))
  ([basis src-id src-label]
    (filterv (fn [^ArcBase a] (= src-label (.sourceLabel a))) (get-in basis [:graphs (gt/node-id->graph-id src-id) :sarcs src-id]))))

(defn- explicit-arcs-by-target
  ([basis tgt-id]
    (get-in basis [:graphs (gt/node-id->graph-id tgt-id) :tarcs tgt-id]))
  ([basis tgt-id tgt-label]
    (filterv (fn [^ArcBase a] (= tgt-label (.targetLabel a))) (get-in basis [:graphs (gt/node-id->graph-id tgt-id) :tarcs tgt-id]))))

(defn- implicit-overrides [basis node-id label arc-fn override-filter-fn]
  (let [overrides (filterv #(or (override-filter-fn (gt/override-id (node-by-id-at basis %)))
                                (empty? (arc-fn basis % label)))
                           (overrides basis node-id))]
    (mapcat #(cons % (implicit-overrides basis % label arc-fn override-filter-fn)) overrides)))

(defn- implicit-target-arcs [basis arcs]
  (mapcat (fn [^ArcBase a]
            (let [override-ids (into #{} (map #(gt/override-id (node-by-id-at basis %))
                                              (implicit-overrides basis (.source a) (.sourceLabel a) explicit-arcs-by-source (constantly true))))]
              (map (fn [nid] (assoc a :target nid))
                   (implicit-overrides basis (.target a) (.targetLabel a) explicit-arcs-by-target (complement override-ids)))))
          arcs))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (arcs-by-tail
    [this node-id]
    (let [graphs (:graphs this)
          arcs (explicit-arcs-by-target this node-id)]
      (if-let [original (gt/original-node this node-id)]
        (let [node (node-by-id-at this node-id)
              override-id (gt/override-id node)
              arcs (loop [arcs (group-arcs-by-target arcs)
                          original original
                          or-path [override-id]]
                     (if original
                       (recur (merge (->> (get-in (node-id->graph graphs original) [:tarcs original])
                                       (map (fn [arc]
                                              (-> arc
                                                (assoc :source (override-of this (get arc :source) or-path))
                                                (assoc :target node-id))))
                                       (filter :source)
                                       group-arcs-by-target)
                                     arcs)
                              (gt/original-node this original)
                              (if-let [or-id (gt/override-id (node-by-id-at this original))]
                                (cons or-id or-path)
                                or-path))
                      arcs))]
          (mapcat second arcs))
        arcs)))

  (arcs-by-head
    [this node-id]
    (let [explicit (explicit-arcs-by-source this node-id)
          implicit-targets (implicit-target-arcs this explicit)
          node (node-by-id-at this node-id)
          implicit (when-let [override-ids (and node #{(gt/override-id node)})]
                     (loop [original (gt/original node)
                            arcs (transient [])]
                       (if original
                         (let [explicit (explicit-arcs-by-source this original)
                               implicit (mapcat (fn [^ArcBase a]
                                                  (map (fn [nid] (assoc a :target nid))
                                                       (implicit-overrides this (.target a) (.targetLabel a) explicit-arcs-by-target override-ids)))
                                            explicit)]
                           (recur (gt/original (node-by-id-at this original)) (reduce conj! arcs implicit)))
                         (persistent! arcs))))]
      (concat explicit implicit-targets implicit)))

  (sources
    [this node-id]
    (mapv gt/head
         (filter (fn [^ArcBase arc]
                   (node-by-id-at this (.source arc)))
                 (gt/arcs-by-tail this node-id))))

  (sources
    [this node-id label]
    (mapv gt/head
         (filter (fn [^ArcBase arc]
                   (and (= label (.targetLabel arc))
                        (node-by-id-at this (.source arc))))
                 (gt/arcs-by-tail this node-id))))

  (targets
    [this node-id]
    (mapv gt/tail
         (filter (fn [^ArcBase arc]
                   (node-by-id-at this (.target arc)))
                 (gt/arcs-by-head this node-id))))

  (targets
    [this node-id label]
    (mapv gt/tail
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

  (replace-override
    [this override-id new-override]
    (let [gid      (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] assoc override-id new-override)))

  (override-node
    [this original-node-id override-node-id]
    (let [gid      (gt/node-id->graph-id override-node-id)]
      (update-in this [:graphs gid :node->overrides original-node-id] conjv override-node-id)))

  (override-node-clear [this original-id]
    (let [gid (gt/node-id->graph-id original-id)]
      (update-in this [:graphs gid :node->overrides] dissoc original-id)))

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
      (assert (some #{tgt-label} (-> tgt-type :input util/key-set)) (str "No label " tgt-label " exists on node " tgt-node))
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
    (let [src-graph (node-id->graph graphs src-id)
          targets (gt/targets this src-id src-label)]
      (some #{[tgt-id tgt-label]} targets)))

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

(defn- input-deps [basis node-id]
  (some-> (node-by-id-at basis node-id)
          (gt/node-type basis)
          :input-dependencies))

(defn update-successors
  [basis changes]
  (loop [basis basis
         changes (sort changes)]
    (if-let [change (first changes)]
      (let [input-deps (partial input-deps basis)
            node-id (first change)
            gid (gt/node-id->graph-id node-id)
            deps-by-label (input-deps node-id)
            out-arcs (group-by first (map (fn [^ArcBase a] [(.sourceLabel a) (gt/tail a)])
                                          (filter (fn [^ArcBase arc]
                                                    (node-by-id-at basis (.target arc)))
                                                  (gt/arcs-by-head basis node-id))))
            overrides (overrides basis node-id)
            ; Inner loop over labels while the node-id is constant
            [changes deps] (loop [changes changes
                                  all-deps {}]
                             (let [change (first changes)
                                   [next-node-id label] change]
                               (if (and next-node-id (= next-node-id node-id))
                                 (let [deps (mapv vector (repeat node-id) (get deps-by-label label))
                                       or-deps (for [[_ label] deps
                                                     override overrides]
                                                 [override label])
                                       target-deps (mapcat (fn [[_ [id label]]] (mapv vector (repeat id) (get (input-deps id) label))) (get out-arcs label))
                                       deps (set (concat deps or-deps target-deps))]
                                   (recur (next changes) (assoc all-deps change deps)))
                                 [changes all-deps])))
            successors (let [s (transient (get-in basis [:graphs gid :successors] {}))]
                         (persistent! (reduce conj! s deps)))]
        (recur (assoc-in basis [:graphs gid :successors] successors) changes))
      basis)))

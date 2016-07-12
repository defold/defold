(ns internal.graph
  (:require [clojure.set :as set]
            [internal.graph.types :as gt]
            [schema.core :as s]
            [internal.util :as util]
            [internal.node :as in]))

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
        all-graphs (vals (assoc (:graphs basis) gid graph-state))
        all-arcs (mapcat (fn [g] (mapcat (fn [[nid label-tarcs]] (mapcat second label-tarcs))
                                         (:tarcs g)))
                         all-graphs)
        all-arcs-filtered (filter (fn [^ArcBase arc] (= (gt/node-id->graph-id (.source arc)) gid)) all-arcs)]
    (reduce
      (fn [sarcs arc] (update-in sarcs [(.source ^ArcBase arc) (.sourceLabel ^ArcBase arc)] util/conjv arc))
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

(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(definline node-id->graph [basis node-id] `(get (:graphs ~basis) (gt/node-id->graph-id ~node-id)))
(definline graph->node [graph node-id] `(get (:nodes ~graph) ~node-id))

(defn remove-override-node [g n original]
  (if original
    (let [override-id (gt/override-id (get-in g [:nodes n]))
          override (get-in g [:overrides override-id])]
      (cond-> g
        true (update-in [:node->overrides original] (partial util/removev #{n}))
        (= original (:root-id override)) (update :overrides dissoc override-id)))
    g))

(defn remove-node
  ([g n]
    (remove-node g n (some-> (get-in g [:nodes n]) gt/original)))
  ([g n original]
    (let [sarcs (mapcat second (get-in g [:sarcs n]))
          tarcs (mapcat second (get-in g [:tarcs n]))]
      (-> g
        (remove-override-node n original)
        (update :nodes dissoc n)
        (update :node->overrides dissoc n)
        (update :sarcs dissoc n)
        (update :sarcs (fn [s] (reduce (fn [s ^ArcBase arc]
                                         (update-in s [(.source arc) (.sourceLabel arc)]
                                                    (fn [arcs] (util/removev (fn [^ArcBase arc] (= n (.target arc))) arcs))))
                                       s tarcs)))
        (update :tarcs dissoc n)
        (update :tarcs (fn [s] (reduce (fn [s ^ArcBase arc]
                                         (update-in s [(.target arc) (.targetLabel arc)]
                                                    (fn [arcs] (util/removev (fn [^ArcBase arc] (= n (.source arc))) arcs))))
                                       s sarcs)))))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn connect-source
  [g source source-label target target-label]
  (let [from (graph->node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:sarcs source source-label] util/conjv (arc source target source-label target-label))))

(defn connect-target
  [g source source-label target target-label]
  (let [to (graph->node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:tarcs target target-label] util/conjv (arc source target source-label target-label))))

(defn disconnect-source
  [g source source-label target target-label]
  (update-in g [:sarcs source source-label]
          (fn [arcs]
            (util/removev
             (fn [^ArcBase arc]
               (and (= source       (.source arc))
                    (= target       (.target arc))
                    (= source-label (.sourceLabel arc))
                    (= target-label (.targetLabel arc))))
             arcs))))

(defn disconnect-target
  [g source source-label target target-label]
  (update-in g [:tarcs target target-label]
          (fn [arcs]
            (util/removev
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

(defn- successors-fn [basis]
  (let [gid (atom -1)
        succ (atom nil)]
    (fn [basis output]
      (let [curr-gid (gt/node-id->graph-id (first output))
            succ (if (not= @gid curr-gid)
                   (do
                     (reset! gid curr-gid)
                     (reset! succ (get-in basis [:graphs curr-gid :successors] curr-gid)))
                   @succ)]
        (get succ output [])))))

(defn pre-traverse
  "Traverses a graph depth-first preorder from start, successors being
  a function that returns direct successors for the node. Returns a
  lazy seq of nodes."
  [basis start succ & {:keys [seen] :or {seen #{}}}]
  (loop [stack start
         next []
         seen seen
         result (transient [])]
    (if-let [nxt (first stack)]
      (if (contains? seen nxt)
        (recur (rest stack) next seen result)
        (let [seen (conj seen nxt)]
          (recur (succ basis nxt) (conj next (rest stack))
                 seen (conj! result nxt))))
      (if-let [next-stack (peek next)]
        (recur next-stack (pop next) seen result)
        (persistent! result)))))

(defn- arcs->tuples
  [arcs]
  (util/project arcs [:source :sourceLabel :target :targetLabel]))

(def ^:private sources-of (comp arcs->tuples gt/arcs-by-tail))

(defn- successors-sources [pred basis node-id]
  (mapv first (filter (partial pred basis) (sources-of basis node-id))))

(defn pre-traverse-sources
  [basis start traverse?]
  (pre-traverse basis start (partial successors-sources traverse?)))

(defn- overrides [graph node-id]
  (get (:node->overrides graph) node-id))

(defn get-overrides [basis node-id]
  (overrides (node-id->graph basis node-id) node-id))

(defn- override-of [graph node-id or-id]
  (some #(and (= or-id (gt/override-id (graph->node graph %))) %) (overrides graph node-id)))

(defn- closest-override-of [graph node-id or-path]
  (if-let [or-id (first or-path)]
    (if-let [or-node-id (override-of graph node-id or-id)]
      (closest-override-of graph or-node-id (next or-path))
      node-id)
    node-id))

(defn- group-arcs-by-source [arcs]
  (group-by :sourceLabel arcs))

(defn- group-arcs-by-target [arcs]
  (group-by :targetLabel arcs))

(defn- node-id->arcs [graph node-id arc-kw]
  (loop [all-arcs (-> (get graph arc-kw) (get node-id) vals)
         res []]
    (if-let [arcs (first all-arcs)]
      (recur (rest all-arcs) (reduce conj res arcs))
      res)))

(defn- explicit-arcs-by-source
  ([graph src-id]
    (node-id->arcs graph src-id :sarcs))
  ([graph src-id src-label]
    (-> (:sarcs graph)
      (get src-id)
      (get src-label))))

(defn- explicit-arcs-by-target
  ([graph tgt-id]
    (node-id->arcs graph tgt-id :tarcs))
  ([graph tgt-id tgt-label]
    (-> (:tarcs graph)
      (get tgt-id)
      (get tgt-label))))

(defn- implicit-overrides [basis node-id label arc-fn override-filter-fn]
  (let [graph (get (:graphs basis) (gt/node-id->graph-id node-id))]
    (loop [overrides (overrides graph node-id)
           res []]
      (if-let [override (first overrides)]
        (if (and (override-filter-fn (gt/override-id (gt/node-by-id-at basis override)))
                 (empty? (arc-fn graph override label)))
          (let [res (conj res override)
                res (reduce conj res (implicit-overrides basis override label arc-fn override-filter-fn))]
            res)
          (recur (rest overrides) res))
        res))))

(defn- implicit-target-arcs [basis arcs override-filter-fn]
  (loop [arcs arcs
         res []]
    (if-let [^ArcBase a (first arcs)]
      (->> (implicit-overrides basis (.target a) (.targetLabel a) explicit-arcs-by-target override-filter-fn)
        (reduce (fn [res nid] (conj res (assoc a :target nid))) res)
        (recur (rest arcs)))
      res)))

(defn- basis-arcs-by-tail [basis node-id label]
  (let [graph (node-id->graph basis node-id)
        arcs (explicit-arcs-by-target graph node-id label)]
    (if-let [original (and (empty? arcs) (gt/original-node basis node-id))]
      (let [node (gt/node-by-id-at basis node-id)
            or-path [(gt/override-id node)]]
        (->> (basis-arcs-by-tail basis original label)
          (mapv (fn [arc]
                  (let [src-id (:source arc)
                        src-graph (node-id->graph basis src-id)]
                    (assoc arc
                           :source (closest-override-of src-graph src-id or-path)
                           :target node-id))))))
      arcs)))

(defn- basis-arcs-by-head
  [basis graph node-id node label override-filter-fn]
    (let [arcs (explicit-arcs-by-source graph node-id label)]
      (if-let [original (and (empty? arcs) (gt/original-node basis node-id))]
        (let [or-id (gt/override-id node)
              arcs (loop [arcs (basis-arcs-by-head basis graph original (graph->node graph original) label nil)
                          res []]
                     (if-let [arc (first arcs)]
                       (let [tgt-id (:target arc)
                             tgt-graph (node-id->graph basis tgt-id)]
                         (if-let [new-target (override-of tgt-graph tgt-id or-id)]
                           (let [new-arc (assoc arc
                                                :source node-id
                                                :target new-target)]
                             (recur (rest arcs) (conj res new-arc)))
                           (recur (rest arcs) res)))
                       res))]
          arcs)
        (if override-filter-fn
          (into arcs (implicit-target-arcs basis arcs override-filter-fn))
          arcs))))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-id-at
      [this node-id]
    (graph->node (node-id->graph this node-id) node-id))

  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (arcs-by-tail
    [this node-id]
    (let [graph (node-id->graph this node-id)
          arcs (explicit-arcs-by-target graph node-id)]
      (if-let [original (gt/original-node this node-id)]
        (let [node (gt/node-by-id-at this node-id)
              override-id (gt/override-id node)
              arcs (loop [arcs (group-arcs-by-target arcs)
                          original original
                          or-path [override-id]]
                     (if original
                       (recur (merge (->> (mapcat second (get-in (node-id->graph this original) [:tarcs original]))
                                       (map (fn [arc]
                                              (let [src-id (:source arc)
                                                    src-graph (node-id->graph this src-id)]
                                                (-> arc
                                                  (assoc :source (closest-override-of src-graph src-id or-path))
                                                  (assoc :target node-id)))))
                                       (filter :source)
                                       group-arcs-by-target)
                                     arcs)
                              (gt/original-node this original)
                              (if-let [or-id (gt/override-id (gt/node-by-id-at this original))]
                                (cons or-id or-path)
                                or-path))
                      arcs))]
          (mapcat second arcs))
        arcs)))
  (arcs-by-tail [this node-id label]
    (basis-arcs-by-tail this node-id label))

  (arcs-by-head
    [this node-id]
    (let [arcs (transient [])
          graph (node-id->graph this node-id)
          explicit (explicit-arcs-by-source graph node-id)
          arcs (reduce conj! arcs explicit)
          override-filter-fn (->> (tree-seq (constantly true) (partial overrides graph) node-id)
                                     (map #(gt/override-id (graph->node graph %)))
                                     (into #{})
                                     complement)
          implicit-targets (implicit-target-arcs this explicit override-filter-fn)
          arcs (reduce conj! arcs implicit-targets)
          node (gt/node-by-id-at this node-id)
          arcs (if-let [override-ids (and node #{(gt/override-id node)})]
                 (loop [original (gt/original node)
                        arcs arcs]
                   (if original
                     (let [explicit (explicit-arcs-by-source graph original)
                           implicit (mapcat (fn [^ArcBase a]
                                              (map (fn [nid] (assoc a :target nid))
                                                   (implicit-overrides this (.target a) (.targetLabel a) explicit-arcs-by-target override-ids)))
                                        explicit)]
                       (recur (gt/original (gt/node-by-id-at this original)) (reduce conj! arcs implicit)))
                     arcs))
                 arcs)]
      (persistent! arcs)))

  (arcs-by-head
    [this node-id label]
    (let [graph (node-id->graph this node-id)
          override-filter-fn (->> (tree-seq (constantly true) (partial overrides graph) node-id)
                               (map #(gt/override-id (graph->node graph %)))
                               (into #{})
                               complement)]
      (basis-arcs-by-head this graph node-id (graph->node graph node-id) label override-filter-fn)))

  (sources
    [this node-id]
    (->> (gt/arcs-by-tail this node-id)
      (mapv gt/head)))

  (sources
    [this node-id label]
    (->> (gt/arcs-by-tail this node-id label)
      (mapv gt/head)))

  (targets
    [this node-id]
    (->> (gt/arcs-by-head this node-id)
      (mapv gt/tail)))

  (targets
    [this node-id label]
    (->> (gt/arcs-by-head this node-id label)
      (mapv gt/tail)))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          gid     (gt/node-id->graph-id node-id)
          graph   (add-node (get graphs gid) node-id node)]
      [(update this :graphs assoc gid graph) node]))

  (delete-node
    [this node-id]
    (let [node  (gt/node-by-id-at this node-id)
          gid   (gt/node-id->graph-id node-id)
          graph (remove-node (node-id->graph this node-id) node-id (gt/original node))]
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
      (update-in this [:graphs gid :node->overrides original-node-id] util/conjv override-node-id)))

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
          tgt-node      (graph->node tgt-graph tgt-id)
          tgt-type      (gt/node-type tgt-node this)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert (in/has-input? tgt-type tgt-label) (str "No label " tgt-label " exists on node " tgt-node))
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
    (let [src-graph (node-id->graph this src-id)
          targets (gt/targets this src-id src-label)]
      (some #{[tgt-id tgt-label]} targets)))

  (dependencies
    [this outputs]
    (pre-traverse this outputs (successors-fn this)))

  (original-node [this node-id]
    (when-let [node (gt/node-by-id-at this node-id)]
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
        new-basis (update basis :graphs assoc gid (assoc graph-state :sarcs sarcs))]
    new-basis))

(defn- input-deps [basis node-id]
  (or (some-> (gt/node-by-id-at basis node-id)
        (gt/node-type basis)
        in/input-dependencies) {}))

(defn- succ-output-successors [succ basis node-id labels]
  (let [gid (gt/node-id->graph-id node-id)
        graph (get (:graphs basis) gid)
        deps-by-label (input-deps basis node-id)
        override-filter-fn (->> (tree-seq (constantly true) (partial overrides graph) node-id)
                             (map #(gt/override-id (graph->node graph %)))
                             (into #{})
                             complement)
        overrides (overrides graph node-id)
        node (gt/node-by-id-at basis node-id)
        labels (or labels (-> node (gt/node-type basis) in/output-labels))]
    (loop [succ succ
           labels labels]
      (if-let [label (first labels)]
        (let [deps (transient #{})
              dep-labels (get deps-by-label label)
              deps (reduce conj! deps (map (partial vector node-id) dep-labels))
              deps (reduce conj! deps (for [label dep-labels
	                                           override overrides]
	                                       [override label]))
              deps (loop [arcs (basis-arcs-by-head basis graph node-id node label override-filter-fn)
                          res deps]
                     (if-let [^ArcBase arc (first arcs)]
                       (let [tgt-id (.target arc)
                             tgt-label (.targetLabel arc)
                             res (loop [labels (get (input-deps basis tgt-id) tgt-label)
                                        res res]
                                   (if-let [label (first labels)]
                                     (recur (rest labels) (conj! res [tgt-id label]))
                                     res))]
                         (recur (rest arcs) res))
                       res))]
          (recur (assoc succ [node-id label] (persistent! deps)) (rest labels)))
        succ))))

(defn update-successors
  [basis changes]
  (let [changes (vec changes)
        succ-map (->> changes
                   (map (comp gt/node-id->graph-id first))
                   (into #{})
                   (map (fn [gid] [gid (get-in basis [:graphs gid :successors] {})]))
                   (into {}))
        succ-map (loop [succ-map succ-map
                        changes changes]
                   (if-let [[node-id labels] (first changes)]
                     (if (gt/node-by-id-at basis node-id)
                       (let [gid (gt/node-id->graph-id node-id)
                             succ-map (update succ-map gid succ-output-successors basis node-id labels)]
                         (recur succ-map (rest changes)))
                       (recur succ-map (rest changes)))
                     succ-map))]
    (reduce (fn [basis [gid succ]] (assoc-in basis [:graphs gid :successors] succ)) basis succ-map)))

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

(defn- overrides [graph node-id]
  (get (:node->overrides graph) node-id))

;; This function only removes the node from the single graph in which it exists
;; It should only be used for testing purposes
(defn graph-remove-node
  ([g n]
    (graph-remove-node g n (some-> (get-in g [:nodes n]) gt/original)))
  ([g n original]
    (graph-remove-node g n original false))
  ([g n original original-deleted?]
    (if (contains? (:nodes g) n)
      (let [g (reduce (fn [g or-n] (graph-remove-node g or-n n true)) g (overrides g n))
            sarcs (mapcat second (get-in g [:sarcs n]))
            tarcs (mapcat second (get-in g [:tarcs n]))
            override-id (when original (gt/override-id (get-in g [:nodes n])))
            override (when original (get-in g [:overrides override-id]))]
        (-> g
          (cond->
            (not original-deleted?) (update-in [:node->overrides original] (partial util/removev #{n}))
            (and override (= original (:root-id override))) (update :overrides dissoc override-id))
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
                                  s sarcs)))))
      g)))

(defn- arc-cross-graph? [^ArcBase arc]
  (not= (gt/node-id->graph-id (.source arc)) (gt/node-id->graph-id (.target arc))))

(defn- arcs-remove-node [arcs node-id]
  (util/removev (fn [^ArcBase arc] (or (= node-id (.source arc))
                                     (= node-id (.target arc)))) arcs))

(defn basis-remove-node
  ([basis n]
    (basis-remove-node basis n (some-> (get-in basis [:graphs (gt/node-id->graph-id n) :nodes n]) gt/original)))
  ([basis n original]
    (basis-remove-node basis n original false))
  ([basis n original original-deleted?]
    (let [g (node-id->graph basis n)
          basis (reduce (fn [basis or-n] (basis-remove-node basis or-n n true)) basis (overrides g n))
          sarcs (mapcat second (get-in g [:sarcs n]))
          tarcs (mapcat second (get-in g [:tarcs n]))
          override-id (when original (gt/override-id (get-in g [:nodes n])))
          override (when original (get-in g [:overrides override-id]))
          ext-sarcs (filterv arc-cross-graph? sarcs)
          ext-tarcs (filterv arc-cross-graph? tarcs)
          basis (-> basis
                  (update-in [:graphs (gt/node-id->graph-id n)]
                    (fn [g]
                      (-> g
                        (cond->
                          (not original-deleted?) (update-in [:node->overrides original] (partial util/removev #{n}))
                          (and override (= original (:root-id override))) (update :overrides dissoc override-id))
                        (update :nodes dissoc n)
                        (update :node->overrides dissoc n)
                        (update :sarcs dissoc n)
                        (update :sarcs (fn [s] (reduce (fn [s ^ArcBase arc]
                                                         (update-in s [(.source arc) (.sourceLabel arc)] arcs-remove-node n))
                                                 s tarcs)))
                        (update :tarcs dissoc n)
                        (update :tarcs (fn [s] (reduce (fn [s ^ArcBase arc]
                                                         (update-in s [(.target arc) (.targetLabel arc)] arcs-remove-node n))
                                                 s sarcs)))))))
          basis (reduce (fn [basis ^ArcBase arc]
                          (let [nid (.target arc)]
                            (update-in basis [:graphs (gt/node-id->graph-id nid) :tarcs nid (.targetLabel arc)] arcs-remove-node n)))
                  basis ext-sarcs)
          basis (reduce (fn [basis ^ArcBase arc]
                          (let [nid (.source arc)]
                            (update-in basis [:graphs (gt/node-id->graph-id nid) :tarcs nid (.sourceLabel arc)] arcs-remove-node n)))
                  basis ext-tarcs)]
      basis)))

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
  (cond-> g
    (graph->node g source)
    (update-in  [:sarcs source source-label]
                (fn [arcs]
                  (util/removev
                    (fn [^ArcBase arc]
                      (and (= source       (.source arc))
                           (= target       (.target arc))
                           (= source-label (.sourceLabel arc))
                           (= target-label (.targetLabel arc))))
                    arcs)))))

(defn disconnect-target
  [g source source-label target target-label]
  (cond-> g
    (graph->node g target)
    (update-in [:tarcs target target-label]
               (fn [arcs]
                 (util/removev
                   (fn [^ArcBase arc]
                     (and (= source       (.source arc))
                          (= target       (.target arc))
                          (= source-label (.sourceLabel arc))
                          (= target-label (.targetLabel arc))))
                   arcs)))))

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

(defn get-overrides [basis node-id]
  (overrides (node-id->graph basis node-id) node-id))

(defn override-original [basis node-id]
  (when-let [n (gt/node-by-id-at basis node-id)]
    (gt/original n)))

(defn override-originals [basis node-id]
  (into '() (take-while some? (iterate (partial override-original basis) node-id))))

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

(defn- graph-explicit-arcs-by-source
  ([graph src-id]
    (node-id->arcs graph src-id :sarcs))
  ([graph src-id src-label]
    (-> (:sarcs graph)
      (get src-id)
      (get src-label))))

(defn explicit-arcs-by-source
  ([basis src-id]
    (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id))
  ([basis src-id src-label]
    (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id src-label)))

(defn- graph-explicit-arcs-by-target
  ([graph tgt-id]
    (node-id->arcs graph tgt-id :tarcs))
  ([graph tgt-id tgt-label]
    (-> (:tarcs graph)
      (get tgt-id)
      (get tgt-label))))

(defn explicit-arcs-by-target
  ([basis tgt-id]
    (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id))
  ([basis tgt-id tgt-label]
    (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id tgt-label)))

(defn explicit-sources
  ([basis tgt-id]
    (->> (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id)
      (mapv gt/head)))
  ([basis tgt-id tgt-label]
    (->> (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id tgt-label)
      (mapv gt/head))))

(defn explicit-targets
  ([basis src-id]
    (->> (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id)
      (mapv gt/tail)))
  ([basis src-id tgt-label]
    (->> (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id tgt-label)
      (mapv gt/tail))))

(defn- implicit-overrides [basis node-id label arc-fn override-filter-fn]
  (let [graph (get (:graphs basis) (gt/node-id->graph-id node-id))]
    (loop [overrides (overrides graph node-id)
           res []]
      (if-let [override (first overrides)]
        (if (and (override-filter-fn (gt/override-id (gt/node-by-id-at basis override)))
                 (empty? (arc-fn graph override label)))
          (let [res (conj res override)
                res (reduce conj res (implicit-overrides basis override label arc-fn override-filter-fn))]
            (recur (rest overrides) res))
          (recur (rest overrides) res))
        res))))

(defn- implicit-target-arcs [basis arcs override-filter-fn]
  (loop [arcs arcs
         res []]
    (if-let [^ArcBase a (first arcs)]
      (->> (implicit-overrides basis (.target a) (.targetLabel a) graph-explicit-arcs-by-target override-filter-fn)
        (reduce (fn [res nid] (conj res (assoc a :target nid))) res)
        (recur (rest arcs)))
      res)))

(defn- basis-arcs-by-tail [basis node-id label]
  (let [graph (node-id->graph basis node-id)
        arcs (graph-explicit-arcs-by-target graph node-id label)]
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
    (let [arcs (graph-explicit-arcs-by-source graph node-id label)]
      (if-let [original (and (empty? arcs) (gt/original node))]
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

(def ^:private none-seen (constantly false))
(def ^:private set-or-union (fn [s1 s2] (if s1 (set/union s1 s2) s2)))
(def ^:private merge-with-union (let [red-f (fn [m [k v]] (update m k set-or-union v))]
                                  (completing (fn [m1 m2] (reduce red-f m1 m2)))))

(defn- basis-dependencies [basis outputs-by-node-ids]
  (let [gid->succ (into {} (map (fn [[gid graph]] [gid (:successors graph)])) (:graphs basis))
        nid->succ (fn [nid]
                    (some-> nid
                      gt/node-id->graph-id
                      gid->succ
                      (get nid)))]
    (loop [;; 'todo' is the running stack (actually a map) of entries to traverse
           ;; it's expensive to iterate a map, so start by turning it into a seq
           todo (seq outputs-by-node-ids)
           ;; collect next batch of entries in a map, to coalesce common node ids
           next-todo {}
           ;; final transitive closure of entries found, as a map
           result {}]
      (if-let [[node-id outputs] (first todo)]
        (let [seen? (get result node-id)]
          ;; termination condition is when we have seen *every* output already
          (if (and seen? (every? seen? outputs))
            ;; completely remove the node-id from todo as we have seen *every* output
            (recur (next todo) next-todo result)
            ;; does the node-id have any successors at all?
            (if-let [label->succ (nid->succ node-id)]
              ;; ignore the outputs we have already seen
              (let [outputs (if seen? (set/difference outputs seen?) outputs)
                    ;; Add every successor to the stack for later processing
                    next-todo (transduce (map #(label->succ %)) merge-with-union next-todo outputs)
                    ;; And include the unseen output labels to the result
                    result (update result node-id set-or-union outputs)]
                (recur (next todo) next-todo result))
              ;; There were no successors, recur without that node-id
              (recur (next todo) next-todo result))))
        ;; check if there is a next batch of entries to process
        (if-let [todo (seq next-todo)]
          (recur todo {} result)
          result)))))

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
          arcs (graph-explicit-arcs-by-target graph node-id)]
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
          explicit (graph-explicit-arcs-by-source graph node-id)
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
                     (let [explicit (graph-explicit-arcs-by-source graph original)
                           implicit (mapcat (fn [^ArcBase a]
                                              (map (fn [nid] (assoc a :target nid))
                                                   (implicit-overrides this (.target a) (.targetLabel a) graph-explicit-arcs-by-target override-ids)))
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
    (basis-remove-node this node-id (-> this (gt/node-by-id-at node-id) gt/original)))

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
    [this outputs-by-node-ids]
    (basis-dependencies this outputs-by-node-ids))

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

(defn- sarcs->arcs [sarcs]
  (into #{}
        (comp (map vals)
              cat
              cat)
        (vals sarcs)))

(defn hydrate-after-undo
  [basis graph-state]
  (let [old-sarcs (get graph-state :sarcs)
        sarcs (rebuild-sarcs basis graph-state)
        sarcs-diff (clojure.set/difference (sarcs->arcs sarcs) (sarcs->arcs old-sarcs))
        gid (:_gid graph-state)
        new-basis (update basis :graphs assoc gid (assoc graph-state :sarcs sarcs))]
    {:basis new-basis
     :outputs-to-refresh (mapv (juxt :source :sourceLabel) sarcs-diff)}))

(defn- input-deps [basis node-id]
  (some-> (gt/node-by-id-at basis node-id)
    (gt/node-type basis)
    in/input-dependencies))

;; The purpose of this fn is to build a data structure that reflects which set of node-id + outputs that can be reached from the incoming changes (map of node-id + outputs)
;; For a specific node-id-a + output-x, add:
;;   the internal input-dependencies, i.e. outputs consuming the given output
;;   the closest override-nodes, i.e. override-node-a + output-x, as they can be potential dependents
;;   all connected nodes, where node-id-a + output-x => [[node-id-b + input-y] ...] => [[node-id-b + output+z] ...]
(defn- update-graph-successors [succs basis gid graph changes]
  (let [node-id->overrides (or (:node->overrides graph) (constantly nil))
        ;; Transducer to collect override-id's
        override-id-xf (keep #(some->> %
                                (graph->node graph)
                                (gt/override-id)))]
    (reduce (fn [succs [node-id labels]]
              (if-let [node (gt/node-by-id-at basis node-id)]
                (let [;; Support data and functions
                      node-type (gt/node-type node basis)
                      deps-by-label (or (in/input-dependencies node-type) (constantly nil))
                      node-and-overrides (tree-seq (constantly true) node-id->overrides node-id)
                      override-filter-fn (complement (into #{} override-id-xf node-and-overrides))
                      overrides (node-id->overrides node-id)
                      labels (or labels (in/output-labels node-type))
                      repeat-node-id (repeat node-id)]
                  (update succs node-id
                    (fn [succs]
                      (let [succs (or succs {})]
                        (reduce (fn [succs label]
                                  (let [single-label #{label}
                                        dep-labels (get deps-by-label label)
                                        ;; The internal dependent outputs
                                        deps (cond-> (transient {})
                                               (and dep-labels (> (count dep-labels) 0)) (assoc! node-id dep-labels))
                                        ;; The closest overrides
                                        deps (transduce (map #(vector % single-label)) conj! deps overrides)
                                        ;; The connected nodes and their outputs
                                        deps (transduce (keep (fn [^ArcBase arc]
                                                                (let [tgt-id (.target arc)
                                                                      tgt-label (.targetLabel arc)]
                                                                  (when-let [dep-labels (get (input-deps basis tgt-id) tgt-label)]
                                                                    [tgt-id dep-labels]))))
                                               conj! deps (basis-arcs-by-head basis graph node-id node label override-filter-fn))]
                                    (assoc succs label (persistent! deps))))
                          succs labels)))))
                ;; Clean-up missing nodes from the data structure
                (dissoc succs node-id)))
      succs changes)))

(defn update-successors
  [basis changes]
  (let [changes (vec changes)]
    (reduce (fn [basis [gid changes]]
              (if-let [graph (get (:graphs basis) gid)]
                (update-in basis [:graphs gid :successors] update-graph-successors basis gid graph changes)
                basis))
      basis (group-by (comp gt/node-id->graph-id first) changes))))

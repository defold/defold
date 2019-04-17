(ns internal.system
  (:require [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as history]
            [internal.id-gen :as id-gen]
            [internal.node :as in]
            [internal.util :as util]))

(set! *warn-on-reflection* true)

(def ^:private maximum-cached-items 40000)

(prefer-method print-method java.util.Map clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord clojure.lang.IDeref)

(defn last-graph            [system]          (-> system :last-graph))
(defn system-cache          [system]          (some-> system :cache))
(defn graphs                [system]          (-> system :graphs))
(defn graph                 [system graph-id] (some-> system :graphs (get graph-id)))
(defn graph-time            [system graph-id] (some-> system :graphs (get graph-id) :tx-id))
(defn graph-history         [system graph-id] (-> system :history (get graph-id)))
(defn basis                 [system]          (ig/multigraph-basis (:graphs system)))
(defn id-generators         [system]          (-> system :id-generators))
(defn override-id-generator [system]          (-> system :override-id-generator))

(defn- bump-invalidate-counters
  [invalidate-map entries]
  (reduce (fn [m entry] (update m entry (fnil unchecked-inc 0))) invalidate-map entries))

(defn invalidate-outputs
  "Invalidate the given outputs and _everything_ that could be
  affected by them. Outputs are specified as pairs of [node-id label]
  for both the argument and return value."
  [system outputs]
  ;; 'dependencies' takes a map, where outputs is a vec of node-id+label pairs
  (let [basis (basis system)
        cache-entries (->> outputs
                           ;; vec -> map, [[node-id label]] -> {node-id #{labels}}
                           (reduce (fn [m [node-id label]]
                                     (update m node-id (fn [label-set label] (if label-set (conj label-set label) #{label})) label))
                                   {})
                           (gt/dependencies basis)
                           ;; map -> vec
                           (into [] (mapcat (fn [[node-id labels]] (mapv #(vector node-id %) labels)))))]
    (-> system
        (update :cache c/cache-invalidate cache-entries)
        (update :invalidate-counters bump-invalidate-counters cache-entries))))

(defn alter-history
  "Alters history for a graph in the system. The supplied alter-fn is called
  with a snapshot of the system, its basis, and the history for the specified
  graph-id. It should return nil to leave the system unaltered, or a map
  containing :basis, :history and :outputs-to-refresh."
  [system graph-id alter-fn]
  (let [graphs (graphs system)
        basis (ig/multigraph-basis graphs)
        history (graph-history system graph-id)
        alter-results (alter-fn system basis history)]
    (if (nil? alter-results)
      system
      (let [altered-basis (:basis alter-results)
            altered-history (:history alter-results)
            altered-outputs (:outputs-to-refresh alter-results)
            altered-graph (-> altered-basis :graphs (get graph-id))]
        (assert (gt/basis? altered-basis))
        (assert (history/history? altered-history))
        (-> system
            (assoc-in [:graphs graph-id] altered-graph)
            (assoc-in [:history graph-id] altered-history)
            (invalidate-outputs altered-outputs))))))

(def ^:private default-history-context-pred (constantly true))

(defrecord DefaultHistoryContextController []
  gt/HistoryContextController
  (history-context [_this _evaluation-context] nil)
  (history-context-pred [_this _evaluation-context] default-history-context-pred)
  (restore-history-context! [_this _history-context] false))

(def ^:private default-history-context-controller (DefaultHistoryContextController.))

(defn history-context-controller
  [system]
  (or (:history-context-controller system)
      default-history-context-controller))

(defn set-history-context-controller
  [system history-context-controller]
  (assert (gt/history-context-controller? history-context-controller))
  (assoc system :history-context-controller history-context-controller))

;; -----------------------------------------------------------------------------

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_graph-id 0)}}]
  graph)

(defn make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size))

(defn- next-available-graph-id
  [system]
  (let [used? (partial contains? (graphs system))]
    (first (drop-while used? (range 0 gt/MAX-GROUP-ID)))))

(defn next-node-id!
  [system graph-id]
  (id-gen/next-node-id! (id-generators system) graph-id))

(defn claim-node-id!
  [system graph-id node-key]
  (id-gen/claim-node-id! (id-generators system) graph-id node-key))

(defn next-override-id!
  [system graph-id]
  (id-gen/next-override-id! (override-id-generator system) graph-id))

(defn claim-override-id!
  [system graph-id override-key]
  (id-gen/claim-override-id! (override-id-generator system) graph-id override-key))

(defn attach-graph
  [system new-graph]
  (let [graph-id (next-available-graph-id system)
        graph (assoc new-graph :_graph-id graph-id)]
    (-> system
        (assoc :last-graph graph-id)
        (assoc-in [:id-generators graph-id] (id-gen/make-id-generator))
        (assoc-in [:graphs graph-id] graph))))

(defn attach-graph-with-history
  [system new-graph]
  (let [system (attach-graph system new-graph)
        graph-id (last-graph system)
        graph (graph system graph-id)]
    (assoc-in system [:history graph-id] (history/make graph "Genesis"))))

(defn detach-graph
  [system graph]
  (let [graph-id (if (map? graph) (:_graph-id graph) graph)]
    (-> system
        (update :graphs dissoc graph-id)
        (update :history dissoc graph-id))))

(defn system?
  [value]
  (and (map? value)
       (map? (:graphs value))
       (map? (:id-generators value))
       (id-gen/id-generator? (:override-id-generator value))
       (c/cache? (:cache value))
       (map? (:invalidate-counters value))
       (map? (:user-data value))
       (if-some [history (:history value)]
         (map? history)
         true)
       (if-some [history-context-controller (history-context-controller value)]
         (gt/history-context-controller? history-context-controller)
         true)))

(defn make-system
  [configuration]
  (let [initial-graph (make-initial-graph configuration)
        cache (make-cache configuration)]
    (-> {:graphs {}
         :id-generators {}
         :override-id-generator (id-gen/make-id-generator)
         :cache cache
         :invalidate-counters {}
         :user-data {}}
        (attach-graph initial-graph))))

(declare default-evaluation-context update-cache-from-evaluation-context)

(defn merge-graphs
  [system tx-data post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (let [outputs-modified-by-graph-id (util/group-into {} #{}
                                                      (comp gt/node-id->graph-id first)
                                                      outputs-modified)
        graphs-after-by-graph-id (into {}
                                       (map (fn [[graph-id graph]]
                                              (let [start-tx (:tx-id graph -1)
                                                    sidereal-tx (graph-time system graph-id)]
                                                (if (< start-tx sidereal-tx)
                                                  ;; Graph was modified concurrently by a different transaction.
                                                  (throw (ex-info "Concurrent modification of graph"
                                                                  {:_graph-id graph-id :start-tx start-tx :sidereal-tx sidereal-tx}))
                                                  [graph-id (update graph :tx-id util/safe-inc)]))))
                                       post-tx-graphs)
        history-updates (sequence
                          (keep (fn [[graph-id graph-after]]
                                  (when (and (contains? significantly-modified-graphs graph-id)
                                             (some? (graph-history system graph-id)))
                                    (let [outputs-modified (outputs-modified-by-graph-id graph-id)]
                                      [graph-id graph-after outputs-modified]))))
                          graphs-after-by-graph-id)
        history-context-controller (history-context-controller system)
        history-context-before (when (seq history-updates)
                                 (let [evaluation-context (default-evaluation-context system)]
                                   (gt/history-context history-context-controller evaluation-context)))
        post-system (-> system
                        (update :graphs merge graphs-after-by-graph-id)
                        (update :cache c/cache-invalidate outputs-modified)
                        (update :invalidate-counters bump-invalidate-counters outputs-modified)
                        (update :user-data (fn [user-data]
                                             (reduce (fn [user-data [graph-id deleted-node-ids]]
                                                       (update user-data graph-id (partial apply dissoc) deleted-node-ids))
                                                     user-data
                                                     (group-by gt/node-id->graph-id (keys nodes-deleted))))))]
    (if (empty? history-updates)
      post-system
      (let [evaluation-context (default-evaluation-context post-system)
            history-context-after (gt/history-context history-context-controller evaluation-context)]
        (-> post-system
            (update-cache-from-evaluation-context evaluation-context)
            (update :history (fn [history-by-graph-id]
                               (reduce (fn [history-by-graph-id [graph-id graph-after outputs-modified]]
                                         (update history-by-graph-id
                                                 graph-id
                                                 history/write
                                                 history-context-before
                                                 history-context-after
                                                 graph-after
                                                 outputs-modified
                                                 tx-data))
                                       history-by-graph-id
                                       history-updates))))))))

(defn basis-graphs-identical? [basis1 basis2]
  (let [graph-ids (keys (:graphs basis1))]
    (and (= graph-ids (keys (:graphs basis2)))
         (every? true? (map identical?
                            (map (:graphs basis1) graph-ids)
                            (map (:graphs basis2) graph-ids))))))

(defn default-evaluation-context [system]
  (in/default-evaluation-context (basis system)
                                 (system-cache system)
                                 (:invalidate-counters system)))

(defn custom-evaluation-context
  ;; Basis & cache options:
  ;;  * only supplying a cache makes no sense and is a programmer error
  ;;  * if neither is supplied, use from system
  ;;  * if only given basis it's not at all certain that system cache is
  ;;    derived from the given basis. One safe case is if the graphs of
  ;;    basis "==" graphs of system. If so, we use the system cache.
  ;;  * if given basis & cache we assume the cache is derived from the basis
  ;; We can only later on update the cache if we have invalidate-counters from
  ;; when the evaluation context was created, and those are only merged if
  ;; we're using the system basis & cache.
  [system options]
  (assert (not (and (some? (:cache options)) (nil? (:basis options)))))
  (let [system-options {:basis (basis system)
                        :cache (system-cache system)
                        :initial-invalidate-counters (:invalidate-counters system)}
        options (merge
                  options
                  (cond
                    (and (nil? (:cache options)) (nil? (:basis options)))
                    system-options

                    (and (nil? (:cache options))
                         (some? (:basis options))
                         (basis-graphs-identical? (:basis options) (:basis system-options)))
                    system-options))]
    (in/custom-evaluation-context options)))

(defn update-cache-from-evaluation-context
  [system evaluation-context]
  ;; We assume here that the evaluation context was created from
  ;; the system but they may have diverged, making some cache
  ;; hits/misses invalid.
  ;; Any change making the hits/misses invalid will have caused an
  ;; invalidation which we track using an invalidate-counter
  ;; map. If the cache hit/miss has not been invalidated (counters
  ;; differ) since the e.c. was created, the hit/miss is safe to
  ;; use.
  ;; If the evaluation context was created with an explicit basis
  ;; that differed from the system basis at the time, there is no
  ;; initial-invalidate-counters to compare with, and we dont even try to
  ;; update the cache.
  (if-some [initial-invalidate-counters (:initial-invalidate-counters evaluation-context)]
    (let [cache (:cache system)
          invalidate-counters (:invalidate-counters system)
          evaluation-context-hits @(:hits evaluation-context)
          evaluation-context-misses @(:local evaluation-context)]
      (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
        (cond-> system
                (seq evaluation-context-hits)
                (update :cache c/cache-hit evaluation-context-hits)

                (seq evaluation-context-misses)
                (update :cache c/cache-encache evaluation-context-misses))
        (let [invalidated-during-node-value? (fn [node-id+output]
                                               (not= (get initial-invalidate-counters node-id+output 0)
                                                     (get invalidate-counters node-id+output 0)))
              safe-cache-hits (remove invalidated-during-node-value? evaluation-context-hits)
              safe-cache-misses (remove (comp invalidated-during-node-value? first) evaluation-context-misses)]
          (cond-> system
                  (seq safe-cache-hits)
                  (update :cache c/cache-hit safe-cache-hits)

                  (seq safe-cache-misses)
                  (update :cache c/cache-encache safe-cache-misses)))))
    system))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [node-id label evaluation-context]
  (in/node-value node-id label evaluation-context))

(defn user-data [system node-id key]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (get-in (:user-data system) [graph-id node-id key])))

(defn assoc-user-data [system node-id key value]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (assoc-in system [:user-data graph-id node-id key] value)))

(defn update-user-data [system node-id key f & args]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (update-in system [:user-data graph-id node-id key] #(apply f %1 %2) args)))

(defn clone-system [system]
  {:graphs (:graphs system)
   :history (:history system)
   :history-context-controller (:history-context-controller system)
   :id-generators (into {}
                        (map (fn [[graph-id id-generator]]
                               [graph-id (atom (deref id-generator))]))
                        (:id-generators system))
   :override-id-generator (atom (deref (:override-id-generator system)))
   :cache (:cache system)
   :user-data (:user-data system)
   :invalidate-counters (:invalidate-counters system)
   :last-graph (:last-graph system)})

(defn system= [s1 s2]
  (and (= (:graphs s1) (:graphs s2))
       (= (:history s1) (:history s2))
       (= (:history-context-controller s1) (:history-context-controller s2))
       (= (map (fn [[graph-id id-generator]] [graph-id (id-gen/peek-id id-generator)]) (:id-generators s1))
          (map (fn [[graph-id id-generator]] [graph-id (id-gen/peek-id id-generator)]) (:id-generators s2)))
       (= (id-gen/peek-id (:override-id-generator s1))
          (id-gen/peek-id (:override-id-generator s2)))
       (= (:cache s1) (:cache s2))
       (= (:user-data s1) (:user-data s2))
       (= (:invalidate-counters s1) (:invalidate-counters s2))
       (= (:last-graph s1) (:last-graph s2))))

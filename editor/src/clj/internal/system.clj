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

(defn- update-history [system graph-id update-fn]
  (let [history (graph-history system graph-id)
        graphs (graphs system)
        basis (ig/multigraph-basis graphs)
        update-results (update-fn history basis)]
    (if (nil? update-results)
      system
      (let [updated-basis (:basis update-results)
            updated-history (:history update-results)
            updated-outputs (:outputs-to-refresh update-results)
            updated-graph (-> updated-basis :graphs (get graph-id))]
        (-> system
            (assoc-in [:graphs graph-id] updated-graph)
            (assoc-in [:history graph-id] updated-history)
            (invalidate-outputs updated-outputs))))))

(defn undo-history [system graph-id]
  (update-history system graph-id
                  (fn [history basis]
                    (when-some [alteration (history/find-undoable-entry history)]
                      (history/alter-history history basis alteration (id-generators system) (override-id-generator system))))))

(defn redo-history [system graph-id]
  (update-history system graph-id
                  (fn [history basis]
                    (when-some [alteration (history/find-redoable-entry history)]
                      (history/alter-history history basis alteration (id-generators system) (override-id-generator system))))))

(defn undo-stack [history]
  ;; TODO: Inefficient. Remove!
  (filterv :enabled? (rseq (subvec history 1))))

(defn redo-stack [history]
  ;; TODO: Inefficient. Remove!
  (into []
        (remove :enabled?)
        history))

(defn clear-history [system graph-id]
  (let [graph (graph system graph-id)
        cleared-history (history/make-history graph)]
    (assoc-in system [:history graph-id] cleared-history)))

(defn cancel [system graph-id sequence-label]
  (assert (some? sequence-label))
  (update-history system graph-id
                  (fn [history basis]
                    (let [history-length (count history)]
                      (when (= sequence-label (history/history-sequence-label history))
                        (history/rewind-history history basis (- history-length 2)))))))

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

(defn- attach-graph*
  [system graph-id graph]
  (-> system
      (assoc :last-graph graph-id)
      (update-in [:id-generators] assoc graph-id (id-gen/make-id-generator))
      (update-in [:graphs] assoc graph-id graph)))

(defn attach-graph
  [system graph]
  (let [graph-id (next-available-graph-id system)]
    (attach-graph* system graph-id graph)))

(defn attach-graph-with-history
  [system graph]
  (let [graph-id (next-available-graph-id system)]
    (-> system
        (attach-graph* graph-id graph)
        (assoc-in [:history graph-id] (history/make-history graph)))))

(defn detach-graph
  [system graph]
  (let [graph-id (if (map? graph) (:_graph-id graph) graph)]
    (-> system
        (update :graphs dissoc graph-id)
        (update :history dissoc graph-id))))

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

(defn merge-graphs
  [system tx-data post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (let [outputs-modified-by-graph-id (util/group-into #{} (comp gt/node-id->graph-id first) outputs-modified)
        post-system (reduce (fn [system [graph-id graph]]
                              (let [start-tx (:tx-id graph -1)
                                    sidereal-tx (graph-time system graph-id)]
                                (if (< start-tx sidereal-tx)
                                  ;; graph was modified concurrently by a different transaction.
                                  (throw (ex-info "Concurrent modification of graph"
                                                  {:_graph-id graph-id :start-tx start-tx :sidereal-tx sidereal-tx}))
                                  (let [meaningful-change? (contains? significantly-modified-graphs graph-id)
                                        graph-after (update graph :tx-id util/safe-inc)
                                        graph-after (if meaningful-change?
                                                      graph-after
                                                      (let [prev-tx-sequence-label (get-in system [:graphs graph-id :tx-sequence-label])]
                                                        (assoc graph-after :tx-sequence-label prev-tx-sequence-label)))
                                        system-after (assoc-in system [:graphs graph-id] graph-after)
                                        history-before (graph-history system graph-id)]
                                    (if (or (nil? history-before)
                                            (not meaningful-change?))
                                      system-after
                                      (let [outputs-modified (outputs-modified-by-graph-id graph-id)
                                            history-after (history/write-history history-before graph-after outputs-modified tx-data)]
                                        (assoc-in system-after [:history graph-id] history-after)))))))
                            system
                            post-tx-graphs)]
    (-> post-system
        (update :cache c/cache-invalidate outputs-modified)
        (update :invalidate-counters bump-invalidate-counters outputs-modified)
        (update :user-data (fn [user-data]
                             (reduce (fn [user-data [graph-id deleted-node-ids]]
                                       (update user-data graph-id (partial apply dissoc) deleted-node-ids))
                                     user-data
                                     (group-by gt/node-id->graph-id (keys nodes-deleted))))))))

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
  [system node-or-node-id label evaluation-context]
  (in/node-value node-or-node-id label evaluation-context))

(defn node-property-value [node-or-node-id label evaluation-context]
  (in/node-property-value node-or-node-id label evaluation-context))

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
       (= (map (fn [[graph-id id-generator]] [graph-id (id-gen/peek-id id-generator)]) (:id-generators s1))
          (map (fn [[graph-id id-generator]] [graph-id (id-gen/peek-id id-generator)]) (:id-generators s2)))
       (= (id-gen/peek-id (:override-id-generator s1))
          (id-gen/peek-id (:override-id-generator s2)))
       (= (:cache s1) (:cache s2))
       (= (:user-data s1) (:user-data s2))
       (= (:invalidate-counters s1) (:invalidate-counters s2))
       (= (:last-graph s1) (:last-graph s2))))

(ns internal.system
  (:require [internal.util :as util]
            [internal.util :refer [map-vals]]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as h]
            [internal.node :as in]
            [service.log :as log])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(declare graphs)

(def ^:private maximum-cached-items     40000)
(def ^:private maximum-disposal-backlog 2500)
(def ^:private history-size-max         60)

(prefer-method print-method java.util.Map clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord clojure.lang.IDeref)

(defn- integer-counter
  []
  (AtomicLong. 0))

(defn- new-history []
  {:tape (conj (h/paper-tape history-size-max) [])})

(defrecord HistoryState [label graph sequence-label cache-keys])

(defn history-state [graph outputs-modified]
  (->HistoryState (:tx-label graph) graph (:tx-sequence-label graph) outputs-modified))

(defn- history-state-merge-cache-keys
  [new old]
  (update new :cache-keys into (:cache-keys old)))

(defn- merge-into-top
  [tape new-state]
  (let [old-state (h/ivalue tape)]
    (conj
      (h/truncate (h/iprev tape))
      (history-state-merge-cache-keys new-state old-state))))

(defn =*
  "Comparison operator that treats nil as not equal to anything."
  ([x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn merge-or-push-history
  [history graph-ref old-graph new-graph outputs-modified]
  (let [new-state (history-state new-graph (set outputs-modified))
        tape-op (if (=* (:tx-sequence-label new-graph) (:tx-sequence-label old-graph))
                  merge-into-top
                  conj)]
    (update history :tape tape-op new-state)))

(defn undo-stack [history-ref]
  (->> history-ref
       deref
       :tape
       h/before
       next
       vec))

(defn time-warp [history-ref system-snapshot graph outputs-to-refresh]
  (let [graph-id (:_graph-id graph)
        graphs (graphs system-snapshot)]
    (let [pseudo-basis (ig/multigraph-basis (map-vals deref graphs))
          {hydrated-basis :basis
           hydrated-outputs-to-refresh :outputs-to-refresh} (ig/hydrate-after-undo pseudo-basis graph)
          outputs-to-refresh (into (or outputs-to-refresh #{}) hydrated-outputs-to-refresh)
          changes (->> outputs-to-refresh
                       (group-by first)
                       (map (fn [[node-id labels]] [node-id (set (map second labels))]))
                       (into {}))
          warped-basis (ig/update-successors hydrated-basis changes)]
      {:graph (get-in warped-basis [:graphs graph-id])
       :outputs-to-refresh outputs-to-refresh})))

(defn last-graph            [system]          (-> system :last-graph))
(defn system-cache          [system]          (some-> system :cache deref))
(defn graphs                [system]          (-> system :graphs))
(defn graph-ref             [system graph-id] (-> system :graphs (get graph-id)))
(defn graph                 [system graph-id] (some-> system (graph-ref graph-id) deref))
(defn graph-time            [system graph-id] (-> system (graph graph-id) :tx-id))
(defn graph-history         [system graph-id] (-> system :history (get graph-id)))
(defn basis                 [system]          (ig/multigraph-basis (map-vals deref (graphs system))))
(defn id-generators         [system]          (-> system :id-generators))
(defn override-id-generator [system]          (-> system :override-id-generator))

(defn- bump-invalidate-counters
  [invalidate-map entries]
  (reduce (fn [m entry] (update m entry (fnil unchecked-inc 0))) invalidate-map entries))

(def retry-counts (atom {}))

(defn invalidate-outputs!
  "Invalidate the given outputs and _everything_ that could be
  affected by them. Outputs are specified as pairs of [node-id label]
  for both the argument and return value."
  [system outputs]
  ;; 'dependencies' takes a map, where outputs is a vec of node-id+label pairs
  (let [retries (atom 0)
        cache-entries (dosync
                        (swap! retries inc)
                        (let [basis (basis system)
                              cache-entries (->> outputs
                                                 ;; vec -> map, [[node-id label]] -> {node-id #{labels}}
                                                 (reduce (fn [m [node-id label]]
                                                           (update m node-id (fn [label-set label] (if label-set (conj label-set label) #{label})) label))
                                                         {})
                                                 (gt/dependencies basis)
                                                 ;; map -> vec
                                                 (into [] (mapcat (fn [[node-id labels]] (mapv #(vector node-id %) labels)))))]
                          (alter (:cache system) c/cache-invalidate cache-entries)
                          (alter (:invalidate-counters system) bump-invalidate-counters cache-entries)
                          cache-entries))]
    (swap! retry-counts update-in [:invalidate-outputs @retries] (fnil inc 0))
    cache-entries))

(defn step-through-history!
  [step-function system graph-id]
  (dosync
    (let [graph-ref (graph-ref system graph-id)
          history-ref (graph-history system graph-id)
          {:keys [tape]} @history-ref
          prior-state (h/ivalue tape)
          tape (step-function tape)
          next-state (h/ivalue tape)
          outputs-to-refresh (into (:cache-keys prior-state) (:cache-keys next-state))]
      (when next-state
        (let [{:keys [graph outputs-to-refresh]} (time-warp history-ref system (:graph next-state) outputs-to-refresh)]
          (ref-set graph-ref graph)
          (alter history-ref assoc :tape tape)
          (invalidate-outputs! system outputs-to-refresh)
          outputs-to-refresh)))))

(def undo-history! (partial step-through-history! h/iprev))
(def cancel-history! (partial step-through-history! h/drop-current))
(def redo-history! (partial step-through-history! h/inext))

(defn redo-stack [history-ref]
  (->> history-ref
       deref
       :tape
       h/after
       vec))

(defn clear-history!
  [system graph-id]
  (let [history-ref (graph-history system graph-id)]
    (dosync
      (let [graph (graph system graph-id)
            initial-state (history-state graph #{})]
        (alter history-ref update :tape (fn [tape]
                                          (conj (empty tape) initial-state)))))))

(defn cancel!
  [system-snapshot graph-id sequence-id]
  (let [history-ref (graph-history system-snapshot graph-id)
        {:keys [tape]}  @history-ref
        previous-change (h/ivalue tape)
        ok-to-cancel?   (=* sequence-id (:sequence-label previous-change))]
    (when ok-to-cancel? (cancel-history! system-snapshot graph-id))))

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_graph-id 0)}}]
  graph)

(defn make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size))

(defn- next-available-graph-id
  [system]
  (let [used (into #{} (keys (graphs system)))]
    (first (drop-while used (range 0 gt/MAX-GROUP-ID)))))

(defn next-node-id*
  [id-generators graph-id]
  (gt/make-node-id graph-id (.getAndIncrement ^AtomicLong (get id-generators graph-id))))

(defn next-node-id
  [system graph-id]
  (next-node-id* (id-generators system) graph-id))

(defn next-override-id*
  [override-id-generator graph-id]
  (gt/make-override-id graph-id (.getAndIncrement ^AtomicLong override-id-generator)))

(defn next-override-id
  [system graph-id]
  (next-override-id* (override-id-generator system) graph-id))

(defn- attach-graph*!
  [system graph-ref]
  (let [graph-id (next-available-graph-id system)]
    (dosync (alter graph-ref assoc :_graph-id graph-id))
    (-> system
        (assoc :last-graph graph-id)
        (update-in [:id-generators] assoc graph-id (integer-counter))
        (update-in [:graphs] assoc graph-id graph-ref))))

(defn attach-graph!
  [system graph]
  (attach-graph*! system (ref graph)))

(defn attach-graph-with-history!
  [system graph]
  (let [graph-ref (ref graph)
        history-ref (ref (new-history))]
    (-> system
        (attach-graph*! graph-ref)
        (update :history assoc (:_graph-id @graph-ref) history-ref))))

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
         :override-id-generator (integer-counter)
         :cache (ref cache)
         :invalidate-counters (ref {})
         :user-data (ref {})}
        (attach-graph! initial-graph))))

(defn- has-history? [system graph-id] (not (nil? (graph-history system graph-id))))
(def ^:private meaningful-change? contains?)

(defn- remember-change!
  [system graph-id before after outputs-modified]
  (alter (graph-history system graph-id) merge-or-push-history (graph-ref system graph-id) before after outputs-modified))

(defn merge-graphs!
  [system post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (let [retries (atom 0)
        result (dosync
                 (swap! retries inc)
                 (let [outputs-modified (group-by #(gt/node-id->graph-id (first %)) outputs-modified)]
                   (doseq [[graph-id graph] post-tx-graphs]
                     (let [start-tx (:tx-id graph -1)
                           sidereal-tx (graph-time system graph-id)]
                       (if (< start-tx sidereal-tx)
                         ;; graph was modified concurrently by a different transaction.
                         (throw (ex-info "Concurrent modification of graph"
                                         {:_graph-id graph-id :start-tx start-tx :sidereal-tx sidereal-tx}))
                         (let [graph-ref (graph-ref system graph-id)
                               before @graph-ref
                               after (update-in graph [:tx-id] util/safe-inc)
                               after (if (not (meaningful-change? significantly-modified-graphs graph-id))
                                       (assoc after :tx-sequence-label (:tx-sequence-label before))
                                       after)]
                           (when (and (has-history? system graph-id) (meaningful-change? significantly-modified-graphs graph-id))
                             (remember-change! system graph-id before after (outputs-modified graph-id)))
                           (ref-set graph-ref after))))))
                 (alter (:cache system) c/cache-invalidate outputs-modified)
                 (alter (:invalidate-counters system) bump-invalidate-counters outputs-modified)
                 (alter (:user-data system) (fn [user-data]
                                              (reduce (fn [user-data node-id]
                                                        (let [graph-id (gt/node-id->graph-id node-id)]
                                                          (update user-data graph-id dissoc node-id)))
                                                      user-data (keys nodes-deleted)))))
        ]
    (swap! retry-counts update-in [:merge-graphs @retries] (fnil inc 0))
    result))

(defn basis-graphs-identical? [basis1 basis2]
  (let [graph-ids (keys (:graphs basis1))]
    (and (= graph-ids (keys (:graphs basis2)))
         (every? true? (map identical?
                            (map (:graphs basis1) graph-ids)
                            (map (:graphs basis2) graph-ids))))))

(defn default-evaluation-context [system]
  (let [retries (atom 0)
        ec (dosync
             (swap! retries inc)
             (in/default-evaluation-context (basis system)
                                            (system-cache system)
                                            @(:invalidate-counters system)))]
    (swap! retry-counts update-in [:def-eval-ctxt @retries] (fnil inc 0))
    ec))

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
  (let [retries (atom 0)
        system-options (dosync
                         (swap! retries inc)
                         {:basis (basis system)
                          :cache (system-cache system)
                          :initial-invalidate-counters @(:invalidate-counters system)})
        options (merge
                  options
                  (cond
                    (and (nil? (:cache options)) (nil? (:basis options)))
                    system-options

                    (and (nil? (:cache options))
                         (some? (:basis options))
                         (basis-graphs-identical? (:basis options) (:basis system-options)))
                    system-options))]
    (swap! retry-counts update-in [:cust-eval-ctxt @retries] (fnil inc 0))
    (in/custom-evaluation-context options)))

(defn update-cache-from-evaluation-context!
  [system evaluation-context]
  (let [retries (atom 0)
        res (dosync
      (swap! retries inc)
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
      (when-let [initial-invalidate-counters (:initial-invalidate-counters evaluation-context)]
        (let [cache (:cache system)
              invalidate-counters @(:invalidate-counters system)
              evaluation-context-hits @(:hits evaluation-context)
              evaluation-context-misses @(:local evaluation-context)]
          (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
            (do (when (seq evaluation-context-hits) (alter cache c/cache-hit evaluation-context-hits))
                (when (seq evaluation-context-misses) (alter cache c/cache-encache evaluation-context-misses)))
            (let [invalidated-during-node-value? (fn [node-id+output]
                                                   (not= (get initial-invalidate-counters node-id+output 0)
                                                         (get invalidate-counters node-id+output 0)))
                  safe-cache-hits (remove invalidated-during-node-value? evaluation-context-hits)
                  safe-cache-misses (remove (comp invalidated-during-node-value? first) evaluation-context-misses)]
              (when (seq safe-cache-hits) (alter cache c/cache-hit safe-cache-hits))
              (when (seq safe-cache-misses) (alter cache c/cache-encache safe-cache-misses)))))))]
    
    (swap! retry-counts update-in [:update-cache @retries] (fnil inc 0))
    res
    ))

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
    (get-in @(:user-data system) [graph-id node-id key])))

(defn user-data! [system node-id key value]
  (dosync
    (let [graph-id (gt/node-id->graph-id node-id)]
      (alter (:user-data system) assoc-in [graph-id node-id key] value))))

(defn user-data-swap! [system node-id key f & args]
  (dosync
    (let [graph-id (gt/node-id->graph-id node-id)
          path [graph-id node-id key]
          user-data (apply alter (:user-data system) update-in path f args)]
      (get-in user-data path))))

(defn clone-system [system]
  (dosync 
    {:graphs (into {} (map (fn [[graph-id graph-ref]] [graph-id (ref (deref graph-ref))]))
                   (:graphs system))
     :history (into {} (map (fn [[graph-id history-ref]] [graph-id (ref (deref history-ref))]))
                    (:history system))
     :id-generators (into {} (map (fn [[graph-id ^AtomicLong gen]] [graph-id (AtomicLong. (.longValue gen))]))
                          (:id-generators system))
     :override-id-generator (AtomicLong. (.longValue ^AtomicLong (:override-id-generator system)))
     :cache (ref (deref (:cache system)))
     :user-data (ref (deref (:user-data system)))
     :invalidate-counters (ref (deref (:invalidate-counters system)))
     :last-graph (:last-graph system)}))

(defn system= [s1 s2]
  (dosync
    (and (= (map (fn [[graph-id graph-ref]] [graph-id (deref graph-ref)]) (:graphs s1))
            (map (fn [[graph-id graph-ref]] [graph-id (deref graph-ref)]) (:graphs s2)))
         (= (map (fn [[graph-id history-ref]] [graph-id (deref history-ref)]) (:history s1))
            (map (fn [[graph-id history-ref]] [graph-id (deref history-ref)]) (:history s2)))
         (= (map (fn [[graph-id ^AtomicLong gen]] [graph-id (.longValue gen)]) (:id-generators s1))
            (map (fn [[graph-id ^AtomicLong gen]] [graph-id (.longValue gen)]) (:id-generators s2)))
         (= (.longValue ^AtomicLong (:override-id-generator s1))
            (.longValue ^AtomicLong (:override-id-generator s2)))
         (= (deref (:cache s1)) (deref (:cache s2)))
         (= (deref (:user-data s1)) (deref (:user-data s2)))
         (= (deref (:invalidate-counters s1)) (deref (:invalidate-counters s2)))
         (= (:last-graph s1) (:last-graph s2)))))

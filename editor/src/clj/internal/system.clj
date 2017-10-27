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

(prefer-method print-method java.util.Map               clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord        clojure.lang.IDeref)

(defn- integer-counter
  []
  (AtomicLong. 0))

(defn- new-history []
  {:tape       (conj (h/paper-tape history-size-max) [])})

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
  [history gref old-graph new-graph outputs-modified]
  (let [new-state (history-state new-graph (set outputs-modified))
        tape-op   (if (=* (:tx-sequence-label new-graph) (:tx-sequence-label old-graph))
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
  (let [gid (:_gid graph)
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
      {:graph (get-in warped-basis [:graphs gid])
       :outputs-to-refresh outputs-to-refresh})))

(defn last-graph     [s]     (-> s :last-graph))
(defn system-cache   [s]     (some-> s :cache deref))
(defn graphs         [s]     (-> s :graphs))
(defn graph-ref      [s gid] (-> s :graphs (get gid)))
(defn graph          [s gid] (some-> s (graph-ref gid) deref))
(defn graph-time     [s gid] (-> s (graph gid) :tx-id))
(defn graph-history  [s gid] (-> s :history (get gid)))
(defn basis          [s]     (ig/multigraph-basis (map-vals deref (graphs s))))
(defn id-generators  [s]     (-> s :id-generators))
(defn override-id-generator [s] (-> s :override-id-generator))

(defn- bump-invalidate-counters
  [invalidate-map entries]
  (reduce (fn [m entry] (update m entry (fnil unchecked-inc 0))) invalidate-map entries))

(defn invalidate-outputs!
  "Invalidate the given outputs and _everything_ that could be
  affected by them. Outputs are specified as pairs of [node-id label]
  for both the argument and return value."
  [sys outputs]
  ;; 'dependencies' takes a map, where outputs is a vec of node-id+label pairs
  (dosync
    (let [basis (basis sys)
          cache-entries (->> outputs
                             ;; vec -> map, [[node-id label]] -> {node-id #{labels}}
                             (reduce (fn [m [nid l]]
                                       (update m nid (fn [s l] (if s (conj s l) #{l})) l))
                                     {})
                             (gt/dependencies basis)
                             ;; map -> vec
                             (into [] (mapcat (fn [[nid ls]] (mapv #(vector nid %) ls)))))]
      (alter (:cache sys) c/cache-invalidate cache-entries)
      (alter (:invalidate-counters sys) bump-invalidate-counters cache-entries))))

(defn step-through-history!
  [step-function sys graph-id]
  (dosync
   (let [graph-ref (graph-ref sys graph-id)
         history-ref (graph-history sys graph-id)
         {:keys [tape]} @history-ref
         prior-state              (h/ivalue tape)
         tape                     (step-function tape)
         next-state               (h/ivalue tape)
         outputs-to-refresh       (into (:cache-keys prior-state) (:cache-keys next-state))]
     (when next-state
       (let [{:keys [graph outputs-to-refresh]} (time-warp history-ref sys (:graph next-state) outputs-to-refresh)]
         (ref-set graph-ref graph)
         (alter history-ref assoc :tape tape)
         (invalidate-outputs! sys outputs-to-refresh)
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
  [sys graph-id]
  (let [history-ref (graph-history sys graph-id)]
    (dosync
      (let [graph         (graph sys graph-id)
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
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_gid 0)}}]
  graph)

(defn- make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size))

(defn next-available-gid
  [s]
  (let [used (into #{} (keys (graphs s)))]
    (first (drop-while used (range 0 gt/MAX-GROUP-ID)))))

(defn next-node-id*
  [id-generators gid]
  (gt/make-node-id gid (.getAndIncrement ^AtomicLong (get id-generators gid))))

(defn next-node-id
  [s gid]
  (next-node-id* (id-generators s) gid))

(defn next-override-id*
  [override-id-generator gid]
  (gt/make-override-id gid (.getAndIncrement ^AtomicLong override-id-generator)))

(defn next-override-id
  [s gid]
  (next-override-id* (override-id-generator s) gid))

(defn- attach-graph*!
  [s gref]
  (let [gid (next-available-gid s)]
    (dosync (alter gref assoc :_gid gid))
    (-> s
        (assoc :last-graph gid)
        (update-in [:id-generators] assoc gid (integer-counter))
        (update-in [:graphs] assoc gid gref))))

(defn attach-graph!
  [s g]
  (attach-graph*! s (ref g)))

(defn attach-graph-with-history!
  [s g]
  (let [gref (ref g)
        href (ref (new-history))]
    (-> s
      (attach-graph*! gref)
      (update :history assoc (:_gid @gref) href))))

(defn detach-graph
  [s g]
  (let [gid (if (map? g) (:_gid g) g)]
    (-> s
      (update :graphs dissoc gid)
      (update :history dissoc gid))))

(defn make-system
  [configuration]
  (let [initial-graph  (make-initial-graph configuration)
        cache          (make-cache configuration)]
    (-> {:graphs         {}
         :id-generators  {}
         :override-id-generator (integer-counter)
         :cache          (ref cache)
         :invalidate-counters (ref {})
         :user-data      (ref {})}
        (attach-graph! initial-graph))))

(defn- has-history? [sys graph-id] (not (nil? (graph-history sys graph-id))))
(def ^:private meaningful-change? contains?)

(defn- remember-change!
  [sys graph-id before after outputs-modified]
  (alter (graph-history sys graph-id) merge-or-push-history (graph-ref sys graph-id) before after outputs-modified))

(defn merge-graphs!
  [sys post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (dosync
    (let [outputs-modified (group-by #(gt/node-id->graph-id (first %)) outputs-modified)]
      (doseq [[graph-id graph] post-tx-graphs]
        (let [start-tx    (:tx-id graph -1)
              sidereal-tx (graph-time sys graph-id)]
          (if (< start-tx sidereal-tx)
            ;; graph was modified concurrently by a different transaction.
            (throw (ex-info "Concurrent modification of graph"
                     {:_gid graph-id :start-tx start-tx :sidereal-tx sidereal-tx}))
            (let [gref   (graph-ref sys graph-id)
                  before @gref
                  after  (update-in graph [:tx-id] util/safe-inc)
                  after  (if (not (meaningful-change? significantly-modified-graphs graph-id))
                           (assoc after :tx-sequence-label (:tx-sequence-label before))
                           after)]
              (when (and (has-history? sys graph-id) (meaningful-change? significantly-modified-graphs graph-id))
                (remember-change! sys graph-id before after (outputs-modified graph-id)))
              (ref-set gref after))))))
    (alter (:cache sys) c/cache-invalidate outputs-modified)
    (alter (:invalidate-counters sys) bump-invalidate-counters outputs-modified)
    (alter (:user-data sys) (fn [user-data]
                              (reduce (fn [user-data node-id]
                                        (let [gid (gt/node-id->graph-id node-id)]
                                          (update user-data gid dissoc node-id)))
                                      user-data (keys nodes-deleted))))))

(defn basis-graphs-identical? [basis1 basis2]
  (let [gids (keys (:graphs basis1))]
    (and (= gids (keys (:graphs basis2)))
         (every? true? (map identical?
                            (map (:graphs basis1) gids)
                            (map (:graphs basis2) gids))))))

(defn make-evaluation-context
  ;; Basis & cache options:
  ;;  * only supplying a cache makes no sense and is a programmer error
  ;;  * if neither is supplied, use from sys
  ;;  * if only given basis it's not at all certain that sys cache is
  ;;    derived from the given basis. One safe case is if the graphs of
  ;;    basis "==" graphs of sys. If so, we use the sys cache.
  ;;  * if given basis & cache we assume the cache is derived from the basis
  ;; We can only later on update the cache if we have invalidate-counters from
  ;; when the evaluation context was created, and those are only merged if
  ;; we're using the sys basis & cache.
  [sys options]
  (assert (not (and (some? (:cache options)) (nil? (:basis options)))))
  (let [sys-options (dosync
                      {:basis (basis sys)
                       :cache (system-cache sys)
                       :initial-invalidate-counters @(:invalidate-counters sys)})
        options (merge
                  options
                  (cond
                    (and (nil? (:cache options)) (nil? (:basis options)))
                    sys-options

                    (and (nil? (:cache options))
                         (some? (:basis options))
                         (basis-graphs-identical? (:basis options) (:basis sys-options)))
                    sys-options))]
    (in/make-evaluation-context options)))

(defn update-cache-from-evaluation-context!
  [sys evaluation-context]
  (dosync
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
      (let [cache (:cache sys)
            invalidate-counters @(:invalidate-counters sys)
            invalidated-during-node-value? (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
                                             (constantly false)
                                             (fn [node-id+output]
                                               (not= (get initial-invalidate-counters node-id+output 0)
                                                     (get invalidate-counters node-id+output 0))))
            safe-cache-hits (remove invalidated-during-node-value? @(:hits evaluation-context))
            safe-cache-misses (remove (comp invalidated-during-node-value? first) @(:local evaluation-context))]
        (alter cache c/cache-hit safe-cache-hits)
        (alter cache c/cache-encache safe-cache-misses)))))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [sys node-or-node-id label evaluation-context]
  (in/node-value node-or-node-id label evaluation-context))

(defn node-property-value [node-or-node-id label evaluation-context]
  (in/node-property-value node-or-node-id label evaluation-context))

(defn user-data [sys node-id key]
  (let [gid (gt/node-id->graph-id node-id)]
    (get-in @(:user-data sys) [gid node-id key])))

(defn user-data! [sys node-id key value]
  (dosync
    (let [gid (gt/node-id->graph-id node-id)]
      (alter (:user-data sys) assoc-in [gid node-id key] value))))

(defn user-data-swap! [sys node-id key f & args]
  (dosync
    (let [gid (gt/node-id->graph-id node-id)
          path [gid node-id key]
          user-data (apply alter (:user-data sys) update-in path f args)]
      (get-in user-data path))))

(defn clone-system [s]
  (dosync 
    {:graphs (into {} (map (fn [[gid gref]] [gid (ref (deref gref))]))
                   (:graphs s))
     :history (into {} (map (fn [[gid href]] [gid (ref (deref href))]))
                    (:history s))
     :id-generators (into {} (map (fn [[gid ^AtomicLong gen]] [gid (AtomicLong. (.longValue gen))]))
                          (:id-generators s))
     :override-id-generator (AtomicLong. (.longValue ^AtomicLong (:override-id-generator s)))
     :cache (ref (deref (:cache s)))
     :user-data (ref (deref (:user-data s)))
     :invalidate-counters (ref (deref (:invalidate-counters s)))
     :last-graph (:last-graph s)}))

;; Copyright 2020-2022 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.system
  (:require [internal.util :as util]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as h]
            [internal.node :as in]
            [service.log :as log])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(declare graphs)

(def ^:private maximum-cached-items     20000)
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

(defn- =*
  "Comparison operator that treats nil as not equal to anything."
  ([x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn merge-or-push-history
  [history old-graph new-graph outputs-modified]
  (assert (set? outputs-modified))
  (let [new-state (history-state new-graph outputs-modified)
        tape-op (if (=* (:tx-sequence-label new-graph) (:tx-sequence-label old-graph))
                  merge-into-top
                  conj)]
    (update history :tape tape-op new-state)))

(defn undo-stack [history]
  (->> history
       :tape
       h/before
       next
       vec))

(defn- time-warp [system graph outputs-to-refresh]
  (let [graph-id (:_graph-id graph)
        graphs (graphs system)]
    (let [pseudo-basis (ig/multigraph-basis graphs)
          {hydrated-basis :basis
           hydrated-outputs-to-refresh :outputs-to-refresh} (ig/hydrate-after-undo pseudo-basis graph)
          outputs-to-refresh (into (or outputs-to-refresh #{}) hydrated-outputs-to-refresh)
          changes (util/group-into {} #{} gt/endpoint-node-id gt/endpoint-label outputs-to-refresh)
          warped-basis (ig/update-successors hydrated-basis changes)]
      {:graph (get-in warped-basis [:graphs graph-id])
       :outputs-to-refresh outputs-to-refresh})))

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
  [invalidate-map endpoints]
  (persistent!
    (reduce
      (fn [m endpoint]
        (assert (gt/endpoint? endpoint))
        (assoc! m endpoint (unchecked-inc (m endpoint 0))))
      (transient invalidate-map)
      endpoints)))

(defn invalidate-outputs
  "Invalidate the given outputs and _everything_ that could be
  affected by them. Outputs are specified as collection of endpoints
  for both the argument and return value."
  [system outputs]
  (assert (every? gt/endpoint? outputs))
  ;; 'dependencies' takes a map, where outputs is a vec of node-id+label pairs
  (let [basis (basis system)
        cache-entries (gt/dependencies basis outputs)]
    (-> system
        (update :cache c/cache-invalidate cache-entries)
        (update :invalidate-counters bump-invalidate-counters cache-entries))))

(defn- step-through-history
  [step-function system graph-id]
  (let [history (graph-history system graph-id)
        {:keys [tape]} history
        prior-state (h/ivalue tape)
        tape (step-function tape)
        next-state (h/ivalue tape)
        outputs-to-refresh (into (:cache-keys prior-state)
                                 (:cache-keys next-state))]
    (if-not next-state
      system
      (let [{:keys [graph outputs-to-refresh]} (time-warp system (:graph next-state) outputs-to-refresh)]
        (-> system
            (assoc-in [:graphs graph-id] graph)
            (assoc-in [:history graph-id :tape] tape)
            (invalidate-outputs outputs-to-refresh))))))

(def undo-history (partial step-through-history h/iprev))
(def cancel-history (partial step-through-history h/drop-current))
(def redo-history (partial step-through-history h/inext))

(defn redo-stack [history]
  (->> history
       :tape
       h/after
       vec))

(defn clear-history
  [system graph-id]
  (let [graph (get-in system [:graphs graph-id])
        initial-state (history-state graph #{})]
    (update-in system [:history graph-id :tape] (fn [tape] (conj (empty tape) initial-state)))))

(defn cancel
  [system graph-id sequence-id]
  (let [history (graph-history system graph-id)
        tape (:tape history)
        previous-change (h/ivalue tape)
        ok-to-cancel? (=* sequence-id (:sequence-label previous-change))]
    (cond-> system
            ok-to-cancel? (cancel-history graph-id))))

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_graph-id 0)}}]
  graph)

(defn make-cache
  [{:keys [cache-size cache-retain?] :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size cache-retain?))

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

(defn- attach-graph*
  [system graph-id graph]
  (-> system
      (assoc :last-graph graph-id)
      (assoc-in [:id-generators graph-id] (integer-counter))
      (assoc-in [:graphs graph-id] (assoc graph :_graph-id graph-id))))

(defn attach-graph
  [system graph]
  (let [graph-id (next-available-graph-id system)]
    (attach-graph* system graph-id graph)))

(defn attach-graph-with-history
  [system graph]
  (let [graph-id (next-available-graph-id system)]
    (-> system
        (attach-graph* graph-id graph)
        (assoc-in [:history graph-id] (new-history)))))

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
         :cache cache
         :invalidate-counters {}
         :user-data {}}
        (attach-graph initial-graph))))

(defn- has-history? [system graph-id] (not (nil? (graph-history system graph-id))))
(def ^:private meaningful-change? contains?)

(defn- remember-change
  [system graph-id before after outputs-modified]
  (update-in system [:history graph-id] merge-or-push-history before after outputs-modified))

(defn merge-graphs
  [system post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (let [graph-id->outputs-modified (util/group-into
                                     {}
                                     #{}
                                     #(gt/node-id->graph-id (gt/endpoint-node-id %))
                                     outputs-modified)
        post-system (reduce (fn [system [graph-id graph]]
                              (let [start-tx (:tx-id graph -1)
                                    sidereal-tx (graph-time system graph-id)]
                                (if (< start-tx sidereal-tx)
                                  ;; graph was modified concurrently by a different transaction.
                                  (throw (ex-info "Concurrent modification of graph"
                                                  {:_graph-id graph-id :start-tx start-tx :sidereal-tx sidereal-tx}))
                                  (let [graph-before (get-in system [:graphs graph-id])
                                        graph-after (update graph :tx-id util/safe-inc)
                                        graph-after (if (not (meaningful-change? significantly-modified-graphs graph-id))
                                                      (assoc graph-after :tx-sequence-label (:tx-sequence-label graph-before))
                                                      graph-after)
                                        system-after (if (and (has-history? system graph-id)
                                                              (meaningful-change? significantly-modified-graphs graph-id))
                                                       (remember-change system graph-id graph-before graph-after (graph-id->outputs-modified graph-id))
                                                       system)]
                                    (assoc-in system-after [:graphs graph-id] graph-after)))))
                            system
                            post-tx-graphs)]
    (-> post-system
        (update :cache c/cache-invalidate outputs-modified)
        (update :user-data (fn [user-data]
                             (reduce (fn [user-data [graph-id deleted-node-ids]]
                                       (update user-data graph-id (partial apply dissoc) deleted-node-ids))
                                     user-data
                                     (group-by gt/node-id->graph-id (keys nodes-deleted)))))
        (update :invalidate-counters bump-invalidate-counters outputs-modified))))

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
  [system {options-basis :basis options-cache :cache :as options}]
  (in/custom-evaluation-context
    (if (some? options-cache)
      (do
        (assert (some? options-basis))
        options)
      (let [system-basis (basis system)]
        (if (or (nil? options-basis)
                (basis-graphs-identical? options-basis system-basis))
          (assoc options
            :basis system-basis
            :cache (system-cache system)
            :initial-invalidate-counters (:invalidate-counters system))
          options)))))

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
    (let [invalidate-counters (:invalidate-counters system)
          evaluation-context-hits @(:hits evaluation-context)
          evaluation-context-misses @(:local evaluation-context)]
      (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
        (cond-> system
                (seq evaluation-context-hits)
                (update :cache c/cache-hit evaluation-context-hits)

                (seq evaluation-context-misses)
                (update :cache c/cache-encache evaluation-context-misses (:basis evaluation-context)))
        (let [invalidated-during-node-value? (fn [endpoint]
                                               (not= (get initial-invalidate-counters endpoint 0)
                                                     (get invalidate-counters endpoint 0)))
              safe-cache-hits (remove invalidated-during-node-value? evaluation-context-hits)
              safe-cache-misses (remove (comp invalidated-during-node-value? first) evaluation-context-misses)]
          (cond-> system
                  (seq safe-cache-hits)
                  (update :cache c/cache-hit safe-cache-hits)

                  (seq safe-cache-misses)
                  (update :cache c/cache-encache safe-cache-misses (:basis evaluation-context))))))
    system))

(defn node-value
  "Get a value, possibly cached, from a node. This is the entry point
  to the \"plumbing\". If the value is cacheable and exists in the
  cache, then return that value. Otherwise, produce the value by
  gathering inputs to call a production function, invoke the function,
  maybe cache the value that was produced, and return it."
  [system node-id label evaluation-context]
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
   :id-generators (into {}
                        (map (fn [[graph-id ^AtomicLong gen]]
                               [graph-id (AtomicLong. (.longValue gen))]))
                        (:id-generators system))
   :override-id-generator (AtomicLong. (.longValue ^AtomicLong (:override-id-generator system)))
   :cache (:cache system)
   :user-data (:user-data system)
   :invalidate-counters (:invalidate-counters system)
   :last-graph (:last-graph system)})

(defn system= [s1 s2]
  (and (= (:graphs s1) (:graphs s2))
       (= (:history s1) (:history s2))
       (= (map (fn [[graph-id ^AtomicLong gen]] [graph-id (.longValue gen)]) (:id-generators s1))
          (map (fn [[graph-id ^AtomicLong gen]] [graph-id (.longValue gen)]) (:id-generators s2)))
       (= (.longValue ^AtomicLong (:override-id-generator s1))
          (.longValue ^AtomicLong (:override-id-generator s2)))
       (= (:cache s1) (:cache s2))
       (= (:user-data s1) (:user-data s2))
       (= (:invalidate-counters s1) (:invalidate-counters s2))
       (= (:last-graph s1) (:last-graph s2))))

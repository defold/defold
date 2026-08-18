;; Copyright 2020-2026 The Defold Foundation
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
  (:require [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.paper-tape :as tape]
            [internal.transaction :as it]
            [internal.util :as util]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(declare graphs)

(def ^:private maximum-cached-items 20000)
(def ^:private maximum-undo-steps 60)
(def ^:private global-undo-key :undo/global)
(def ^:private full-invalidation-endpoint (gt/endpoint Long/MAX_VALUE ::full-invalidation))

(prefer-method print-method java.util.Map clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord clojure.lang.IDeref)

(defn- integer-counter
  []
  (AtomicLong. 0))

(defonce/record UndoState [label sequence-label undoable-changes])

(defn- merge-into-top
  [tape new-state]
  (let [old-state (tape/ivalue tape)]
    (conj
      (tape/truncate (tape/iprev tape))
      (assoc new-state
        :undoable-changes (into (:undoable-changes old-state)
                                (:undoable-changes new-state))))))

(defn- =*
  "Comparison operator that treats nil as not equal to anything."
  ([_x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn- new-undo []
  (tape/paper-tape maximum-undo-steps))

(defn maybe-undo [system undo-key]
  (-> system :undo (get undo-key)))

(defn undo-stack-revision [system undo-key]
  (if-let [undo (maybe-undo system undo-key)]
    (tape/revision undo)
    0))

(defn undo-stack-revisions [system]
  (coll/map-vals tape/revision (:undo system)))

(defn undo [system undo-key]
  (or (maybe-undo system undo-key)
      (throw
        (ex-info
          "Missing undo-key."
          {:undo-key undo-key
           :candidates (mapv key (:undo system))}))))

(defn undo-stack [undo]
  (if-not undo
    []
    (-> undo tape/before vec)))

(defn redo-stack [undo]
  (if-not undo
    []
    (-> undo tape/after vec)))

(defn- set-undo
  [system undo-key undo]
  (assoc-in system [:undo undo-key] undo))

(defn merge-or-push-undo
  [paper-tape label sequence-label undoable-changes]
  (let [new-state (->UndoState label sequence-label undoable-changes)
        tape-op (if (=* sequence-label (:sequence-label (tape/ivalue paper-tape)))
                  merge-into-top
                  conj)]
    (tape-op paper-tape new-state)))

(defn last-graph            [system]          (-> system :last-graph))
(defn system-cache          [system]          (some-> system :cache))
(defn graphs                [system]          (-> system :graphs))
(defn graph                 [system graph-id] (some-> system :graphs (get graph-id)))
(defn graph-time            [system graph-id] (some-> system :graphs (get graph-id) :tx-id))
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
  affected by them. Outputs are specified as a seq of Endpoints
  for both the argument and return value."
  [system outputs]
  (assert (coll/every? gt/endpoint? outputs))
  ;; 'dependencies' takes a map, where outputs is a vec of node-id+label pairs
  (let [basis (basis system)
        cache-entries (gt/dependencies basis outputs)]
    (-> system
        (update :cache c/cache-invalidate cache-entries)
        (update :invalidate-counters bump-invalidate-counters cache-entries))))

(defn cache-output-values
  "Write the supplied key-value pairs to the cache. Downstream endpoints will be
  invalidated if the value differs from the previously cached entry."
  [system endpoint+value-pairs]
  (let [basis (basis system)
        cache (:cache system)

        changed-endpoint+value-pairs
        (filterv (fn [[endpoint new-value]]
                   (let [old-value (get cache endpoint ::not-found)]
                     (or (= ::not-found old-value)
                         (not= old-value new-value))))
                 endpoint+value-pairs)

        invalidated-endpoints
        (gt/dependencies basis (mapv first changed-endpoint+value-pairs))]

    (-> system
        (update :invalidate-counters bump-invalidate-counters invalidated-endpoints)
        (assoc :cache (-> cache
                          (c/cache-invalidate invalidated-endpoints)
                          (c/cache-encache changed-endpoint+value-pairs basis))))))

(defn- remove-deleted-user-data
  [user-data deleted-node-ids]
  (reduce-kv (fn [user-data graph-id deleted-node-ids]
               (update user-data graph-id #(apply dissoc % deleted-node-ids)))
             user-data
             (group-by gt/node-id->graph-id deleted-node-ids)))

(defn- commit-transaction-effects
  [system outputs-modified nodes-deleted]
  (-> system
      (update :cache c/cache-invalidate outputs-modified)
      (update :user-data remove-deleted-user-data (coll/keys nodes-deleted))
      (update :invalidate-counters bump-invalidate-counters outputs-modified)))

(defn modified-graph-states
  [pre-tx-graphs post-tx-graphs]
  (coll/transform-> post-tx-graphs
    (filter (fn [[graph-id graph]]
              (not (identical? (pre-tx-graphs graph-id)
                               graph))))))

(defn- ensure-no-concurrent-modifications!
  [system modified-post-tx-graphs]
  (coll/reduce-kv-> modified-post-tx-graphs nil
    (fn [_ graph-id modified-graph]
      (let [start-tx (:tx-id modified-graph -1)
            sidereal-tx (graph-time system graph-id)]
        (when (< start-tx sidereal-tx)
          ;; graph was modified concurrently by a different transaction.
          (throw
            (ex-info
              "Concurrent modification of graph"
              {:graph-id graph-id
               :start-tx start-tx
               :sidereal-tx sidereal-tx})))))))

(defn- commit-graph-states
  [system modified-post-tx-graphs]
  (update
    system :graphs
    (fn [graphs]
      (coll/reduce-kv-> modified-post-tx-graphs graphs
        (fn [graphs graph-id modified-graph]
          (assoc graphs graph-id (update modified-graph :tx-id util/safe-inc)))))))

(defn- replay-changes
  [system transaction-changes change-fn]
  (let [ctx (it/new-transaction-context
              (basis system)
              (id-generators system)
              (override-id-generator system)
              {}
              nil
              false)
        pre-tx-graphs (it/ctx-graphs ctx)
        ctx (reduce (fn [ctx transaction-change]
                      (-> ctx
                          (change-fn transaction-change)
                          (update :completed-action-count inc)))
                    ctx
                    transaction-changes)
        {:keys [nodes-deleted outputs-modified] :as tx-result} (it/finalize-applied-changes ctx)
        post-tx-graphs (get-in tx-result [:basis :graphs])
        modified-post-tx-graphs (modified-graph-states pre-tx-graphs post-tx-graphs)]
    (ensure-no-concurrent-modifications! system modified-post-tx-graphs)
    (-> system
        (commit-graph-states modified-post-tx-graphs)
        (commit-transaction-effects outputs-modified nodes-deleted))))

(defn undo-action
  [system undo-key]
  (let [undo (undo system undo-key)
        state (tape/ivalue undo)]
    (if-not state
      system
      (-> system
          (replay-changes (rseq (:undoable-changes state)) it/revert-change)
          (set-undo undo-key (tape/iprev undo))))))

(defn redo-action
  [system undo-key]
  (let [undo (undo system undo-key)
        state (peek (tape/after undo))]
    (if-not state
      system
      (-> system
          (replay-changes (:undoable-changes state) it/perform-change)
          (set-undo undo-key (tape/inext undo))))))

(defn clear-undo
  [system undo-key]
  (let [undo (undo system undo-key)]
    (set-undo system undo-key (empty undo))))

(defn cancel-undo
  [system undo-key sequence-id]
  (let [undo (undo system undo-key)
        state (tape/ivalue undo)]
    (if (=* sequence-id (:sequence-label state))
      (-> system
          (replay-changes (rseq (:undoable-changes state)) it/revert-change)
          (set-undo undo-key (-> undo
                                 tape/drop-current
                                 tape/truncate)))
      system)))

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_graph-id 0)}}]
  graph)

(defn make-cache
  [{:keys [cache-size cache-retain?] :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size cache-retain?))

(defn- next-available-graph-id
  [system]
  (let [used (set (coll/keys (graphs system)))]
    (coll/first-where (complement used) (range 0 gt/MAX-GROUP-ID))))

(defn next-node-id
  ^long [system ^long graph-id]
  (gt/next-node-id (id-generators system) graph-id))

(defn take-node-ids*
  [id-generators ^long graph-id ^long node-id-count]
  (let [^AtomicLong id-generator (get id-generators graph-id)
        node-ids (long-array node-id-count)]
    (loop [index 0]
      (when (< index node-id-count)
        (let [node-id (gt/make-node-id graph-id (.getAndIncrement id-generator))]
          (aset node-ids index node-id)
          (recur (inc index)))))
    node-ids))

(defn take-node-ids
  [system ^long graph-id ^long node-id-count]
  (take-node-ids* (id-generators system) graph-id node-id-count))

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

(defn detach-graph
  [system graph]
  (let [graph-id (if (map? graph) (:_graph-id graph) graph)]
    (update system :graphs dissoc graph-id)))

(defn make-system
  [configuration]
  (let [initial-graph (make-initial-graph configuration)
        cache (make-cache configuration)]
    (-> {:graphs {}
         :undo {global-undo-key (new-undo)}
         :id-generators {}
         :override-id-generator (integer-counter)
         :cache cache
         :invalidate-counters {}
         :user-data {}}
        (attach-graph initial-graph))))

(defn- register-undoable-changes
  [system undo-key label sequence-label undoable-changes]
  (if (coll/empty? undoable-changes)
    system
    (let [undo (or (maybe-undo system undo-key) (new-undo))
          undo (merge-or-push-undo undo label sequence-label undoable-changes)]
      (set-undo system undo-key undo))))

(defn merge-graphs
  [system modified-post-tx-graphs outputs-modified nodes-deleted undo-key label sequence-label undoable-changes full-invalidation]
  (ensure-no-concurrent-modifications! system modified-post-tx-graphs)
  (-> system
      (register-undoable-changes undo-key label sequence-label undoable-changes)
      (commit-graph-states modified-post-tx-graphs)
      (commit-transaction-effects outputs-modified nodes-deleted)
      (cond-> full-invalidation
        (-> (update :cache c/cache-clear)
            (update :invalidate-counters update full-invalidation-endpoint util/safe-inc)))))

(defn basis-graphs-identical?
  [basis1 basis2]
  (let [graphs1 (:graphs basis1)
        graphs2 (:graphs basis2)]
    (or (identical? graphs1 graphs2)
        (and (= (count graphs1) (count graphs2))
             (coll/reduce-kv-> graphs1 true
               (fn [_ graph-id graph]
                 (if (identical? graph (get graphs2 graph-id))
                   true
                   (reduced false))))))))

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

(defn evaluation-context-invalidate-counters [evaluation-context]
  (if-let [invalidate-counters (:initial-invalidate-counters evaluation-context)]
    invalidate-counters
    (throw (IllegalArgumentException. "The evaluation-context does not have :initial-invalidate-counters."))))

(defn invalidate-counters [system]
  (if-let [invalidate-counters (:invalidate-counters system)]
    invalidate-counters
    (throw (IllegalArgumentException. "The argument is not a valid system."))))

(defn full-invalidation-since?
  [snapshot-invalidate-counters system-invalidate-counters]
  (not= (long (get snapshot-invalidate-counters full-invalidation-endpoint 0))
        (long (get system-invalidate-counters full-invalidation-endpoint 0))))

(definline endpoint-invalidated-since? [endpoint snapshot-invalidate-counters system-invalidate-counters]
  `(not= (long (get ~snapshot-invalidate-counters ~endpoint 0))
         (long (get ~system-invalidate-counters ~endpoint 0))))

(defn update-cache-from-evaluation-context
  [system evaluation-context]
  {:pre [(some? system)]}
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
  (if-let [initial-invalidate-counters (:initial-invalidate-counters evaluation-context)]
    (let [invalidate-counters (:invalidate-counters system)]
      (if (and (not (identical? initial-invalidate-counters invalidate-counters))
               (full-invalidation-since? initial-invalidate-counters invalidate-counters))
        system
        (let [system-basis (basis system)
              evaluation-context-hits @(:hits evaluation-context)
              evaluation-context-misses @(:local evaluation-context)]
          (if (and (identical? invalidate-counters initial-invalidate-counters)
                   (basis-graphs-identical? system-basis (:basis evaluation-context))) ; nice case
            (cond-> system
              (coll/not-empty evaluation-context-hits)
              (update :cache c/cache-hit evaluation-context-hits)

              (coll/not-empty evaluation-context-misses)
              (update :cache c/cache-encache evaluation-context-misses (:basis evaluation-context)))
            (let [unsafe-cache-entry? (fn [endpoint]
                                        (or (nil? (gt/node-by-id-at system-basis (gt/endpoint-node-id endpoint)))
                                            (endpoint-invalidated-since? endpoint initial-invalidate-counters invalidate-counters)))
                  safe-cache-hits (coll/into-> evaluation-context-hits []
                                    (remove unsafe-cache-entry?))
                  safe-cache-misses (coll/into-> evaluation-context-misses []
                                      (remove (comp unsafe-cache-entry? first)))]
              (cond-> system
                (coll/not-empty safe-cache-hits)
                (update :cache c/cache-hit safe-cache-hits)

                (coll/not-empty safe-cache-misses)
                (update :cache c/cache-encache safe-cache-misses (:basis evaluation-context))))))))
    system))

(defn user-data [system node-id key]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (-> system :user-data (get graph-id) (get node-id) (get key))))

(defn assoc-user-data [system node-id key value]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (update system :user-data update graph-id update node-id assoc key value)))

(defn update-user-data [system node-id key f & args]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (update-in system [:user-data graph-id node-id key] #(apply f %1 %2) args)))

(defn merge-user-data [system values-by-key-by-node-id]
  (assoc system
    :user-data (reduce (fn [user-data [graph-id values-by-key-by-node-id]]
                         (assoc user-data
                           graph-id (reduce (fn [graph-user-data [node-id values-by-key]]
                                              (update graph-user-data node-id coll/merge values-by-key))
                                            (get user-data graph-id)
                                            values-by-key-by-node-id)))
                       (:user-data system)
                       (group-by (fn [[node-id]]
                                   (gt/node-id->graph-id node-id))
                                 values-by-key-by-node-id))))

(defn clone-system [system]
  {:graphs (:graphs system)
   :undo (:undo system)
   :id-generators (into {}
                        (map (fn [[graph-id ^AtomicLong gen]]
                               [graph-id (AtomicLong. (.longValue gen))]))
                        (:id-generators system))
   :override-id-generator (AtomicLong. (.longValue ^AtomicLong (:override-id-generator system)))
   :cache (:cache system)
   :user-data (:user-data system)
   :invalidate-counters (:invalidate-counters system)
   :last-graph (:last-graph system)})

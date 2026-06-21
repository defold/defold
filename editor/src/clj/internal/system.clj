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
            [internal.transaction :as it]
            [internal.paper-tape :as tape]
            [internal.util :as util]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(declare graphs)

(def ^:private maximum-cached-items 20000)
(def ^:private maximum-undo-steps 60)
(def ^:private global-undo-key :undo/global)

(prefer-method print-method java.util.Map clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord clojure.lang.IDeref)

(defn- integer-counter
  []
  (AtomicLong. 0))

(defonce/record UndoState [label sequence-label changes])

(defn- merge-into-top
  [tape new-state]
  (let [old-state (tape/ivalue tape)]
    (conj
      (tape/truncate (tape/iprev tape))
      (assoc new-state :changes (into (:changes old-state) (:changes new-state))))))

(defn- =*
  "Comparison operator that treats nil as not equal to anything."
  ([_x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn- new-undo []
  (tape/paper-tape maximum-undo-steps))

(defn merge-or-push-undo
  [paper-tape label sequence-label changes]
  (let [new-state (->UndoState label sequence-label changes)
        tape-op (if (=* sequence-label (:sequence-label (tape/ivalue paper-tape)))
                  merge-into-top
                  conj)]
    (tape-op paper-tape new-state)))

(defn undo-stack [undo]
  (-> undo tape/before vec))

(defn last-graph [system] (-> system :last-graph))
(defn system-cache [system] (some-> system :cache))
(defn graphs [system] (-> system :graphs))
(defn graph [system graph-id] (some-> system :graphs (get graph-id)))
(defn graph-time [system graph-id] (some-> system :graphs (get graph-id) :tx-id))
(defn undo [system undo-key] (-> system :undo (get undo-key)))
(defn basis [system] (ig/multigraph-basis (:graphs system)))
(defn id-generators [system] (-> system :id-generators))
(defn override-id-generator [system] (-> system :override-id-generator))

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
  (assert (every? gt/endpoint? outputs))
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
      (update :user-data remove-deleted-user-data (keys nodes-deleted))
      (update :invalidate-counters bump-invalidate-counters outputs-modified)))

(defn- commit-graph-states
  [system post-tx-graphs]
  (reduce-kv (fn [system graph-id graph]
               (assoc-in system [:graphs graph-id] (update graph :tx-id util/safe-inc)))
             system
             post-tx-graphs))

(defn- replay-undo
  [system changes change-fn]
  (let [ctx (it/new-transaction-context
              (basis system)
              (id-generators system)
              (override-id-generator system)
              {}
              nil
              false)
        ctx (reduce (fn [ctx change]
                      (-> ctx
                          (change-fn change)
                          (update :completed-action-count inc)))
                    ctx
                    changes)
        tx-result (it/finalize-applied-changes ctx)]
    (-> system
        (commit-graph-states (get-in tx-result [:basis :graphs]))
        (commit-transaction-effects (:outputs-modified tx-result)
                                    (:nodes-deleted tx-result)))))

(defn- update-undo
  [system undo-key undo]
  (assoc-in system [:undo undo-key] undo))

(defn undo-action
  [system undo-key]
  (let [undo (undo system undo-key)]
    (if-let [state (tape/ivalue undo)]
      (-> system
          (replay-undo (rseq (:changes state)) it/revert-change)
          (update-undo undo-key (tape/iprev undo)))
      system)))

(defn redo-action
  [system undo-key]
  (let [undo (undo system undo-key)]
    (if-let [state (clojure.core/peek (tape/after undo))]
      (-> system
          (replay-undo (:changes state) it/perform-change)
          (update-undo undo-key (tape/inext undo)))
      system)))

(defn redo-stack [undo]
  (-> undo tape/after vec))

(defn clear-undo
  [system undo-key]
  (if-let [undo (undo system undo-key)]
    (update-undo system undo-key (empty undo))
    system))

(defn cancel-undo
  [system undo-key sequence-id]
  (let [undo (undo system undo-key)
        state (tape/ivalue undo)]
    (if (=* sequence-id (:sequence-label state))
      (-> system
          (replay-undo (rseq (:changes state)) it/revert-change)
          (update-undo undo-key (tape/drop-current undo)))
      system)))

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

(defn- remember-change
  [system label sequence-label changes]
  (update-in system [:undo global-undo-key] merge-or-push-undo label sequence-label changes))

(defn- prepare-transaction-graphs
  [system post-tx-graphs]
  (reduce-kv (fn [post-tx-graphs graph-id graph]
               (let [start-tx (:tx-id graph -1)
                     sidereal-tx (graph-time system graph-id)]
                 (when (< start-tx sidereal-tx)
                   ;; graph was modified concurrently by a different transaction.
                   (throw (ex-info "Concurrent modification of graph"
                                   {:_graph-id graph-id :start-tx start-tx :sidereal-tx sidereal-tx})))
                 (assoc post-tx-graphs graph-id graph)))
             {}
             post-tx-graphs))

(defn- remember-transaction-changes
  [system is-undo-significant changes label sequence-label]
  (cond-> system
          (and is-undo-significant
               (coll/not-empty changes))
          (remember-change label sequence-label changes)))

(defn merge-graphs
  [system post-tx-graphs is-undo-significant outputs-modified nodes-deleted changes label sequence-label]
  (let [post-tx-graphs (prepare-transaction-graphs system post-tx-graphs)]
    (-> system
        (remember-transaction-changes is-undo-significant changes label sequence-label)
        (commit-graph-states post-tx-graphs)
        (commit-transaction-effects outputs-modified nodes-deleted))))

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

(defn evaluation-context-invalidate-counters [evaluation-context]
  (if-let [invalidate-counters (:initial-invalidate-counters evaluation-context)]
    invalidate-counters
    (throw (IllegalArgumentException. "The evaluation-context does not have :initial-invalidate-counters."))))

(defn invalidate-counters [system]
  (if-let [invalidate-counters (:invalidate-counters system)]
    invalidate-counters
    (throw (IllegalArgumentException. "The argument is not a valid system."))))

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
  (if-some [initial-invalidate-counters (:initial-invalidate-counters evaluation-context)]
    (let [invalidate-counters (:invalidate-counters system)
          evaluation-context-hits @(:hits evaluation-context)
          evaluation-context-misses @(:local evaluation-context)]
      (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
        (cond-> system
                (coll/not-empty evaluation-context-hits)
                (update :cache c/cache-hit evaluation-context-hits)

                (coll/not-empty evaluation-context-misses)
                (update :cache c/cache-encache evaluation-context-misses (:basis evaluation-context)))
        (let [invalidated-during-node-value? #(endpoint-invalidated-since? % initial-invalidate-counters invalidate-counters)
              safe-cache-hits (remove invalidated-during-node-value? evaluation-context-hits)
              safe-cache-misses (remove (comp invalidated-during-node-value? first) evaluation-context-misses)]
          (cond-> system
                  (coll/not-empty safe-cache-hits)
                  (update :cache c/cache-hit safe-cache-hits)

                  (coll/not-empty safe-cache-misses)
                  (update :cache c/cache-encache safe-cache-misses (:basis evaluation-context))))))
    system))

(defn user-data [system node-id key]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (get-in (:user-data system) [graph-id node-id key])))

(defn assoc-user-data [system node-id key value]
  (let [graph-id (gt/node-id->graph-id node-id)]
    (assoc-in system [:user-data graph-id node-id key] value)))

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

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
  (:import [java.util ConcurrentModificationException]
           [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

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

(defn system-cache          [system]          (some-> system :cache))

(defn basis [system] (:graph system))
(defn node-id-generator     [system]          (-> system :node-id-generator))
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
        cache-entries (ig/dependencies basis outputs)]
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
        (ig/dependencies basis (mapv first changed-endpoint+value-pairs))]

    (-> system
        (update :invalidate-counters bump-invalidate-counters invalidated-endpoints)
        (assoc :cache (-> cache
                          (c/cache-invalidate invalidated-endpoints)
                          (c/cache-encache changed-endpoint+value-pairs basis))))))

(defn- remove-deleted-user-data
  [user-data deleted-node-ids]
  (reduce dissoc user-data deleted-node-ids))

(defn- commit-transaction-effects
  [system outputs-modified nodes-deleted]
  (-> system
      (update :cache c/cache-invalidate outputs-modified)
      (update :user-data remove-deleted-user-data (coll/keys nodes-deleted))
      (update :invalidate-counters bump-invalidate-counters outputs-modified)))

(defn- ensure-no-concurrent-modifications!
  [system pre-tx-basis post-tx-basis]
  (when-not (identical? pre-tx-basis post-tx-basis)
    (let [start-tx (gt/tx-id post-tx-basis)
          current-tx (gt/tx-id (basis system))]
      (when (< start-tx current-tx)
        (throw (ConcurrentModificationException.
                 (format "Concurrent modification of graph: transaction revision %s, current revision %s"
                         start-tx current-tx)))))))

(defn- commit-basis
  [system pre-tx-basis post-tx-basis]
  (if (identical? pre-tx-basis post-tx-basis)
    system
    (assoc system :graph (update post-tx-basis :tx-id util/safe-inc))))

(defn- replay-changes
  [system transaction-changes change-fn]
  (let [ctx (it/new-transaction-context
              (basis system)
              (node-id-generator system)
              (override-id-generator system)
              {}
              nil
              false)
        pre-tx-basis (:basis ctx)
        ctx (reduce (fn [ctx transaction-change]
                      (-> ctx
                          (change-fn transaction-change)
                          (update :completed-action-count inc)))
                    ctx
                    transaction-changes)
        {:keys [nodes-deleted outputs-modified] :as tx-result} (it/finalize-applied-changes ctx)
        post-tx-basis (:basis tx-result)]
    (ensure-no-concurrent-modifications! system pre-tx-basis post-tx-basis)
    (-> system
        (commit-basis pre-tx-basis post-tx-basis)
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

(defn make-cache
  [{:keys [cache-size cache-retain?] :or {cache-size maximum-cached-items}}]
  (c/make-cache cache-size cache-retain?))

(defn next-node-id
  ^long [system]
  (gt/next-node-id (node-id-generator system)))

(defn take-node-ids
  [system ^long node-id-count]
  (let [^AtomicLong node-id-generator (node-id-generator system)
        node-ids (long-array node-id-count)]
    (loop [index 0]
      (when (< index node-id-count)
        (aset node-ids index (.getAndIncrement node-id-generator))
        (recur (inc index))))
    node-ids))

(defn make-system
  [configuration]
  {:graph (or (:initial-graph configuration) (ig/empty-graph))
   :undo {global-undo-key (new-undo)}
   :node-id-generator (integer-counter)
   :override-id-generator (integer-counter)
   :cache (make-cache configuration)
   :invalidate-counters {}
   :user-data {}})

(defn- register-undoable-changes
  [system undo-key label sequence-label undoable-changes]
  (if (coll/empty? undoable-changes)
    system
    (let [undo (or (maybe-undo system undo-key) (new-undo))
          undo (merge-or-push-undo undo label sequence-label undoable-changes)]
      (set-undo system undo-key undo))))

(defn merge-basis
  [system pre-tx-basis post-tx-basis outputs-modified nodes-deleted undo-key label sequence-label undoable-changes full-invalidation]
  (ensure-no-concurrent-modifications! system pre-tx-basis post-tx-basis)
  (-> system
      (register-undoable-changes undo-key label sequence-label undoable-changes)
      (commit-basis pre-tx-basis post-tx-basis)
      (commit-transaction-effects outputs-modified nodes-deleted)
      (cond-> full-invalidation
        (-> (update :cache c/cache-clear)
            (update :invalidate-counters update full-invalidation-endpoint util/safe-inc)))))

(defn default-evaluation-context [system]
  (in/default-evaluation-context (basis system)
                                 (system-cache system)
                                 (:invalidate-counters system)))

(defn custom-evaluation-context
  ;; Basis & cache options:
  ;;  * only supplying a cache makes no sense and is a programmer error
  ;;  * if neither is supplied, use from system
  ;;  * if only given basis it's not at all certain that system cache is
  ;;    derived from the given basis. One safe case is if the
  ;;    basis is identical to the system basis. If so, we use the system cache.
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
                (identical? options-basis system-basis))
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
        (let [evaluation-context-hits @(:hits evaluation-context)
              evaluation-context-misses @(:local evaluation-context)]
          (if (identical? invalidate-counters initial-invalidate-counters) ; nice case
            (cond-> system
              (coll/not-empty evaluation-context-hits)
              (update :cache c/cache-hit evaluation-context-hits)

              (coll/not-empty evaluation-context-misses)
              (update :cache c/cache-encache evaluation-context-misses (:basis evaluation-context)))
            (let [invalidated-during-node-value? #(endpoint-invalidated-since? % initial-invalidate-counters invalidate-counters)
                  safe-cache-hits (coll/into-> evaluation-context-hits []
                                    (remove invalidated-during-node-value?))
                  safe-cache-misses (coll/into-> evaluation-context-misses []
                                      (remove (comp invalidated-during-node-value? first)))]
              (cond-> system
                (coll/not-empty safe-cache-hits)
                (update :cache c/cache-hit safe-cache-hits)

                (coll/not-empty safe-cache-misses)
                (update :cache c/cache-encache safe-cache-misses (:basis evaluation-context))))))))
    system))

(defn user-data [system node-id key]
  (get-in system [:user-data node-id key]))

(defn assoc-user-data [system node-id key value]
  (assoc-in system [:user-data node-id key] value))

(defn update-user-data [system node-id key f & args]
  (update-in system [:user-data node-id key] #(apply f %1 %2) args))

(defn merge-user-data [system values-by-key-by-node-id]
  (update system :user-data
          (fn [user-data]
            (reduce-kv (fn [user-data node-id values-by-key]
                         (update user-data node-id coll/merge values-by-key))
                       user-data
                       values-by-key-by-node-id))))

(defn clone-system [system]
  {:graph (:graph system)
   :undo (:undo system)
   :node-id-generator (AtomicLong. (.longValue ^AtomicLong (:node-id-generator system)))
   :override-id-generator (AtomicLong. (.longValue ^AtomicLong (:override-id-generator system)))
   :cache (:cache system)
   :user-data (:user-data system)
   :invalidate-counters (:invalidate-counters system)})

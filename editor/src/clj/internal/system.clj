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
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as h]
            [internal.node :as in]
            [service.log :as log]
            [internal.cache2 :as c2])
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

(defrecord HistoryState [label graph sequence-label])

(defn history-state [graph]
  (->HistoryState (:tx-label graph) graph (:tx-sequence-label graph)))

(defn- merge-into-top
  [tape new-state]
  (conj
    (h/truncate (h/iprev tape))
    new-state))

(defn- =*
  "Comparison operator that treats nil as not equal to anything."
  ([x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn merge-or-push-history
  [history old-graph new-graph]
  (let [new-state (history-state new-graph)
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

(defn- time-warp [system graph]
  (let [graph-id (:_graph-id graph)
        graphs (graphs system)]
    (let [pseudo-basis (ig/multigraph-basis graphs)
          hydrated-basis (ig/hydrate-after-undo pseudo-basis graph)]
      (get-in hydrated-basis [:graphs graph-id]))))

(defn last-graph            [system]          (-> system :last-graph))
(defn system-cache          [system]          (-> system :cache))
(defn graphs                [system]          (-> system :graphs))
(defn graph                 [system graph-id] (some-> system :graphs (get graph-id)))
(defn graph-time            [system graph-id] (some-> system :graphs (get graph-id) :tx-id))
(defn graph-history         [system graph-id] (-> system :history (get graph-id)))
(defn basis                 [system]          (ig/multigraph-basis (:graphs system)))
(defn id-generators         [system]          (-> system :id-generators))
(defn override-id-generator [system]          (-> system :override-id-generator))

(defn- step-through-history
  [step-function system graph-id]
  (let [history (graph-history system graph-id)
        {:keys [tape]} history
        tape (step-function tape)
        next-state (h/ivalue tape)]
    (if-not next-state
      system
      (let [graph (time-warp system (:graph next-state))]
        (-> system
            (assoc-in [:graphs graph-id] graph)
            (assoc-in [:history graph-id :tape] tape))))))

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
        initial-state (history-state graph)]
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
  (let [initial-graph (make-initial-graph configuration)]
    (-> {:graphs {}
         :id-generators {}
         :override-id-generator (integer-counter)
         :cache (c2/make (:cache-size configuration maximum-cached-items))
         :invalidate-counters {}
         :user-data {}}
        (attach-graph initial-graph))))

(defn- has-history? [system graph-id] (not (nil? (graph-history system graph-id))))
(def ^:private meaningful-change? contains?)

(defn- remember-change
  [system graph-id before after]
  (update-in system [:history graph-id] merge-or-push-history before after))

(defn merge-graphs
  [system post-tx-graphs significantly-modified-graphs outputs-modified nodes-deleted]
  (let [post-system (reduce (fn [system [graph-id graph]]
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
                                                       (remember-change system graph-id graph-before graph-after)
                                                       system)]
                                    (assoc-in system-after [:graphs graph-id] graph-after)))))
                            system
                            post-tx-graphs)]
    (-> post-system
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
                                 (system-cache system)))

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
                        :cache (system-cache system)}
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
  system)

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
       (= (:last-graph s1) (:last-graph s2))))

(ns dynamo.system
  "Functions for performing transactional changes to the system graph."
  (:require [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.cache :as c]
            [internal.disposal :as dispose]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [internal.transaction :as it]
            [potemkin.namespaces :refer [import-vars]]))

;; Only marked dynamic so tests can rebind. Should never be rebound "for real".
(defonce ^:dynamic *the-system* (atom nil))

(import-vars [internal.system system-cache disposal-queue])

;; ---------------------------------------------------------------------------
;; Interrogating the Graph
;; ---------------------------------------------------------------------------
(defn now
  []
  (is/basis @*the-system*))

;; ---------------------------------------------------------------------------
;; Transactional state
;; ---------------------------------------------------------------------------
(defn- merge-graphs [sys basis tx-graphs gids]
  (doseq [gid  gids
          :let [graph (get tx-graphs gid)]]
    (let [start-tx    (:tx-id graph -1)
          sidereal-tx (is/graph-time sys gid)]
      (if (< start-tx sidereal-tx)
        ;; graph was modified concurrently by a different transaction.
        (throw (ex-info "Concurrent modification of graph"
                        {:_gid gid :start-tx start-tx :sidereal-tx sidereal-tx}))
        (ref-set (is/graph-ref sys gid) (update-in graph [:tx-id] safe-inc))))))

(defn- graphs-touched
  [tx-result]
  (distinct
   (map #(gt/nref->gid (first %))
        (concat
         (:outputs-modified tx-result)
         (:properties-modified tx-result)))))

(defn transact
  ([txs]
   (transact *the-system* txs))
  ([sys-ref txs]
   (let [basis     (ig/multigraph-basis (map-vals deref (is/graphs @sys-ref)))
         tx-result (it/transact* (it/new-transaction-context basis txs))]
     (when (= :ok (:status tx-result))
       (dosync
        (merge-graphs @sys-ref basis (get-in tx-result [:basis :graphs]) (graphs-touched tx-result)))
       (c/cache-invalidate (is/system-cache @sys-ref) (:outputs-modified tx-result))
       (doseq [l (:old-event-loops tx-result)]
         (is/stop-event-loop! @sys-ref l))
       (doseq [l (:new-event-loops tx-result)]
         (is/start-event-loop! @sys-ref l))
       (doseq [d (vals (:nodes-deleted tx-result))]
         (is/dispose! @sys-ref d)))
     tx-result)))

;; ---------------------------------------------------------------------------
;; Using transaction results
;; ---------------------------------------------------------------------------
(defn tx-nodes-added
  [{:keys [basis nodes-added]}]
  (map (partial gt/node-by-id basis) nodes-added))

;; ---------------------------------------------------------------------------
;; For use by triggers
;; ---------------------------------------------------------------------------
(defn is-modified?
  ([transaction node]
    (boolean (contains? (:outputs-modified transaction) (gt/node-id node))))
  ([transaction node output]
    (boolean (get-in transaction [:outputs-modified (gt/node-id node) output]))))

(defn is-added? [transaction node]
  (contains? (:nodes-added transaction) (gt/node-id node)))

(defn is-deleted? [transaction node]
  (contains? (:nodes-deleted transaction) (gt/node-id node)))

(defn outputs-modified
  [transaction node]
  (get-in transaction [:outputs-modified (gt/node-id node)]))

(defn transaction-basis
  [transaction]
  (:basis transaction))

;; ---------------------------------------------------------------------------
;; Boot, initialization, and facade
;; ---------------------------------------------------------------------------
(defn cache [] (is/system-cache @*the-system*))

(defn graph [gid] (is/graph @*the-system* gid))

(defn initialize
  [config]
  (reset! *the-system* (is/make-system config)))

(defn undo
  []
  (is/undo-history (is/history @*the-system*)))

(defn redo
  []
  (is/redo-history (is/history @*the-system*)))

(defn dispose-pending
  []
  (dispose/dispose-pending (is/disposal-queue @*the-system*)))

(defn cache-invalidate
  "Uses the system’s cache.

  Atomic action to invalidate the given collection of [node-id label]
  pairs. If nothing is cached for a pair, it is ignored."
  [pairs]
  (c/cache-invalidate (cache) pairs))

(defn cache-encache
  "Uses the system’s cache.

  Atomic action to record one or more items in cache.

  The collection must contain tuples of [[node-id label] value]."
  [coll]
  (c/cache-encache (cache) coll))

(defn cache-hit
  "Uses the system’s cache.

  Atomic action to record hits on cached items

  The collection must contain tuples of [node-id label] pairs."
  [coll]
  (c/cache-hit (cache) coll))

(defn cache-snapshot
  "Get a value of the cache at a point in time."
  []
  (c/cache-snapshot (cache)))

(defn node-value
  ([node label]
   (in/node-value (now) (cache) (gt/node-id node) label)))

(defn attach-graph
  [graph]
  (let [s (swap! *the-system* is/attach-graph graph)]
    (some-> s :graphs (get (:last-graph s)) deref :_gid)))

(defn attach-graph-with-history
  [graph]
  (let [s (swap! *the-system* is/attach-graph-with-history graph)]
    (some-> s :graphs (get (:last-graph s)) deref :_gid)))

(defn last-graph-added
  []
  (is/last-graph @*the-system*))

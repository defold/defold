(ns internal.system
  (:require [clojure.core.async :as a]
            [dynamo.util :as util]
            [dynamo.util :refer :all]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [service.log :as log]))

(def ^:private maximum-cached-items     10000)
(def ^:private maximum-disposal-backlog 2500)
(def ^:private history-size-min         50)
(def ^:private history-size-max         60)

(prefer-method print-method java.util.Map               clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord        clojure.lang.IDeref)

(defn- subscriber-id
  [n]
  (if (number? n)
    n
    (:_id n)))

(defn- address-to
  [node body]
  (assoc body ::node-id (subscriber-id node)))

(defn- publish
  [{publish-to :publish-to} msg]
  (a/put! publish-to msg))

(defn- publish-all
  [sys msgs]
  (doseq [s msgs]
    (publish sys s)))

(defn- subscribe
  [{subscribe-to :subscribe-to} node]
  (a/sub subscribe-to (subscriber-id node) (a/chan 100)))

(defn- make-bus
  []
  (let [pubch (a/chan 100)]
    {:publish-to   pubch
     :subscribe-to (a/pub pubch ::node-id (fn [_] (a/dropping-buffer 100)))}))

(defn evict-obsoletes
  [cache _ world-state _ new-world-state]
  (let [obsoletes (-> new-world-state :last-tx :outputs-modified)]
    (c/cache-invalidate cache obsoletes)))

(defn- new-history
  [state]
  {:state state
   :undo-stack []
   :redo-stack []})

(defn- transaction-applied?
  [{old-world-time :tx-id} {new-world-time :tx-id :as new-world}]
  (and (= :ok (-> new-world :last-tx :status)) (< old-world-time new-world-time)))

(def conj-undo-stack (partial util/push-with-size-limit history-size-min history-size-max))

(defn- history-label [world]
  (or (get-in world [:last-tx :label]) (str "World Time: " (:tx-id world))))

(defn- push-history [history-ref  _ world-ref old-world new-world]
  (when (transaction-applied? old-world new-world)
    (dosync
      (assert (= (:state @history-ref) world-ref))
      (alter history-ref update-in [:undo-stack] conj-undo-stack old-world)
      (alter history-ref assoc-in  [:redo-stack] []))
    #_(record-history-operation undo-context (history-label new-world))))

(defn undo-history [history-ref]
  (dosync
    (let [world-ref (:state @history-ref)
          old-world @world-ref
          new-world (peek (:undo-stack @history-ref))]
      (when new-world
        (ref-set world-ref (dissoc new-world :last-tx))
        (alter history-ref update-in [:undo-stack] pop)
        (alter history-ref update-in [:redo-stack] conj old-world)))))

(defn redo-history [history-ref]
  (dosync
    (let [world-ref (:state @history-ref)
          old-world @world-ref
          new-world (peek (:redo-stack @history-ref))]
      (when new-world
        (ref-set world-ref (dissoc new-world :last-tx))
        (alter history-ref update-in [:undo-stack] conj old-world)
        (alter history-ref update-in [:redo-stack] pop)))))

(defn- make-disposal-queue
  [{queue :disposal-queue :or {queue (a/chan (a/dropping-buffer maximum-disposal-backlog))}}]
  queue)

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (ig/empty-graph)}}]
  graph)

(defn- make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}} disposal-queue]
  (c/cache-subsystem cache-size disposal-queue))

(defn make-system
  [configuration]
  (let [disposal-queue (make-disposal-queue configuration)
        initial-graph  (make-initial-graph configuration)
        cache          (make-cache configuration disposal-queue)
        world-ref      (ref nil)]
    (dosync
     (ref-set world-ref
              (ig/map-nodes #(assoc % :world-ref world-ref) initial-graph)))
    (merge (make-bus)
           {:disposal-queue disposal-queue
            :history        (ref (new-history world-ref))
            :world          world-ref
            :cache          cache})))

(defn system-cache   [s] (-> s :cache))
(defn world-ref      [s] (-> s :world))
(defn history        [s] (-> s :history))
(defn world-graph    [s] (some-> (world-ref s) deref))
(defn world-time     [s] (some-> (world-graph s) :tx-id))
(defn disposal-queue [s] (-> s :disposal-queue))

(defn start-system
  [sys]
  (add-watch (world-ref sys) ::decache      (partial evict-obsoletes (system-cache sys)))
  (add-watch (world-ref sys) ::push-history (partial push-history (history sys)))
  sys)

(defn stop-system
  [sys]
  (remove-watch (world-ref sys) ::push-history)
  (remove-watch (world-ref sys) ::decache)
  sys)

(defn start-event-loop!
  [sys id]
  (let [in (subscribe sys id)]
    (a/go-loop []
      (when-let [msg (a/<! in)]
        (when (not= ::stop-event-loop (:type msg))
          (when-let [n (ig/node (world-graph sys) id)]
            (log/logging-exceptions
             (str "Node " id "event loop")
             (gt/process-one-event n msg))
            (recur)))))))

(defn stop-event-loop!
  [sys {:keys [_id]}]
  (publish sys (address-to _id {:type ::stop-event-loop})))

(defn dispose!
  [sys node]
  (a/>!! (disposal-queue sys) node))

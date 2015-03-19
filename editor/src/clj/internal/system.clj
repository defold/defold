(ns internal.system
  (:require [clojure.core.async :as a]
            [com.stuartsierra.component :as component]
            [dynamo.util :as util]
            [dynamo.util :refer :all]
            [internal.bus :as bus]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [service.log :as log]))

(def ^:private maximum-cached-items     10000)
(def ^:private maximum-disposal-backlog 2500)

(prefer-method print-method java.util.Map               clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord        clojure.lang.IDeref)

(defn graph [world-ref]
  (-> world-ref deref :graph))

(defn new-world-state
  [initial-graph disposal-queue]
  {:graph               initial-graph
   :world-time          0
   :message-bus         (bus/make-bus)
   :disposal-queue      disposal-queue})

(defn- new-history
  [state]
  {:state state
   :undo-stack []
   :redo-stack []})

(defrecord World [state history]
  component/Lifecycle
  (start [this]
    (assoc this :started? true))

  (stop [this]
    (assoc this :started? false)))

(defn- transaction-applied?
  [{old-world-time :world-time} {new-world-time :world-time :as new-world}]
  (and (= :ok (-> new-world :last-tx :status)) (< old-world-time new-world-time)))

(def history-size-min 50)
(def history-size-max 60)
(def conj-undo-stack (partial util/push-with-size-limit history-size-min history-size-max))

(defn- history-label [world]
  (or (get-in world [:last-tx :label]) (str "World Time: " (:world-time world))))

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

(defn world
  [initial-graph disposal-queue]
  (let [world-ref      (ref nil)
        injected-graph (ig/map-nodes #(assoc % :world-ref world-ref) initial-graph)
        history-ref    (ref (new-history world-ref))]
    (dosync (ref-set world-ref (new-world-state injected-graph disposal-queue)))
    (add-watch world-ref :push-history (partial push-history history-ref))
    (map->World {:state        world-ref
                 :history      history-ref})))

(defn make-system
 [configuration]
 (let [disposal-queue (a/chan (a/dropping-buffer maximum-disposal-backlog))]
   (component/map->SystemMap
    {:world     (world (:initial-graph configuration) disposal-queue)
     :cache     (component/using (c/cache-subsystem maximum-cached-items disposal-queue) [:world])})))

(defn start-system
  [sys]
  (let [system-map (component/start sys)
        state      (-> system-map :world :state)
        graph      (-> state deref :graph)
        root       (ig/node graph 1)]

    system-map))

(defn stop-system
  [sys]
  (component/stop-system sys))

(defn system-cache   [s] (-> s :cache))
(defn world-ref      [s] (-> s :world :state))
(defn history        [s] (-> s :world :history))
(defn world-graph    [s] (-> (world-ref s) deref :graph))
(defn world-time     [s] (-> (world-ref s) deref :world-time))
(defn disposal-queue [s] (-> (world-ref s) deref :disposal-queue))
(defn message-bus    [s] (-> (world-ref s) deref :message-bus))

(defn start-event-loop!
  [sys id]
  (let [in (bus/subscribe (message-bus sys) id)]
    (a/go-loop []
      (when-let [msg (a/<! in)]
        (when (not= ::stop-event-loop (:type msg))
          (when-let [n (ig/node (world-graph sys) id)]
            (log/logging-exceptions (str "Node " id "event loop")
                                    (gt/process-one-event n msg))
            (recur)))))))

(defn stop-event-loop!
  [sys {:keys [_id]}]
  (bus/publish (message-bus sys) (bus/address-to _id {:type ::stop-event-loop})))

(defn dispose!
  [sys node]
  (a/>!! (disposal-queue sys) node))

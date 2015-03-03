(ns internal.system
  (:require [clojure.core.async :as a]
            [com.stuartsierra.component :as component]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :as util]
            [internal.bus :as bus]
            [internal.cache :as c]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.node :as in]
            [internal.refresh :refer [refresh-message refresh-subsystem]]
            [internal.repaint :as repaint]
            [internal.transaction :as it]
            [schema.core :as s]
            [service.log :as log :refer [logging-exceptions]]))

(def ^:private maximum-cached-items     10000)
(def ^:private maximum-disposal-backlog 2500)

(prefer-method print-method java.util.Map clojure.lang.IDeref)

(defn graph [world-ref]
  (-> world-ref deref :graph))

(defn- attach-root
  [g r]
  (lg/add-labeled-node g (t/inputs r) (t/outputs r) r))

(defn new-world-state
  [state root repaint-needed disposal-queue]
  {:graph               (attach-root (dg/empty-graph) root)
   :cache               (c/make-cache) ;; TODO - remove once the new component is in use
   :cache-keys          {}
   :world-time          0
   :message-bus         (bus/make-bus)
   :disposal-queue      disposal-queue
   :repaint-needed      repaint-needed})

(defn- new-history
  [state repaint-needed]
  {:state state
   :repaint-needed repaint-needed
   :undo-stack []
   :redo-stack []})

(defrecord World [started state history undo-context repaint-needed disposal-queue]
  component/Lifecycle
  (start [this]
    (if (:started this)
      this
      (dosync
        (let [root (n/construct n/RootScope :world-ref state :_id 1)]
          (ref-set state (new-world-state state root repaint-needed disposal-queue))
          (ref-set history (new-history state repaint-needed))
          (assoc this :started true)))))
  (stop [this]
    (if (:started this)
      (dosync
        (ref-set state nil)
        (ref-set history nil)
        (assoc this :started false))
      this)))

(defn- transaction-applied?
  [{old-world-time :world-time} {new-world-time :world-time :as new-world}]
  (and (= :ok (-> new-world :last-tx :status)) (< old-world-time new-world-time)))

(defn- send-tx-reports
  [report-ch _ _ old-world {last-tx :last-tx :as new-world}]
  (when (transaction-applied? old-world new-world)
    (a/put! report-ch last-tx)))

(defn- nodes-modified
  [graph last-tx]
  (map #(dg/node graph (first %)) (:outputs-modified last-tx)))

(defn- schedule-repaints
  [repaint-needed _ world-ref old-world {last-tx :last-tx graph :graph :as new-world}]
  (when (transaction-applied? old-world new-world)
    (repaint/schedule-repaint repaint-needed (keep
                                               #(when (satisfies? t/Frame %) %)
                                               (nodes-modified graph last-tx)))))

(def history-size-min 50)
(def history-size-max 60)
(def conj-undo-stack (partial util/push-with-size-limit history-size-min history-size-max))

#_(declare record-history-operation)

(defn- history-label [world]
  (or (get-in world [:last-tx :label]) (str "World Time: " (:world-time world))))

(defn- push-history [history-ref undo-context _ world-ref old-world new-world]
  (when (transaction-applied? old-world new-world)
    (dosync
      (assert (= (:state @history-ref) world-ref))
      (alter history-ref update-in [:undo-stack] conj-undo-stack old-world)
      (alter history-ref assoc-in  [:redo-stack] []))
    #_(record-history-operation undo-context (history-label new-world))))

(defn- repaint-all [graph repaint-needed]
  (let [nodes (dg/node-values graph)
        nodes-to-repaint (keep #(when (satisfies? t/Frame %) %) nodes)]
    (repaint/schedule-repaint repaint-needed nodes-to-repaint)))

(defn- undo-history [history-ref]
  (dosync
    (let [world-ref (:state @history-ref)
          old-world @world-ref
          new-world (peek (:undo-stack @history-ref))]
      (when new-world
        (ref-set world-ref (dissoc new-world :last-tx))
        (alter history-ref update-in [:undo-stack] pop)
        (alter history-ref update-in [:redo-stack] conj old-world)
        (repaint-all (:graph new-world) (:repaint-needed @history-ref))))))

(defn- redo-history [history-ref]
  (dosync
    (let [world-ref (:state @history-ref)
          old-world @world-ref
          new-world (peek (:redo-stack @history-ref))]
      (when new-world
        (ref-set world-ref (dissoc new-world :last-tx))
        (alter history-ref update-in [:undo-stack] conj old-world)
        (alter history-ref update-in [:redo-stack] pop)
        (repaint-all (:graph new-world) (:repaint-needed @history-ref))))))

(defn world
  [report-ch repaint-needed disposal-queue]
  (let [world-ref    (ref nil)
        history-ref  (ref nil)
        undo-context {} #_(UndoContext.)]
    (add-watch world-ref :send-tx-reports   (partial send-tx-reports report-ch))
    (add-watch world-ref :schedule-repaints (partial schedule-repaints repaint-needed))
    (add-watch world-ref :push-history      (partial push-history history-ref undo-context))
    (->World false world-ref history-ref undo-context repaint-needed disposal-queue)))

(defn- refresh-messages
  [{:keys [expired-outputs]}]
  (filter identity
    (for [[node output] expired-outputs]
      (logging-exceptions "extracting refresh message"
        (refresh-message node output)))))

(defn- multiplex-reports
  [tx-report refresh]
  (a/onto-chan refresh (refresh-messages tx-report) false))

(defn shred-tx-reports
  [in]
  (let [refresh (a/chan (a/sliding-buffer 1000))]
    (a/go-loop []
      (let [tx-report (a/<! in)]
        (if tx-report
          (do
            (multiplex-reports tx-report refresh)
            (recur))
          (a/close! refresh))))
    refresh))

(defn system
 []
 (let [repaint-needed (ref #{})
       tx-report-chan (a/chan 1)
       disposal-queue (a/chan (a/dropping-buffer maximum-disposal-backlog))]
   (component/map->SystemMap
    {:refresh   (refresh-subsystem (shred-tx-reports tx-report-chan) 1)
     :world     (world tx-report-chan repaint-needed disposal-queue)
     :repaint   (component/using (repaint/repaint-subsystem repaint-needed) [:world])
     :cache     (c/cache-subsystem maximum-cached-items disposal-queue)})))

(def the-system (atom (system)))

(defn system-cache [] (-> @the-system :cache))

(defn world-ref [] (-> @the-system :world :state))

(defn start
  ([]    (let [system-map (start the-system)
               state (-> system-map :world :state)
               graph (-> state deref :graph)
               root (dg/node graph 1)]
           (it/set-world-ref! state)
           (alter-var-root #'it/*scope* (constantly root))
           system-map))
  ([sys] (swap! sys component/start-system)))

(defn stop
  ([]    (stop the-system))
  ([sys] (swap! sys component/stop-system)))

(defn undo
  ([]    (undo the-system))
  ([sys] (undo-history (-> @sys :world :history))))

(defn redo
  ([]    (redo the-system))
  ([sys] (redo-history (-> @sys :world :history))))

(defn undo-context
  ([]    (undo-context the-system))
  ([sys] (-> @sys :world :undo-context)))

#_(defn history-operation [undo-context label]
  (doto (GenericOperation. label undo redo)
   (.addContext undo-context)))

#_(defn record-history-operation [undo-context label]
  (comment (let [operation (history-operation undo-context label)
                history (.. PlatformUI getWorkbench getOperationSupport getOperationHistory)]
            (.add history operation))))

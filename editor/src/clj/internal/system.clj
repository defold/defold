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
    (gt/node-id n)))

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

(defn- new-history
  [state]
  {:state      state
   :undo-stack []
   :redo-stack []})

(defn- transaction-applied?
  [{old-time :tx-id :or {old-time 0}}
   {new-time :tx-id :or {new-time 0} :as new-graph}]
  (< old-time new-time))

(def conj-undo-stack (partial util/push-with-size-limit history-size-min history-size-max))

(def ^:private undo-op-keys   [:label])
(def ^:private graph-label    :tx-label)
(def ^:private sequence-label :tx-sequence-label)

(def   history-state vector)
(def   history-state-label first)
(def   history-state-graph second)
(defn  history-state-sequence-label [[_ _ l]] l)

(defn- merge-or-push-history
  [history gref old-graph new-graph]
  (let [new-state (history-state (graph-label new-graph) old-graph (sequence-label new-graph))
        stack-op  (if (and (not (nil? (sequence-label new-graph)))
                           (= (:tx-sequence-label new-graph) (:tx-sequence-label old-graph)))
                    replace-top
                    conj-undo-stack)]
    (-> history
        (update :undo-stack stack-op new-state)
        (assoc :redo-stack []))))

(defn- watch-history [history-ref _ gref old-graph new-graph]
  (when (transaction-applied? old-graph new-graph)
    (dosync
     (assert (= (:state @history-ref) gref))
     (alter history-ref merge-or-push-history gref old-graph new-graph))))

(defn undo-stack [history-ref]
  (mapv (fn [state] {:label (history-state-label state)}) (:undo-stack @history-ref)))

(defn undo-history [history-ref]
  (dosync
   (let [gref          (:state @history-ref)
         old-graph     @gref
         history-state (peek (:undo-stack @history-ref))]
      (when history-state
        (ref-set gref (dissoc (history-state-graph history-state) :last-tx))
        (alter history-ref update-in [:undo-stack] pop)
        (alter history-ref update-in [:redo-stack] conj history-state)))))

(defn redo-stack [history-ref]
  (mapv (fn [state] {:label (history-state-label state)}) (:redo-stack @history-ref)))

(defn redo-history [history-ref]
  (dosync
   (let [gref          (:state @history-ref)
         old-graph     @gref
         history-state (peek (:redo-stack @history-ref))]
      (when history-state
        (ref-set gref (dissoc (history-state-graph history-state) :last-tx))
        (alter history-ref update-in [:undo-stack] conj history-state)
        (alter history-ref update-in [:redo-stack] pop)))))

(defn clear-history
  [history-ref]
  (dosync
   (alter history-ref assoc :undo-stack [] :redo-stack [])))

(defn last-graph     [s]     (-> s :last-graph))
(defn system-cache   [s]     (-> s :cache))
(defn disposal-queue [s]     (-> s :disposal-queue))
(defn graphs         [s]     (-> s :graphs))
(defn graph-ref      [s gid] (-> s :graphs (get gid)))
(defn graph          [s gid] (some-> s (graph-ref gid) deref))
(defn graph-time     [s gid] (-> s (graph gid) :tx-id))
(defn graph-history  [s gid] (-> s (graph gid) :history))
(defn basis          [s]     (ig/multigraph-basis (map-vals deref (graphs s))))

(defn- make-disposal-queue
  [{queue :disposal-queue :or {queue (a/chan (a/dropping-buffer maximum-disposal-backlog))}}]
  queue)

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_gid 0)}}]
  graph)

(defn- make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}} disposal-queue]
  (c/cache-subsystem cache-size disposal-queue))

(defn next-available-gid
  [s]
  (let [used (into #{} (keys (graphs s)))]
    (first (drop-while used (range 0 gt/MAX-GROUP-ID)))))

(defn- attach-graph*
  [s gref]
  (let [gid (next-available-gid s)]
    (dosync (alter gref assoc :_gid gid))
    (-> s
        (assoc :last-graph gid)
        (update-in [:graphs] assoc gid gref))))

(defn attach-graph
  [s g]
  (attach-graph* s (ref g)))

(defn attach-graph-with-history
  [s g]
  (let [gref (ref g)
        href (ref (new-history gref))]
    (dosync (alter gref assoc :history href))
    (let [s (attach-graph* s gref)]
      (add-watch gref ::push-history (partial watch-history href))
      s)))

(defn detach-graph
  [s g]
  (let [gid (:_gid g)]
    (update-in s [:graphs] dissoc gid)))

(defn make-system
  [configuration]
  (let [disposal-queue (make-disposal-queue configuration)
        initial-graph  (make-initial-graph configuration)
        pubch          (a/chan 100)
        subch          (a/pub pubch ::node-id (fn [_] (a/dropping-buffer 100)))
        cache          (make-cache configuration disposal-queue)]
    (-> {:disposal-queue disposal-queue
         :graphs         {}
         :cache          cache
         :publish-to     pubch
         :subscribe-to   subch}
        (attach-graph initial-graph))))

(defn start-event-loop!
  [sys id]
  (let [in (subscribe sys id)]
    (a/go-loop []
      (when-let [msg (a/<! in)]
        (when (not= ::stop-event-loop (:type msg))
          (when-let [n ()]
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

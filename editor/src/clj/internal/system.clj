(ns internal.system
  (:require [clojure.core.async :as a]
            [dynamo.util :as util]
            [dynamo.util :refer [map-vals]]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as h]
            [internal.transaction :as it]
            [service.log :as log])
  (:import [java.util.concurrent.atomic AtomicLong]))

(declare graphs)

(def ^:private maximum-cached-items     40000)
(def ^:private maximum-disposal-backlog 2500)
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

(defn- integer-counter
  []
  (AtomicLong. 0))

(defn- new-history
  [graph-ref]
  {:graph-ref  graph-ref
   :tape       (conj (h/paper-tape history-size-max) [])})

(def ^:private undo-op-keys   [:label])
(def ^:private graph-label    :tx-label)
(def ^:private sequence-label :tx-sequence-label)

(def  history-state                vector)
(def  history-state-label          first)
(def  history-state-graph          second)
(defn history-state-sequence-label [hs] (and hs (nth hs 2)))
(defn history-state-cache-keys     [hs] (and hs (nth hs 3)))

(defn history-state-map
  [[label graph _ cache-keys]]
  {:label    label
   :tx-id    (:tx-id graph)
   :modified cache-keys})

(defn- history-state-merge-cache-keys
  [[_ _ _ ks :as new] [_ _ _ old-ks :as old]]
  (assoc new 3 (into ks old-ks)))

(defn- merge-into-top
  [tape new-state]
  (let [old-state (h/ivalue tape)]
    (conj
     (h/truncate (h/iprev tape))
     (history-state-merge-cache-keys new-state old-state))))

(defn =*
  "Comparison operator that treats nil as not equal to anything."
  ([x] true)
  ([x y] (and x y (= x y) x))
  ([x y & more] (reduce =* (=* x y) more)))

(defn merge-or-push-history
  [history gref old-graph new-graph outputs-modified]
  (let [new-state (history-state (graph-label new-graph) new-graph (sequence-label new-graph) (set outputs-modified))
        tape-op   (if (=* (:tx-sequence-label new-graph) (:tx-sequence-label old-graph))
                    merge-into-top
                    conj)]
    (update history :tape tape-op new-state)))

(defn undo-stack [history-ref]
  (->> history-ref
       deref
       :tape
       h/before
       next
       (mapv history-state-map)))

(defn time-warp [history-ref system-snapshot graph outputs-to-refresh]
  (let [gid                  (:_gid graph)
        graphs               (graphs system-snapshot)]
    (-> (map-vals deref graphs)
        (ig/multigraph-basis)
        (ig/hydrate-after-undo graph)
        (it/update-successors* outputs-to-refresh)
        (get-in [:graphs gid]))))

(defn undo-history
  [history-ref system-snapshot]
  (dosync
   (let [{:keys [graph-ref tape]} @history-ref
         last-change              (h/ivalue tape)
         tape                     (h/iprev tape)
         undo-to                  (h/ivalue tape)
         outputs-to-refresh       (into (history-state-cache-keys last-change) (history-state-cache-keys undo-to))]
     (when undo-to
       (let [graph (time-warp history-ref system-snapshot (history-state-graph undo-to) outputs-to-refresh)]
         (ref-set graph-ref graph)
         (alter history-ref assoc :tape tape)
         outputs-to-refresh)))))

(defn redo-stack [history-ref]
  (->> history-ref
       deref
       :tape
       h/after
       (mapv history-state-map)))

(defn redo-history
  [history-ref system-snapshot]
  (dosync
   (let [{:keys [graph-ref tape]} @history-ref
         previous-change      (h/ivalue tape)
         tape                 (h/inext tape)
         redo-to              (h/ivalue tape)
         outputs-to-refresh   (into (history-state-cache-keys previous-change) (history-state-cache-keys redo-to))]
     (when redo-to
       (let [graph (time-warp history-ref system-snapshot (history-state-graph redo-to) outputs-to-refresh)]
         (ref-set graph-ref graph)
         (alter history-ref assoc :tape tape)
         outputs-to-refresh)))))

(defn clear-history
  [history-ref]
  (dosync
   (let [graph         (-> history-ref deref :graph-ref deref)
         initial-state (history-state (graph-label graph) graph (sequence-label graph) #{})]
     (alter history-ref update :tape (fn [tape]
                                       (conj (empty tape) initial-state))))))

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

(defn next-node-id
  [s gid]
  (gt/make-node-id gid (.getAndIncrement ^AtomicLong (get-in s [:id-generators gid]))))

(defn- attach-graph*
  [s gref]
  (let [gid (next-available-gid s)]
    (dosync (alter gref assoc :_gid gid))
    (-> s
        (assoc :last-graph gid)
        (update-in [:id-generators] assoc gid (integer-counter))
        (update-in [:graphs] assoc gid gref))))

(defn attach-graph
  [s g]
  (attach-graph* s (ref g)))

(defn attach-graph-with-history
  [s g]
  (let [gref (ref g)
        href (ref (new-history gref))]
    (dosync (alter gref assoc :history href))
    (attach-graph* s gref)))

(defn detach-graph
  [s g]
  (let [gid (if (map? g) (:_gid g) g)]
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
         :id-generators  {}
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

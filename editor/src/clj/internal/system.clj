(ns internal.system
  (:require [dynamo.util :as util]
            [dynamo.util :refer [map-vals]]
            [internal.cache :as c]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.history :as h]
            [service.log :as log])
  (:import [java.util.concurrent.atomic AtomicLong]))

(declare graphs)

(def ^:private maximum-cached-items     40000)
(def ^:private maximum-disposal-backlog 2500)
(def ^:private history-size-max         60)

(prefer-method print-method java.util.Map               clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IPersistentMap clojure.lang.IDeref)
(prefer-method print-method clojure.lang.IRecord        clojure.lang.IDeref)

(defn- integer-counter
  []
  (AtomicLong. 0))

(defn- new-history
  [graph-ref]
  {:graph-ref  graph-ref
   :tape       (conj (h/paper-tape history-size-max) [])})

(defrecord HistoryState [label graph sequence-label cache-keys])

(defn history-state [graph outputs-modified]
  (->HistoryState (:tx-label graph) graph (:tx-sequence-label graph) outputs-modified))

(defn- history-state-merge-cache-keys
  [new old]
  (update new :cache-keys into (:cache-keys old)))

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
  (let [new-state (history-state new-graph (set outputs-modified))
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
       vec))

(defn time-warp [history-ref system-snapshot graph outputs-to-refresh]
  (let [gid                  (:_gid graph)
        graphs               (graphs system-snapshot)]
    (-> (map-vals deref graphs)
        (ig/multigraph-basis)
        (ig/hydrate-after-undo graph)
        (ig/update-successors outputs-to-refresh)
        (get-in [:graphs gid]))))

(defn step-through-history
  [step-function history-ref system-snapshot]
  (dosync
   (let [{:keys [graph-ref tape]} @history-ref
         prior-state              (h/ivalue tape)
         tape                     (step-function tape)
         next-state               (h/ivalue tape)
         outputs-to-refresh       (into (:cache-keys prior-state) (:cache-keys next-state))]
     (when next-state
       (let [graph (time-warp history-ref system-snapshot (:graph next-state) outputs-to-refresh)]
         (ref-set graph-ref graph)
         (alter history-ref assoc :tape tape)
         outputs-to-refresh)))))

(def undo-history   (partial step-through-history h/iprev))
(def cancel-history (partial step-through-history h/drop-current))
(def redo-history   (partial step-through-history h/inext))

(defn redo-stack [history-ref]
  (->> history-ref
       deref
       :tape
       h/after
       vec))

(defn clear-history
  [history-ref]
  (dosync
   (let [graph         (-> history-ref deref :graph-ref deref)
         initial-state (history-state graph #{})]
     (alter history-ref update :tape (fn [tape]
                                       (conj (empty tape) initial-state))))))

(defn cancel
  [history-ref system-snapshot sequence-id]
  (let [{:keys [tape]}  @history-ref
        previous-change (h/ivalue tape)
        ok-to-cancel?   (=* sequence-id (:sequence-label previous-change))]
    (when ok-to-cancel? (cancel-history history-ref system-snapshot))))

(defn last-graph     [s]     (-> s :last-graph))
(defn system-cache   [s]     (-> s :cache))
(defn cache-disposal-queue [s] (-> s :cache-disposal-queue))
(defn deleted-disposal-queue [s] (-> s :deleted-disposal-queue))
(defn graphs         [s]     (-> s :graphs))
(defn graph-ref      [s gid] (-> s :graphs (get gid)))
(defn graph          [s gid] (some-> s (graph-ref gid) deref))
(defn graph-time     [s gid] (-> s (graph gid) :tx-id))
(defn graph-history  [s gid] (-> s (graph gid) :history))
(defn basis          [s]     (ig/multigraph-basis (map-vals deref (graphs s))))
(defn id-generators  [s]     (-> s :id-generators))
(defn override-id-generator [s] (-> s :override-id-generator))

(defn- make-initial-graph
  [{graph :initial-graph :or {graph (assoc (ig/empty-graph) :_gid 0)}}]
  graph)

(defn- make-cache
  [{cache-size :cache-size :or {cache-size maximum-cached-items}}]
  (c/cache-subsystem cache-size))

(defn next-available-gid
  [s]
  (let [used (into #{} (keys (graphs s)))]
    (first (drop-while used (range 0 gt/MAX-GROUP-ID)))))

(defn next-node-id*
  [id-generators gid]
  (gt/make-node-id gid (.getAndIncrement ^AtomicLong (get id-generators gid))))

(defn next-node-id
  [s gid]
  (next-node-id* (id-generators s) gid))

(defn next-override-id*
  [override-id-generator gid]
  (gt/make-override-id gid (.getAndIncrement ^AtomicLong override-id-generator)))

(defn next-override-id
  [s gid]
  (next-override-id* (override-id-generator s) gid))

(defn- attach-graph*
  [s gref]
  (let [gid (next-available-gid s)]
    (dosync (alter gref assoc :_gid gid))
    (-> s
        (assoc :last-graph gid)
        (update-in [:id-generators] assoc gid (integer-counter))
        (assoc :override-id-generator (integer-counter))
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
  (let [initial-graph  (make-initial-graph configuration)
        cache          (make-cache configuration)]
    (-> {:graphs         {}
         :id-generators  {}
         :cache          cache}
        (attach-graph initial-graph))))

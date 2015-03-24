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
(def ^:dynamic *the-system* (atom nil))

(import-vars [internal.system system-cache history world-graph world-time disposal-queue])

(defn- n->g
  "Get the graph this node belongs to."
  [node]
  (-> node :world-ref deref))

;; ---------------------------------------------------------------------------
;; Interrogating the Graph
;; ---------------------------------------------------------------------------
(defn node
  "Get a node, given just a world and an id."
  [world-ref id]
  (ig/node @world-ref id))

(defn node-feeding-into
  "Find the one-and-only node that sources this input on this node.
Should you use this on an input label with multiple connections, the result
is undefined."
  [node label]
  (let [graph (n->g node)]
    (ig/node graph
      (ffirst (ig/sources graph (:_id node) label)))))

(defn sources-of
  "Find the [node label] pairs for all connections into the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
    (sources-of (n->g node) node label))
  ([graph node label]
    (map
      (fn [[node-id label]] [(ig/node graph node-id) label])
      (ig/sources graph (:_id node) label))))

(defn nodes-consuming
  "Find the [node label] pairs for all connections reached from the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
     (nodes-consuming (n->g node) node label))
  ([graph node label]
     (map
       (fn [[node-id label]] [(ig/node graph node-id) label])
       (ig/targets graph (:_id node) label))))

(defn node-consuming
  "Like nodes-consuming, but only returns the first result."
  ([node label]
     (node-consuming (n->g node) node label))
  ([graph node label]
     (ffirst (nodes-consuming graph node label))))

(defn query
  "Query the project for resources that match all the clauses. Clauses are implicitly anded together.
A clause may be one of the following forms:

[:attribute value] - returns nodes that contain the given attribute and value.
(protocol protocolname) - returns nodes that satisfy the given protocol
(input fromnode)        - returns nodes that have 'fromnode' as an input
(output tonode)         - returns nodes that have 'tonode' as an output

All the list forms look for symbols in the first position. Be sure to quote the list
to distinguish it from a function call."
  [clauses]
  (map #(ig/node (is/world-graph @*the-system*) %)
       (ig/query (is/world-graph @*the-system*) clauses)))

(defn output-dependencies
  "Find all the outputs that could be affected by a change in the given outputs.
  Outputs are specified as pairs of [node-id label] for both the
  argument and return value."
  [graph outputs]
  (ig/trace-dependencies graph outputs))

;; ---------------------------------------------------------------------------
;; Transactional state
;; ---------------------------------------------------------------------------
(defn transact
  ([txs]
   (transact *the-system* txs))
  ([sys-ref txs]
   (let [world-ref (is/world-ref @sys-ref)
         tx-result (it/transact* world-ref (it/new-transaction-context world-ref txs))]
     (when (= :ok (:status tx-result))
       (dosync (ref-set world-ref (assoc (:graph tx-result)
                                         :last-tx (dissoc tx-result :graph))))
       (doseq [l (:old-event-loops tx-result)]      (is/stop-event-loop! @sys-ref l))
       (doseq [l (:new-event-loops tx-result)]      (is/start-event-loop! @sys-ref l))
       (doseq [d (vals (:nodes-deleted tx-result))] (is/dispose! @sys-ref d)))
     tx-result)))

(defn- resolve-return-val
  [tx-outcome val]
  (cond
    (sequential? val)  (map #(resolve-return-val tx-outcome %) val)
    (gt/node? val)     (or (ig/node (:graph tx-outcome) (it/resolve-tempid tx-outcome (:_id val))) val)
    :else              val))

(defn transactional*
  [inner]
  (binding [gt/*transaction* (gt/tx-begin gt/*transaction*)]
    (let [result     (inner)
          tx-outcome (gt/tx-apply gt/*transaction*)]
      (if (= :ok (:status tx-outcome))
        (resolve-return-val tx-outcome result)
        result))))

(defn in-transaction?
  "Returns true if the call occurs within a transactional boundary."
  []
  (satisfies? gt/Transaction gt/*transaction*))

(defn- is-scope?
  [n]
  (satisfies? t/NamingContext n))

;; ---------------------------------------------------------------------------
;; High level Transaction API
;; ---------------------------------------------------------------------------
(defn current-scope
  "Return the node that constitutes the current scope."
  []
  it/*scope*)

(defn ^:deprecated connect
  "Make a connection from an output of the source node to an input on the target node.
Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (gt/tx-bind gt/*transaction* (it/connect source-node source-label target-node target-label)))

(defn ^:deprecated disconnect
  "Remove a connection from an output of the source node to the input on the target node.
Note that there might still be connections between the two nodes, from other outputs to other inputs.
Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (gt/tx-bind gt/*transaction* (it/disconnect source-node source-label target-node target-label)))

(defn ^:deprecated become
  [source-node new-node]
  (gt/tx-bind gt/*transaction* (it/become source-node new-node))
  new-node)

(defn ^:deprecated set-property
  "Assign a value to a node's property (or properties) value(s) in a transaction."
  [n & kvs]
  (gt/tx-bind gt/*transaction*
    (for [[p v] (partition-all 2 kvs)]
      (it/update-property n p (constantly v) [])))
  n)

(defn ^:deprecated update-property
  "Apply a function to a node's property in a transaction. The function f will be
invoked as if by (apply f current-value args)"
  [n p f & args]
  (gt/tx-bind gt/*transaction*
    (it/update-property n p f args))
  n)

(defn add
  "Add a newly created node to the world. This will attach the node to the
current scope."
  [n]
  (gt/tx-bind gt/*transaction* (it/new-node n))
  (when (current-scope)
    (connect n :self (current-scope) :nodes)
    (if (is-scope? n)
      (set-property n :parent (current-scope))))
  n)

(defn delete
  [n]
  (gt/tx-bind gt/*transaction* (it/delete-node n)))

(defmacro in
  "Execute the forms within the given scope. Scope is a node that
inherits from dynamo.node/Scope."
  [s & forms]
  `(let [new-scope# ~s]
     (assert (satisfies? t/NamingContext new-scope#) (str new-scope# " cannot be used as a scope."))
     (binding [it/*scope* new-scope#]
       ~@forms)))

(defn refresh
  ([n]
     (refresh (:world-ref n) n))
  ([world-ref n]
     (if world-ref
       (node world-ref (:_id n))
       n)))

(defn ^:deprecated tx-label
  [label]
  (gt/tx-bind gt/*transaction* (it/label label)))

;; ---------------------------------------------------------------------------
;; For use by triggers
;; ---------------------------------------------------------------------------
(defn is-modified?
  ([transaction node]
    (boolean (contains? (:outputs-modified transaction) (:_id node))))
  ([transaction node output]
    (boolean (get-in transaction [:outputs-modified (:_id node) output]))))

(defn is-added? [transaction node]
  (contains? (:nodes-added transaction) (:_id node)))

(defn is-deleted? [transaction node]
  (contains? (:nodes-deleted transaction) (:_id node)))

(defn outputs-modified
  [transaction node]
  (get-in transaction [:outputs-modified (:_id node)]))

(defn pre-transaction-graph
  [transaction]
  (-> transaction :world-ref deref))

(defn in-transaction-graph
  [transaction]
  (-> transaction :graph))

(defn- enclosing-scope
  [graph n]
  (node-consuming graph n :self))

(defn path-to-root
  [graph n]
  (take-while :_id
    (iterate
      (partial enclosing-scope graph)
      n)))

(defn transaction-added-nodes
  [transaction]
  (let [g (:graph transaction)]
    (map #(ig/node g %) (:nodes-added transaction))))

(defn parent
  [graph n]
  (first (drop 1 (path-to-root graph n))))

;; ---------------------------------------------------------------------------
;; Boot, initialization, and facade
;; ---------------------------------------------------------------------------

(defn initialize
  [config]
  (reset! *the-system* (is/make-system config)))

(defn start
  []
  (swap! *the-system* is/start-system)
  (alter-var-root #'gt/*transaction* (constantly (gt/->TransactionSeed (partial transact *the-system*))))
  (alter-var-root #'it/*scope* (constantly nil)))

(defn stop
  []
  (swap! *the-system* is/stop-system))

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
  (c/cache-invalidate (is/system-cache @*the-system*) pairs))

(defn cache-encache
  "Uses the system’s cache.

  Atomic action to record one or more items in cache.

  The collection must contain tuples of [[node-id label] value]."
  [coll]
  (c/cache-encache (is/system-cache @*the-system*) coll))

(defn cache-hit
  "Uses the system’s cache.

  Atomic action to record hits on cached items

  The collection must contain tuples of [node-id label] pairs."
  [coll]
  (c/cache-hit (is/system-cache @*the-system*) coll))

(defn cache-snapshot
  "Get a value of the cache at a point in time."
  []
  (c/cache-snapshot (is/system-cache @*the-system*)))

(defn node-by-id
  [node-id]
  (ig/node (is/world-graph @*the-system*) node-id))

(defn node-value
  [node label]
  (in/node-value (is/world-graph @*the-system*) (is/system-cache @*the-system*) (:_id node) label))

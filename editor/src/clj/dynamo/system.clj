(ns dynamo.system
  "Functions for performing transactional changes to the system graph."
  (:require [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.file :as file]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.graph.query :as q]
            [internal.graph.tracing :as gt]
            [internal.transaction :as it :refer [Transaction *transaction*]]))

(defn- n->g
  "Get the graph this node belongs to."
  [node]
  (-> node :world-ref deref :graph))

; ---------------------------------------------------------------------------
; Interrogating the Graph
; ---------------------------------------------------------------------------
(defn node
  "Get a node, given just a world and an id."
  [world-ref id]
  (dg/node (:graph @world-ref) id))

(defn node-feeding-into
  "Find the one-and-only node that sources this input on this node.
Should you use this on an input label with multiple connections, the result
is undefined."
  [node label]
  (let [graph (n->g node)]
    (dg/node graph
      (ffirst (lg/sources graph (:_id node) label)))))

(defn sources-of
  "Find the [node label] pairs for all connections into the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
    (sources-of (n->g node) node label))
  ([graph node label]
    (map
      (fn [[node-id label]] [(dg/node graph node-id) label])
      (lg/sources graph (:_id node) label))))

(defn nodes-consuming
  "Find the [node label] pairs for all connections reached from the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
     (nodes-consuming (n->g node) node label))
  ([graph node label]
     (map
       (fn [[node-id label]] [(dg/node graph node-id) label])
       (lg/targets graph (:_id node) label))))

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
  [world-ref clauses]
  (map #(node world-ref %) (q/query (:graph @world-ref) clauses)))

(defn output-dependencies
  "Find all the outputs that could be affected by a change in the given outputs.
  Outputs are specified as pairs of [node-id label] for both the
  argument and return value."
  [graph outputs]
  (gt/trace-dependencies graph outputs))

; ---------------------------------------------------------------------------
; Transactional state
; ---------------------------------------------------------------------------
(defn node? [v] (satisfies? t/Node v))

(defn- resolve-return-val
  [tx-outcome val]
  (cond
    (sequential? val)  (map #(resolve-return-val tx-outcome %) val)
    (node? val)        (or (dg/node (:graph tx-outcome) (it/resolve-tempid tx-outcome (:_id val))) val)
    :else              val))

(defn transactional*
  [inner]
  (binding [*transaction* (it/tx-begin *transaction*)]
    (let [result     (inner)
          tx-outcome (it/tx-apply *transaction*)]
      (if (= :ok (:status tx-outcome))
        (resolve-return-val tx-outcome result)
        result))))

(defn in-transaction?
  "Returns true if the call occurs within a transactional boundary."
  []
  (satisfies? Transaction *transaction*))

(defn- is-scope?
  [n]
  (satisfies? t/NamingContext n))

; ---------------------------------------------------------------------------
; High level Transaction API
; ---------------------------------------------------------------------------
(defn current-scope
  "Return the node that constitutes the current scope."
  []
  it/*scope*)

(defmacro transactional
  "Executes the body within a project transaction. All actions
described in the body will happen atomically at the end of the transactional
block.

Transactional blocks nest nicely. The transaction will happen when the outermost
block ends."
  [& forms]
  `(transactional* (fn [] ~@forms)))

(defn connect
  "Make a connection from an output of the source node to an input on the target node.
Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (it/tx-bind *transaction* (it/connect source-node source-label target-node target-label)))

(defn disconnect
  "Remove a connection from an output of the source node to the input on the target node.
Note that there might still be connections between the two nodes, from other outputs to other inputs.
Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (it/tx-bind *transaction* (it/disconnect source-node source-label target-node target-label)))

(defn become
  [source-node new-node]
  (it/tx-bind *transaction* (it/become source-node new-node))
  new-node)

(defn set-property
  "Assign a value to a node's property (or properties) value(s) in a transaction."
  [n & kvs]
  (it/tx-bind *transaction*
    (for [[p v] (partition-all 2 kvs)]
      (it/update-property n p (constantly v) [])))
  n)

(defn update-property
  "Apply a function to a node's property in a transaction. The function f will be
invoked as if by (apply f current-value args)"
  [n p f & args]
  (it/tx-bind *transaction*
    (it/update-property n p f args))
  n)

(defn add
  "Add a newly created node to the world. This will attach the node to the
current scope."
  [n]
  (it/tx-bind *transaction* (it/new-node n))
  (when (current-scope)
    (connect n :self (current-scope) :nodes)
    (if (is-scope? n)
      (set-property n :parent (current-scope))))
  n)

(defn delete
  [n]
  (it/tx-bind *transaction* (it/delete-node n)))

(defn send-after
  "Spools messages for delivery after the transaction finishes.
The messages will only be sent if the transaction completes successfully."
  [n args]
  (it/tx-bind *transaction* (it/send-message n args)))

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

(defn tx-label
  [label]
  (it/tx-bind *transaction* (it/label label)))

; ---------------------------------------------------------------------------
; For use by triggers
; ---------------------------------------------------------------------------
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
  (-> transaction :world-ref deref :graph))

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
    (map #(dg/node g %) (:nodes-added transaction))))

(defn parent
  [graph n]
  (first (drop 1 (path-to-root graph n))))

(ns dynamo.graph
  (:require [dynamo.system :as ds]
            [internal.node :as in]
            [internal.system :as is]))

;; ---------------------------------------------------------------------------
;; Transactions
;; ---------------------------------------------------------------------------
(defmacro transactional
  "Executes the body within a project transaction. All actions
  described in the body will happen atomically at the end of the
  transactional block.

  Transactional blocks nest nicely. The transaction will happen when
  the outermost block ends."
  [& forms]
  `(ds/transactional* (fn [] ~@forms)))

(defn connect
  "Make a connection from an output of the source node to an input on the target node.
   Takes effect when a transaction is applied."
  [source-node source-label target-node target-label]
  (ds/connect source-node source-label target-node target-label))

(defn disconnect
  "Remove a connection from an output of the source node to the input on the target node.
  Note that there might still be connections between the two nodes,
  from other outputs to other inputs.  Takes effect when a transaction
  is applied."
  [source-node source-label target-node target-label]
  (ds/disconnect source-node source-label target-node target-label))

(defn become
  "Turn one kind of node into another, in a transaction. All properties and their values
   will be carried over from source-node to new-node. The resulting node will still have
   the same node-id.

   Any input or output connections to labels that exist on both
  source-node and new-node will continue to exist. Any connections to
  labels that don't exist on new-node will be disconnected in the same
  transaction."
  [source-node new-node]
  (ds/become source-node new-node))

(defn set-property
  "Assign a value to a node's property (or properties) value(s) in a
  transaction."
  [n & kvs]
  (apply ds/set-property n kvs))

(defn update-property
  "Apply a function to a node's property in a transaction. The
  function f will be invoked as if by (apply f current-value args)"
  [n p f & args]
  (apply ds/update-property n p f args))



;; ---------------------------------------------------------------------------
;; Values
;; ---------------------------------------------------------------------------
(defn node-value
  "Pull a value from a node's output, identified by `label`.
  The value may be cached or it may be computed on demand. This is
  transparent to the caller.

  This uses the \"current\" value of the node and its output.
  That means the caller will receive a value consistent with the most
  recently committed transaction.

  The label must exist as a defined transform on the node, or an
  AssertionError will result."
  ([node label]
   (node-value (is/world-graph) (is/system-cache) node label))
  ([graph cache node label]
   (in/node-value graph cache (:_id node) label)))

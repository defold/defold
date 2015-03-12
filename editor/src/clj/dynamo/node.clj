(ns dynamo.node
  "This namespace has two fundamental jobs. First, it provides the operations
for defining and instantiating nodes: `defnode` and `construct`.

Second, this namespace defines some of the basic node types and mixins."
  (:require [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [dynamo.util :refer :all]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.node :as in]
            [internal.outline :as outline]
            [internal.property :as ip]
            [plumbing.core :refer [defnk fnk]]
            [schema.core :as s]))

(defn construct
  "Creates an instance of a node. The node-type must have been
previously defined via `defnode`.

The node's properties will all have their default values. The caller
may pass key/value pairs to override properties.

A node that has been constructed is not connected to anything and it
doesn't exist in the graph yet. The caller must use `dynamo.system/add`
to place the node in the graph.

Example:
  (defnode GravityModifier
    (property acceleration s/Int (default 32))

  (construct GravityModifier :acceleration 16)"
  [node-type & {:as args}]
  (assert (::ctor node-type))
  ((::ctor node-type) (merge {:_id (in/tempid)} args)))

(defn abort
  "Abort production function and use substitute value."
  [msg & args]
  (throw (apply ex-info msg args)))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'."
  [node type & {:as body}]
  (process-one-event (ds/refresh node) (assoc body :type type)))

; ---------------------------------------------------------------------------
; Bootstrapping the core node types
; ---------------------------------------------------------------------------
(defn inject-new-nodes
  "Implementation function that performs dependency injection for nodes.
This function should not be called directly."
  [transaction graph self label kind inputs-affected]
  (when (inputs-affected :nodes)
    (let [nodes-before-txn         (cons self (in/get-inputs (ds/pre-transaction-graph transaction) self :nodes))
          nodes-after-txn          (in/get-inputs (ds/in-transaction-graph transaction) self :nodes)
          nodes-before-txn-ids     (into #{} (map :_id nodes-before-txn))
          new-nodes-in-scope       (remove nodes-before-txn-ids nodes-after-txn)
          out-from-new-connections (in/injection-candidates nodes-before-txn new-nodes-in-scope)
          in-to-new-connections    (in/injection-candidates new-nodes-in-scope nodes-before-txn)
          between-new-nodes        (in/injection-candidates new-nodes-in-scope new-nodes-in-scope)
          candidates               (set/union out-from-new-connections in-to-new-connections between-new-nodes)
          candidates               (remove (fn [[out out-label in in-label]]
                                             (or
                                              (= (:_id out) (:_id in))
                                              (lg/connected? graph (:_id out) out-label (:_id in) in-label)))
                                           candidates)]
      (doseq [connection candidates]
        (apply ds/connect connection)))))

(defn dispose-nodes
  "Trigger to dispose nodes from a scope when the scope is destroyed.
This should not be called directly."
  [transaction graph self label kind]
  (when (ds/is-deleted? transaction self)
    (let [graph-before-deletion (-> transaction :world-ref deref :graph)
          nodes-to-delete       (in/get-inputs graph-before-deletion self :nodes)]
      (doseq [n nodes-to-delete]
        (ds/delete n)))))

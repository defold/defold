(ns dynamo.system
  (:require [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.resource :refer [disposable?]]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.query :as iq]
            [internal.system :as is]
            [internal.transaction :as it :refer [*transaction*]])
  (:import [internal.transaction NullTransaction]))

(defn- the-world []  (-> is/the-system deref :world))
(defn groot []       (dg/node (-> (the-world) :state is/graph) 1))
(defn started? []    (-> (the-world) :started))

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

(defn transactional* [world-ref inner]
  (binding [*transaction* (it/tx-begin *transaction* world-ref)]
    (let [result     (inner)
          tx-outcome (it/tx-apply *transaction*)]
      (if tx-outcome
        (resolve-return-val tx-outcome result)
        result))))

(defn in-transaction? []
  (not (instance? NullTransaction *transaction*)))

(defn- is-scope?
  [n]
  (and
    (satisfies? t/InjectionContext n)
    (satisfies? t/NamingContext n)))

; ---------------------------------------------------------------------------
; High level API
; ---------------------------------------------------------------------------
(defn current-scope
  []
  it/*scope*)

(defn enclosing-scopes
  ([]
    (enclosing-scopes it/*scope*))
  ([s]
    (take-while identity (iterate :parent s))))

(defmacro transactional
  [& forms]
  `(transactional* (:world-ref (current-scope)) (fn [] ~@forms)))

(defn update
  [n f & args]
  (it/tx-bind *transaction* (apply it/update-node n f args)))

(defn connect
  [source-node source-label target-node target-label]
  (it/tx-bind *transaction* (it/connect source-node source-label target-node target-label)))

(defn disconnect
  [source-node source-label target-node target-label]
  (it/tx-bind *transaction* (it/disconnect source-node source-label target-node target-label)))

(defn set-property
  [n & kvs]
  (it/tx-bind *transaction* (apply it/update-node n assoc kvs)))

(defn update-property
  [n p f & args]
  (it/tx-bind *transaction* (apply it/update-node n update-in [p] f args)))

(defn add
  [n]
  (it/tx-bind *transaction* (it/new-node n))
  (when (current-scope)
    (connect n :self (current-scope) :nodes)
    (if (is-scope? n)
      (set-property n :parent (current-scope))))
  n)

(defn send-after
  [n args]
  (it/tx-bind *transaction* (it/send-message n args)))

(defmacro in
  [s & forms]
  `(let [new-scope# ~s]
     (assert (satisfies? t/InjectionContext new-scope#) (str new-scope# " cannot be used as a scope."))
     (assert (satisfies? t/NamingContext new-scope#) (str new-scope# " cannot be used as a scope."))
     (binding [it/*scope* new-scope#]
       ~@forms)))

(defn refresh
  [world-ref n]
  (iq/node-by-id world-ref (:_id n)))

; ---------------------------------------------------------------------------
; Documentation
; ---------------------------------------------------------------------------
(doseq [[v doc]
       {*ns*
        "Functions for performing transactional changes to the system graph."

        #'groot
        "Convenience function to access the root node of the graph."

        #'started?
        "Returns true if the system has been started and not yet stopped."

        #'transactional
        "Executes the body within a project transaction. All actions
described in the body will happen atomically at the end of the transactional
block.

Transactional blocks nest nicely. The transaction will happen when the outermost
block ends."

        #'current-scope
        "Return the node that constitutes the current scope."

        #'update
        "Update a node by applying a function to its in-transaction value. The function f
will be invoked as if by (apply f n args)."

        #'connect
        "Make a connection from an output of the source node to an input on the target node.
Takes effect when a transaction is applied."

        #'disconnect
        "Remove a connection from an output of the source node to the input on the target node.
Note that there might still be connections between the two nodes, from other outputs to other inputs.
Takes effect when a transaction is applied."

        #'set-property
        "Assign a value to a node's property (or properties) value(s) in a transaction."

        #'update-property
        "Apply a function to a node's property in a transaction. The function f will be
invoked as if by (apply f current-value args)"

        #'add
        "Add a newly created node to the world. This will attach the node to the
current scope."

        #'send-after
        "Spools messages for delivery after the transaction finishes.
The messages will only be sent if the transaction completes successfully."

        #'in
        "Execute the forms within the given scope. Scope is a node that
inherits from dynamo.node/Scope."

}]
  (alter-meta! v assoc :doc doc))

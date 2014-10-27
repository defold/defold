(ns dynamo.system
  (:require [clojure.core.async :as a]
            [clojure.core.cache :as cache]
            [dynamo.resource :refer [disposable?]]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.system :as is]
            [internal.transaction :as it]))

(defn- the-world []  (-> is/the-system deref :world))
(defn groot []       (dg/node (-> (the-world) :state is/graph) 1))
(defn started? []    (-> (the-world) :started))

; ---------------------------------------------------------------------------
; Internal messaging
; ---------------------------------------------------------------------------
(defn publish-all
  [world-ref msgs]
  (let [ch (:publish-to @world-ref)]
    (doseq [s msgs]
      (a/put! ch s))))

; ---------------------------------------------------------------------------
; Transactional state
; ---------------------------------------------------------------------------

(defprotocol Transaction
  (tx-bind       [this step]  "Add a step to the transaction")
  (tx-send-after [this body]  "Send a message when the transaction completes")
  (tx-apply      [this]       "Apply the transaction steps")
  (tx-begin      [this world] "Create a subordinate transaction scope"))

(declare ->NestedTransaction)

(deftype NestedTransaction [enclosing]
  Transaction
  (tx-bind [this step]        (tx-bind enclosing step))
  (tx-send-after [this body]  nil)
  (tx-apply [this]            nil)
  (tx-begin [this _]          (->NestedTransaction this)))

(deftype RootTransaction [world-ref steps messages]
  Transaction
  (tx-bind [this step]       (conj! steps step))
  (tx-send-after [this body] (conj! messages body))
  (tx-apply [this]           (when (< 0 (count steps))
                               (let [actions (persistent! steps)
                                     tx-result (it/transact world-ref actions)]
                                 (when (= :ok (:status tx-result))
                                   (publish-all world-ref (persistent! messages)))
                                 tx-result)))
  (tx-begin [this _]         (->NestedTransaction this)))

(deftype NullTransaction []
  Transaction
  (tx-bind [this step]       nil)
  (tx-send-after [this body] nil)
  (tx-apply [this]           nil)
  (tx-begin [this world-ref] (->RootTransaction world-ref (transient []) (transient []))))

(def ^:private ^:dynamic *transaction* (->NullTransaction))

(defn transactional* [world-ref inner]
  (binding [*transaction* (tx-begin *transaction* world-ref)]
    (let [result (inner)]
      (tx-apply *transaction*)
      result)))

(defn in-transaction? []
  (not (instance? NullTransaction *transaction*)))

; ---------------------------------------------------------------------------
; High level API
; ---------------------------------------------------------------------------
(defn current-scope
  []
  it/*scope*)

(defmacro transactional
  [& forms]
  `(transactional* (:world-ref (current-scope)) (fn [] ~@forms)))

(defn update
  [n f & args]
  (tx-bind *transaction* (apply it/update-node n f args)))

(defn connect
  [source-node source-label target-node target-label]
  (tx-bind *transaction* (it/connect source-node source-label target-node target-label)))

(defn disconnect
  [source-node source-label target-node target-label]
  (tx-bind *transaction* (it/disconnect source-node source-label target-node target-label)))

(defn set-property
  [n & kvs]
  (tx-bind *transaction* (apply it/update-node n assoc kvs)))

(defn update-property
  [n p f & args]
  (tx-bind *transaction* (apply it/update-node n update-in [p] f args)))

(defn add
  [n]
  (tx-bind *transaction* (it/new-node n))
  (when (current-scope)
    (connect n :self (current-scope) :nodes))
  n)

(defn send-after
  [n args]
  (tx-send-after *transaction* (is/address-to n args)))

(defmacro in
  [s & forms]
  `(let [new-scope# ~s]
     (assert (satisfies? t/InjectionContext new-scope#) (str new-scope# " cannot be used as a scope."))
     (assert (satisfies? t/NamingContext new-scope#) (str new-scope# " cannot be used as a scope."))
     (binding [it/*scope* new-scope#]
       ~@forms)))

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

        #'publish-all
        "Publish a collection of messages on the internal message bus."

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

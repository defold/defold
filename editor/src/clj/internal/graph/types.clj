(ns internal.graph.types
  (:require [schema.core :as s]
            [schema.macros :as sm]))

(defprotocol TransactionStarter
  (tx-begin [this]      "Create a subordinate transaction scope"))

(defprotocol Transaction
  (tx-bind  [this work] "Add a unit of work to the transaction")
  (tx-apply [this]      "Apply the transaction work"))

(defprotocol TransactionReceiver
  (tx-merge [this work] "Merge the transaction work into yourself."))


(defprotocol NodeType
  (supertypes           [this])
  (interfaces           [this])
  (protocols            [this])
  (method-impls         [this])
  (triggers             [this])
  (transforms'          [this])
  (transform-types'     [this])
  (properties'          [this])
  (inputs'              [this])
  (injectable-inputs'   [this])
  (outputs'             [this])
  (cached-outputs'      [this])
  (event-handlers'      [this])
  (output-dependencies' [this]))

(s/defrecord NodeRef [world-ref node-id]
  clojure.lang.IDeref
  (deref [this] (get-in (:graph @world-ref) [:nodes node-id])))

(defmethod print-method NodeRef
  [^NodeRef v ^java.io.Writer w]
  (.write w (str "<NodeRef@" (:node-id v) ">")))

(defn node-ref [node] (NodeRef. (:world-ref node) (:_id node)))

(defprotocol Node
  (node-type           [this]        "Return the node type that created this node.")
  (transforms          [this]        "temporary")
  (transform-types     [this]        "temporary")
  (properties          [this]        "Produce a description of properties supported by this node.")
  (inputs              [this]        "Return a set of labels for the allowed inputs of the node.")
  (injectable-inputs   [this]        "temporary")
  (input-types         [this]        "Return a map from input label to schema of the value type allowed for the input")
  (outputs             [this]        "Return a set of labels for the outputs of this node.")
  (cached-outputs      [this]        "Return a set of labels for the outputs of this node which are cached. This must be a subset of 'outputs'.")
  (output-dependencies [this]        "Return a map of labels for the inputs and properties to outputs that depend on them."))

(defn node? [v] (satisfies? Node v))

(defprotocol MessageTarget
  (process-one-event [this event]))

(defn message-target? [v] (satisfies? MessageTarget v))


;; ---------------------------------------------------------------------------
;; Transaction protocols
;; ---------------------------------------------------------------------------

;; These are here temporarily while I refactor state handling.

(declare ->TransactionLevel)

(defn- make-transaction-level
  ([receiver]
    (make-transaction-level receiver 0))
  ([receiver depth]
    (->TransactionLevel receiver depth (transient []))))

(deftype TransactionLevel [receiver depth accumulator]
  TransactionStarter
  (tx-begin [this]      (make-transaction-level this (inc depth)))

  Transaction
  (tx-bind  [this work] (conj! accumulator work))
  (tx-apply [this]      (tx-merge receiver (persistent! accumulator)))

  TransactionReceiver
  (tx-merge [this work] (tx-bind this work)
                        {:status :pending}))

(deftype TransactionSeed [tx-fn]
  TransactionStarter
  (tx-begin [this]      (make-transaction-level this))

  TransactionReceiver
  (tx-merge [this work] (if (< 0 (count work))
                          (tx-fn work)
                          {:status :empty})))


(deftype NullTransaction []
  TransactionStarter
  (tx-begin [this]      (assert false "The system is not initialized enough to run transactions yet.")))

(def ^:dynamic *transaction* (->NullTransaction))

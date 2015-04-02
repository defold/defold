(ns internal.graph.types
  (:require [schema.core :as s]))

(defprotocol Arc
  (head [this] "returns [source-node source-label]")
  (tail [this] "returns [target-node target-label]"))

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
  (input-dependencies'  [this]))

(defprotocol Node
  (node-id             [this]        "Return an ID that can be used to get this node (or a future value of it).")
  (node-type           [this]        "Return the node type that created this node.")
  (transforms          [this]        "temporary")
  (transform-types     [this]        "temporary")
  (properties          [this]        "Produce a description of properties supported by this node.")
  (inputs              [this]        "Return a set of labels for the allowed inputs of the node.")
  (injectable-inputs   [this]        "temporary")
  (input-types         [this]        "Return a map from input label to schema of the value type allowed for the input")
  (outputs             [this]        "Return a set of labels for the outputs of this node.")
  (cached-outputs      [this]        "Return a set of labels for the outputs of this node which are cached. This must be a subset of 'outputs'.")
  (input-dependencies  [this]        "Return a map of labels for the inputs and properties to outputs that depend on them."))

(defn node? [v] (satisfies? Node v))

(defprotocol MessageTarget
  (process-one-event [this event]))

(defn message-target? [v] (satisfies? MessageTarget v))

(defprotocol IBasis
  (node-by-id       [this node-id])
  (node-by-property [this label value])
  (sources          [this node-id label])
  (targets          [this node-id label])
  (add-node         [this tempid  value]         "returns [basis realid real-value]")
  (delete-node      [this node-id]               "returns [basis node]")
  (replace-node     [this node-id value]         "returns [basis node]")
  (update-property  [this node-id label f args]  "returns [basis new-node]")
  (connect          [this src-id src-label tgt-id tgt-label])
  (disconnect       [this src-id src-label tgt-id tgt-label])
  (connected?       [this src-id src-label tgt-id tgt-label])
  (query [this clauses]
    "Query for nodes that match all the clauses. Clauses are
   implicitly anded together.  A clause may be one of the following
   forms:

[:attribute value] - returns nodes that contain the given attribute and value.
(protocol protocolname) - returns nodes that satisfy the given protocol
(input fromnode)        - returns nodes that have 'fromnode' as an input
(output tonode)         - returns nodes that have 'tonode' as an output

   All the list forms look for symbols in the first position. Be sure
   to quote the list to distinguish it from a function call."))

;; ---------------------------------------------------------------------------
;; ID helpers
;; ---------------------------------------------------------------------------

(def ^:const NID-BITS                                56)
(def ^:const NID-MASK                  0xffffffffffffff)
(def ^:const NID-SIGN-EXTEND         -72057594037927936) ;; as a signed long
(def ^:const GID-BITS                                 7)
(def ^:const GID-MASK                              0x7f)
(def ^:const TEMPID-INDICATOR      -9223372036854775808) ;; 2^63 as a signed long
(def ^:const MAX-GROUP-ID                           254)

(defn make-nref ^long [^long gid ^long nid]
  (bit-or
   (bit-shift-left gid NID-BITS)
   (bit-and nid 0xffffffffffffff)))

(defn nref->gid ^long [^long nref]
  (bit-and (bit-shift-right nref NID-BITS) GID-MASK))

(defn nref->nid ^long [^long nref]
  (let [r (bit-and nref NID-MASK)]
    (if (zero? (bit-and r TEMPID-INDICATOR))
      r
      (bit-or r NID-SIGN-EXTEND))))

(defn- make-tempid ^long [^long gid ^long nid]
  (bit-or TEMPID-INDICATOR (make-nref gid nid)))

(def ^:private ^java.util.concurrent.atomic.AtomicLong next-tempid (java.util.concurrent.atomic.AtomicLong. 1))

(defn tempid  [gid] (make-tempid gid (- (.getAndIncrement next-tempid))))
(defn tempid? [x] (and x (neg? x)))

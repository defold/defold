(ns internal.graph.types
  (:require [schema.core :as s]))

(defn pfnk?
  "True if the function has a schema. (I.e., it is a valid production function"
  [f]
  (contains? (meta f) :schema))

(defprotocol Consumer)
(defprotocol Producer)

(defprotocol Arc
  (head [this] "returns [source-node source-label]")
  (tail [this] "returns [target-node target-label]"))

(defprotocol NodeType
  (supertypes            [this])
  (interfaces            [this])
  (protocols             [this])
  (method-impls          [this])
  (triggers              [this])
  (transforms'           [this])
  (transform-types'      [this])
  (properties'           [this])
  (inputs'               [this])
  (injectable-inputs'    [this])
  (outputs'              [this])
  (cached-outputs'       [this])
  (event-handlers'       [this])
  (input-dependencies'   [this])
  (substitute-for'       [this input])
  (input-type            [this input])
  (input-cardinality     [this input])
  (output-type           [this output])
  (property-type         [this output])
  (property-passthrough? [this output]))

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
  (input-dependencies  [this]        "Return a map of labels for the inputs and properties to outputs that depend on them.")
  (substitute-for      [this input]  "Return a generator for a substitute value (if any) for the given input")
  (produce-value       [this output evaluation-context] "Return the value of the named output"))

(defn node? [v] (satisfies? Node v))

(defprotocol MessageTarget
  (process-one-event [this event]))

(defn message-target? [v] (satisfies? MessageTarget v))

(defprotocol IBasis
  (node-by-property [this label value])
  (arcs-by-head     [this node-id])
  (arcs-by-tail     [this node-id])
  (sources          [this node-id] [this node-id label])
  (targets          [this node-id] [this node-id label])
  (add-node         [this value]                 "returns [basis real-value]")
  (delete-node      [this node-id]               "returns [basis node]")
  (replace-node     [this node-id value]         "returns [basis node]")
  (update-property  [this node-id label f args]  "returns [basis new-node]")
  (connect          [this src-id src-label tgt-id tgt-label])
  (disconnect       [this src-id src-label tgt-id tgt-label])
  (connected?       [this src-id src-label tgt-id tgt-label])
  (dependencies     [this node-id-output-label-pairs]
    "Follow arcs through the graphs, from outputs to the inputs
     connected to them, and from those inputs to the downstream
     outputs that use them, and so on. Continue following links until
     all reachable outputs are found.

     Returns a collection of [node-id output-label] pairs."))

(defn protocol? [x] (and (map? x) (contains? x :on-interface)))

;; ---------------------------------------------------------------------------
;; ID helpers
;; ---------------------------------------------------------------------------

(def NodeID s/Int)

(def ^:const NID-BITS                                56)
(def ^:const NID-MASK                  0xffffffffffffff)
(def ^:const NID-SIGN-EXTEND         -72057594037927936) ;; as a signed long
(def ^:const GID-BITS                                 7)
(def ^:const GID-MASK                              0x7f)
(def ^:const MAX-GROUP-ID                           254)

(defn make-node-id ^long [^long gid ^long nid]
  (bit-or
   (bit-shift-left gid NID-BITS)
   (bit-and nid 0xffffffffffffff)))

(defn node-id->graph-id ^long [^long node-id]
  (bit-and (bit-shift-right node-id NID-BITS) GID-MASK))

(defn node-id->nid ^long [^long node-id]
  (bit-and node-id NID-MASK))

(defn node->graph-id ^long [node] (node-id->graph-id (node-id node)))

;; ---------------------------------------------------------------------------
;; The Error type
;; ---------------------------------------------------------------------------
(defrecord ErrorValue [reason])

(defn error [& reason] (->ErrorValue reason))
(defn error? [x] (instance? ErrorValue x))

(ns internal.graph.types
  (:require [internal.util :as util]
            [schema.core :as s]))

(set! *warn-on-reflection* true)

(defprotocol Arc
  (head [this] "returns [source-node source-label]")
  (tail [this] "returns [target-node target-label]"))

(defn node-id? [v] (integer? v))

(defprotocol Evaluation
  (produce-value       [this label evaluation-context] "Pull a value using an evaluation context"))

(defprotocol Node
  (node-id               [this]                          "Return an ID that can be used to get this node (or a future value of it).")
  (node-type             [this basis]                    "Return the node type that created this node.")
  (get-property          [this basis property]           "Return the value of the named property")
  (set-property          [this basis property value]     "Set the named property")
  (overridden-properties [this basis]                    "Return a map of property name to override value")
  (property-overridden?  [this property]))

(defprotocol OverrideNode
  (clear-property      [this basis property]           "Clear the named property (this is only valid for override nodes)")
  (override-id         [this]                          "Return the ID of the override this node belongs to, if any")
  (original            [this]                          "Return the ID of the original of this node, if any")
  (set-original        [this original-id]              "Set the ID of the original of this node, if any"))

(defprotocol IBasis
  (node-by-id-at    [this node-id])
  (node-by-property [this label value])
  (arcs-by-head     [this node-id] [this node-id label])
  (arcs-by-tail     [this node-id] [this node-id label])
  (sources          [this node-id] [this node-id label])
  (targets          [this node-id] [this node-id label])
  (add-node         [this value]                 "returns [basis real-value]")
  (delete-node      [this node-id]               "returns [basis node]")
  (replace-node     [this node-id value]         "returns [basis node]")
  (override-node    [this original-id override-id])
  (override-node-clear [this original-id])
  (add-override     [this override-id override])
  (delete-override  [this override-id])
  (replace-override [this override-id value])
  (connect          [this src-id src-label tgt-id tgt-label])
  (disconnect       [this src-id src-label tgt-id tgt-label])
  (connected?       [this src-id src-label tgt-id tgt-label])
  (dependencies     [this outputs-by-node-ids]
    "Follow arcs through the graphs, from outputs to the inputs
     connected to them, and from those inputs to the downstream
     outputs that use them, and so on. Continue following links until
     all reachable outputs are found.

     Takes and returns a map of the form {node-id #{label ...} ...}")
  (original-node    [this node-id]))

(defn basis? [value]
  (satisfies? IBasis value))

;; ---------------------------------------------------------------------------
;; ID helpers
;; ---------------------------------------------------------------------------

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

(defn make-override-id ^long [^long gid ^long oid]
  (bit-or
   (bit-shift-left gid NID-BITS)
   (bit-and oid 0xffffffffffffff)))

(defn override-id->graph-id ^long [^long override-id]
  (bit-and (bit-shift-right override-id NID-BITS) GID-MASK))

(ns internal.graph.types
  (:require [potemkin.namespaces :refer [import-vars]]
            [schema.core :as s]))

(defn pfnk?
  "True if the function has a schema. (I.e., it is a valid production function"
  [f]
  (contains? (meta f) :schema))

(defn always
  "Takes a value and produces a constanly function that has a schema and
   is a considered a valid pfnk"
  [v]
  (let [always-fn (constantly v)]
   (vary-meta always-fn assoc :schema (s/=> (class v) {}))))

(defprotocol Arc
  (head [this] "returns [source-node source-label]")
  (tail [this] "returns [target-node target-label]"))

(defprotocol NodeType
  (supertypes             [this])
  (interfaces             [this])
  (protocols              [this])
  (method-impls           [this])
  (triggers               [this])
  (transforms             [this])
  (transform-types        [this])
  (internal-properties    [this])
  (properties             [this])
  (externs                [this])
  (declared-inputs        [this])
  (injectable-inputs      [this])
  (declared-outputs       [this])
  (cached-outputs         [this])
  (input-dependencies     [this])
  (substitute-for         [this input])
  (input-type             [this input])
  (input-cardinality      [this input])
  (output-type            [this output])
  (property-type          [this output])
  (property-passthrough?  [this output])
  (property-display-order [this]))

(defn node-type? [x] (satisfies? NodeType x))

(defn input-labels    [node-type] (-> node-type declared-inputs keys set))
(defn output-labels   [node-type] (-> node-type declared-outputs))
(defn property-labels [node-type] (-> node-type properties keys set))

(defprotocol Node
  (node-id             [this]        "Return an ID that can be used to get this node (or a future value of it).")
  (node-type           [this]        "Return the node type that created this node.")
  (produce-value       [this output evaluation-context] "Return the value of the named output"))

(defn node? [v] (satisfies? Node v))

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

(defprotocol PropertyType
  (property-value-type         [this]   "Prismatic schema for property value type")
  (property-default-value      [this])
  (property-validate           [this v] "Returns a possibly-empty seq of messages.")
  (property-valid-value?       [this v] "If valid, returns nil. If invalid, returns seq of Marker")
  (property-tags               [this]))

(defn property-type? [x] (satisfies? PropertyType x))

(def Properties {s/Keyword {:value s/Any :type (s/protocol PropertyType)}})

(defprotocol Dynamics
  (dynamic-attributes          [this] "Return a map from label to fnk")
  (dynamic-value               [this k v] "Returns the value of the dynamic property key - if a fnk, then the result of applying v"))

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

(defn error [reason] (->ErrorValue reason))

(defn error?
  [x]
  (cond
    (instance? ErrorValue x) x
    (vector? x)              (some error? x)
    :else                    nil))

;; ---------------------------------------------------------------------------
;; Destructors
;; ---------------------------------------------------------------------------
(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(defn disposable? [x] (satisfies? IDisposable x))

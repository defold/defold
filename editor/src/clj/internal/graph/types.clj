(ns internal.graph.types
  (:require [dynamo.util :as util]
            [internal.graph.error-values :as ie]
            [schema.core :as s])
  (:import [internal.graph.error_values ErrorValue]))

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
  (transforms             [this])
  (transform-types        [this])
  (declared-properties    [this])
  (declared-inputs        [this])
  (injectable-inputs      [this])
  (declared-outputs       [this])
  (cached-outputs         [this])
  (input-dependencies     [this])
  (substitute-for         [this input])
  (input-type             [this input])
  (input-cardinality      [this input])
  (cascade-deletes        [this])
  (output-type            [this output])
  (property-passthrough?  [this output])
  (property-display-order [this]))

(defn node-type? [x] (satisfies? NodeType x))

(defn input-labels        [node-type]          (-> node-type declared-inputs keys set))
(defn output-labels       [node-type]          (-> node-type declared-outputs))
(defn property-labels     [node-type]          (-> node-type declared-properties keys set))
(defn internal-properties [node-type]          (->> node-type declared-properties (util/filter-vals :internal?)))
(defn public-properties   [node-type]          (->> node-type declared-properties (util/filter-vals (comp not :internal?))))
(defn externs             [node-type]          (->> node-type declared-properties (util/filter-vals :unjammable?)))
(defn property-type       [node-type property] (-> node-type declared-properties (get property)))

(def NodeID s/Int)

(defprotocol Node
  (node-id             [this]        "Return an ID that can be used to get this node (or a future value of it).")
  (node-type           [this basis]        "Return the node type that created this node.")
  (property-types      [this basis]        "Return the combined map of compile-time and runtime properties")
  (get-property        [this basis property] "Return the value of the named property")
  (set-property        [this basis property value] "Set the named property")
  (clear-property      [this basis property] "Clear the named property (this is only valid for override nodes)")
  (produce-value       [this output evaluation-context] "Return the value of the named output")
  (override-id         [this] "Return the ID of the override this node belongs to, if any")
  (original            [this] "Return the ID of the original of this node, if any"))

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
  (override-node    [this original-id override-id])
  (add-override     [this override-id override])
  (delete-override  [this override-id])
  (connect          [this src-id src-label tgt-id tgt-label])
  (disconnect       [this src-id src-label tgt-id tgt-label])
  (connected?       [this src-id src-label tgt-id tgt-label])
  (dependencies     [this node-id-output-label-pairs]
    "Follow arcs through the graphs, from outputs to the inputs
     connected to them, and from those inputs to the downstream
     outputs that use them, and so on. Continue following links until
     all reachable outputs are found.

     Returns a collection of [node-id output-label] pairs.")
  (original-node    [this node-id]))

(defn protocol? [x] (and (map? x) (contains? x :on-interface)))

(defprotocol PropertyType
  (property-value-type         [this]   "Prismatic schema for property value type")
  (property-default-value      [this])
  (property-tags               [this]))

(defn property-type? [x] (satisfies? PropertyType x))

(def Properties {:properties {s/Keyword {:node-id                              NodeID
                                         (s/optional-key :validation-problems) s/Any
                                         :value                                (s/either s/Any ErrorValue)
                                         :type                                 (s/protocol PropertyType)
                                         s/Keyword                             s/Any}}
                 (s/optional-key :display-order) [(s/either s/Keyword [(s/one String "category") s/Keyword])]})

(defprotocol Dynamics
  (dynamic-attributes          [this] "Return a map from label to fnk"))

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

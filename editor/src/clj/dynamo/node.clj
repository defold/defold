(ns dynamo.node
  "This namespace has two fundamental jobs. First, it provides the operations
for defining and instantiating nodes: `defnode` and `construct`.

Second, this namespace defines some of the basic node types and mixins."
  (:require [dynamo.types :as t :refer :all]
            [dynamo.util :refer :all]
            [internal.graph.types :as gt]
            [plumbing.core :as pc]))

(defn construct
  "Creates an instance of a node. The node-type must have been
  previously defined via `defnode`.

  The node's properties will all have their default values. The caller
  may pass key/value pairs to override properties.

  A node that has been constructed is not connected to anything and it
  doesn't exist in any graph yet.

  Example:
  (defnode GravityModifier
    (property acceleration t/Int (default 32))

  (construct GravityModifier :acceleration 16)"
  [node-type & {:as args}]
  (assert (::ctor node-type))
  ((::ctor node-type) args))

(defn abort
  "Abort production function and use substitute value."
  [msg & args]
  (throw (apply ex-info msg args)))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'."
  [basis node type & {:as body}]
  (gt/process-one-event (gt/node-by-id basis (gt/node-id node)) (assoc body :type type)))

;; ---------------------------------------------------------------------------
;; Intrinsics
;; ---------------------------------------------------------------------------
(defn- gather-property [this prop]
  (let [type     (-> this gt/properties prop)
        value    (get this prop)
        problems (t/property-validate type value)]
    {:node-id             (gt/node-id this)
     :value               value
     :type                type
     :validation-problems problems}))

(pc/defnk gather-properties :- t/Properties
  "Production function that delivers the definition and value
for all properties of this node."
  [this]
  (let [property-names (-> this gt/properties keys)]
    (zipmap property-names (map (partial gather-property this) property-names))))

(def node-intrinsics
  [(list 'output 'self `t/Any `(pc/fnk [~'this] ~'this))
   (list 'output 'properties `t/Properties `gather-properties)])

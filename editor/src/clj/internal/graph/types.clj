(ns internal.graph.types
  (:require [schema.core :as s]
            [schema.macros :as sm]))

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

(ns editor.core
  "Essential node types"
  (:require [clojure.set :as set]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.types :as types]))

(set! *warn-on-reflection* true)

;; ---------------------------------------------------------------------------
;; Copy/paste support
;; ---------------------------------------------------------------------------
(def ^:dynamic *serialization-handlers*
  {:read  {"class"         (transit/read-handler
                            (fn [rep] (java.lang.Class/forName ^String rep)))}
   :write {java.lang.Class (transit/write-handler
                            (constantly "class")
                            (fn [^Class v] (.getName v)))}})

(defn register-read-handler!
  [tag handler]
  (alter-var-root #'*serialization-handlers* assoc-in [:read tag] handler))

(defn register-write-handler!
  [class handler]
  (alter-var-root #'*serialization-handlers* assoc-in [:write class] handler))

(defn register-record-type!
  [type]
  (alter-var-root #'*serialization-handlers*
                  #(-> %
                       (update :read merge (transit/record-read-handlers type))
                       (update :write merge (transit/record-write-handlers type)))))

(defn read-handlers [] (:read *serialization-handlers*))

(defn write-handlers [] (:write *serialization-handlers*))

;; ---------------------------------------------------------------------------
;; Bootstrapping the core node types
;; ---------------------------------------------------------------------------

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :_node-id output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes g/Any :array :cascade-delete))

(defn scope
  ([node-id]
   (scope (g/now) node-id))
  ([basis node-id]
   (assert (some? node-id))
   (let [[_ _ scope _] (first
                         (filter
                           (fn [[src src-lbl tgt tgt-lbl]]
                             (and (= src-lbl :_node-id)
                                  (= tgt-lbl :nodes)))
                           (g/outputs basis node-id)))]
     scope)))

(defn scope-of-type
  ([node-id node-type]
   (scope-of-type (g/now) node-id node-type))
  ([basis node-id node-type]
   (when-let [scope-id (scope basis node-id)]
     (if (g/node-instance? basis node-type scope-id)
       scope-id
       (recur basis scope-id node-type)))))


(g/defnode Saveable
  "Mixin. Content root nodes (i.e., top level nodes for an editor tab) can inherit
this node to indicate that 'Save' is a meaningful action.

Inheritors are required to supply a production function for the :save output."
  (output save g/Keyword :abstract))


(g/defnode ResourceNode
  "Mixin. Any node loaded from the filesystem should inherit this."
  (property filename types/PathManipulation (dynamic visible (g/constantly false)))

  (output content g/Any :abstract))

(g/defnode OutlineNode
  "Mixin. Any OutlineNode can be shown in an outline view.

Inputs:
- children `[OutlineItem]` - Input values that will be nested beneath this node.

Outputs:
- tree `OutlineItem` - A single value that contains the display info for this node and all its children."
  (output outline-children [types/OutlineItem] (g/constantly []))
  (output outline-label    g/Str :abstract)
  (output outline-commands [types/OutlineCommand] (g/constantly []))
  (output outline-tree     types/OutlineItem
          (g/fnk [this outline-label outline-commands outline-children]
               {:label outline-label
                ;; :icon "my type of icon"
                :node-ref (g/node-id this)
                :commands outline-commands
                :children outline-children})))

(defprotocol Adaptable
  (adapt [this t]))

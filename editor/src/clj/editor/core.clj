(ns editor.core
  "Essential node types"
  (:require [clojure.set :as set]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.types :as types]
            [inflections.core :as inflect]))


;; ---------------------------------------------------------------------------
;; Copy/paste support
;; ---------------------------------------------------------------------------
(def ^:dynamic *serialization-handlers*
  {:read  {"class"         (transit/read-handler
                            (fn [rep] (java.lang.Class/forName ^String rep)))}
   :write {java.lang.Class (transit/write-handler
                            (constantly "class")
                            ; TODO - fix reflection warning
                            (fn [v] (.getName v)))}})

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
;; Dependency Injection
;; ---------------------------------------------------------------------------

(defn compatible?
  [basis [out-node out-label in-node in-label]]
  (let [out-type          (some-> out-node (->> (g/node-type* basis)) (g/output-type out-label))
        in-type           (some-> in-node  (->> (g/node-type* basis)) (g/input-type in-label))
        in-cardinality    (some-> in-node  (->> (g/node-type* basis)) (g/input-cardinality in-label))
        type-compatible?  (g/type-compatible? out-type in-type)
        names-compatible? (or (= out-label in-label)
                              (and (= in-label (inflect/plural out-label)) (= in-cardinality :many)))]
    (and type-compatible? names-compatible?)))

(defn injection-candidates
  [basis out-nodes in-nodes]
  (into #{}
     (filter #(compatible? basis %)
        (for [in-node   in-nodes
              in-label  (->> in-node (g/node-type* basis) g/injectable-inputs)
              out-node  out-nodes
              out-label (keys (->> out-node (g/node-type* basis) g/transform-types))]
            [out-node out-label in-node in-label]))))

(defn inject-dependencies
  "Examine the suppliers and consumers to see if any there are any
  connections to make from outputs to compatible, injectable
  inputs. Both suppliers and consumers must be collections.
  Returns a seq of transaction steps."
  [suppliers consumers]
  (assert (seq suppliers))
  (assert (seq consumers))
  (let [basis           (g/now)
        new-connections (remove
                         (fn [[out out-label in in-label]]
                           (or
                            (= out in)
                            (g/connected? basis out out-label in in-label)))
                         (injection-candidates basis suppliers consumers))]
    (map #(apply g/connect %) new-connections)))

;; ---------------------------------------------------------------------------
;; Bootstrapping the core node types
;; ---------------------------------------------------------------------------

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :_node-id output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes g/Any :array :cascade-delete))

(defn scope [node-id]
  (let [[_ _ scope _] (first
                        (filter
                          (fn [[src src-lbl tgt tgt-lbl]]
                            (and (= src-lbl :_node-id)
                                 (= tgt-lbl :nodes)))
                          (g/outputs node-id)))]
    scope))

(g/defnode Saveable
  "Mixin. Content root nodes (i.e., top level nodes for an editor tab) can inherit
this node to indicate that 'Save' is a meaningful action.

Inheritors are required to supply a production function for the :save output."
  (output save g/Keyword :abstract))


(g/defnode ResourceNode
  "Mixin. Any node loaded from the filesystem should inherit this."
  (property filename (g/protocol types/PathManipulation) (dynamic visible (g/always false)))

  (output content g/Any :abstract))

(g/defnode OutlineNode
  "Mixin. Any OutlineNode can be shown in an outline view.

Inputs:
- children `[OutlineItem]` - Input values that will be nested beneath this node.

Outputs:
- tree `OutlineItem` - A single value that contains the display info for this node and all its children."
  (output outline-children [types/OutlineItem] (g/always []))
  (output outline-label    g/Str :abstract)
  (output outline-commands [types/OutlineCommand] (g/always []))
  (output outline-tree     types/OutlineItem
          (g/fnk [this outline-label outline-commands outline-children :- [types/OutlineItem]]
               {:label outline-label
                ;; :icon "my type of icon"
                :node-ref (g/node-id this)
                :commands outline-commands
                :children outline-children})))

(defprotocol MultiNode
  (sub-nodes [self] "Return all contained nodes"))

(defprotocol ICreate
  "A node may implement this protocol if it needs to react after it is created."
  (post-create [this basis message] "Process post-creation message. The message is any data structure. By convention, it is usually a map."))

(ns editor.core
  "Essential node types"
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [dynamo.node :as dn]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [inflections.core :as inflect]
            [internal.graph :as ig]
            [internal.node :as in]
            [internal.transaction :as it]))

;; ---------------------------------------------------------------------------
;; Dependency Injection
;; ---------------------------------------------------------------------------

;; TODO - inject-new-nodes should not require any internal namespaces

(defn- check-single-type
  [out in]
  (or
   (= t/Any in)
   (= out in)
   (and (class? in) (class? out) (.isAssignableFrom ^Class in out))))

(defn type-compatible?
  [output-schema input-schema expect-collection?]
  (let [out-t-pl? (coll? output-schema)
        in-t-pl?  (coll? input-schema)]
    (or
     (= t/Any input-schema)
     (and expect-collection? (= [t/Any] input-schema))
     (and expect-collection? in-t-pl? (check-single-type output-schema (first input-schema)))
     (and (not expect-collection?) (check-single-type output-schema input-schema))
     (and (not expect-collection?) in-t-pl? out-t-pl? (check-single-type (first output-schema) (first input-schema))))))

(defn compatible?
  [[out-node out-label out-type in-node in-label in-type]]
  (cond
   (and (= out-label in-label) (type-compatible? out-type in-type false))
   [out-node out-label in-node in-label]

   (and (= (inflect/plural out-label) in-label) (type-compatible? out-type in-type true))
   [out-node out-label in-node in-label]))

(defn injection-candidates
  [targets nodes]
  (into #{}
     (keep compatible?
        (for [target  targets
              i       (g/injectable-inputs target)
              :let    [i-l (get (g/input-types target) i)]
              node    nodes
              [o o-l] (g/transform-types node)]
            [node o o-l target i i-l]))))

(defn inject-new-nodes
  "Implementation function that performs dependency injection for nodes.
This function should not be called directly."
  [transaction graph self label kind inputs-affected]
  (when (inputs-affected :nodes)
    (let [nodes-before-txn         (cons self (in/get-inputs (ds/pre-transaction-graph transaction) self :nodes))
          nodes-after-txn          (in/get-inputs (ds/in-transaction-graph transaction) self :nodes)
          nodes-before-txn-ids     (into #{} (map :_id nodes-before-txn))
          new-nodes-in-scope       (remove nodes-before-txn-ids nodes-after-txn)
          out-from-new-connections (injection-candidates nodes-before-txn new-nodes-in-scope)
          in-to-new-connections    (injection-candidates new-nodes-in-scope nodes-before-txn)
          between-new-nodes        (injection-candidates new-nodes-in-scope new-nodes-in-scope)
          candidates               (set/union out-from-new-connections in-to-new-connections between-new-nodes)
          candidates               (remove (fn [[out out-label in in-label]]
                                             (or
                                              (= (:_id out) (:_id in))
                                              (ig/connected? graph (:_id out) out-label (:_id in) in-label)))
                                           candidates)]
      (for [connection candidates]
        (apply it/connect connection)))))

;; ---------------------------------------------------------------------------
;; Cascading delete
;; ---------------------------------------------------------------------------

;; TODO - dispose-nodes should not require any internal namespaces.

(defn dispose-nodes
  "Trigger to dispose nodes from a scope when the scope is destroyed.
This should not be called directly."
  [transaction graph self label kind]
  (when (ds/is-deleted? transaction self)
    (let [graph-before-deletion (ds/pre-transaction-graph transaction)
          nodes-to-delete       (in/get-inputs graph-before-deletion self :nodes)]
      (for [n nodes-to-delete]
        (it/delete-node n)))))

;; ---------------------------------------------------------------------------
;; Bootstrapping the core node types
;; ---------------------------------------------------------------------------

;; TODO - lookup should not require any internal namespaces.

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :self output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes [t/Any])

  (property tag      t/Keyword)
  (property parent   (t/protocol t/NamingContext))

  (trigger dependency-injection :input-connections #'inject-new-nodes)
  (trigger garbage-collection   :deleted           #'dispose-nodes)

  t/NamingContext
  (lookup
   [this nm]
   (let [nodes (g/node-value this :nodes)]
     (first (filter #(= nm (:name %)) nodes)))))


(g/defnode Saveable
  "Mixin. Content root nodes (i.e., top level nodes for an editor tab) can inherit
this node to indicate that 'Save' is a meaningful action.

Inheritors are required to supply a production function for the :save output."
  (output save t/Keyword :abstract))


(g/defnode ResourceNode
  "Mixin. Any node loaded from the filesystem should inherit this."
  (property filename (t/protocol t/PathManipulation) (visible false))

  (output content t/Any :abstract))


(g/defnode AutowireResources
  "Mixin. Nodes with this behavior automatically keep their graph connections
up to date with their resource properties."
  (trigger autowire-resources :property-touched #'dn/connect-resource))


(g/defnode OutlineNode
  "Mixin. Any OutlineNode can be shown in an outline view.

Inputs:
- children `[OutlineItem]` - Input values that will be nested beneath this node.

Outputs:
- tree `OutlineItem` - A single value that contains the display info for this node and all its children."
  (output outline-children [t/OutlineItem] (g/fnk [] []))
  (output outline-label    t/Str :abstract)
  (output outline-commands [t/OutlineCommand] (g/fnk [] []))
  (output outline-tree     t/OutlineItem
          (g/fnk [this outline-label outline-commands outline-children :- [t/OutlineItem]]
               {:label outline-label
                ;; :icon "my type of icon"
                :node-ref (g/node-ref this)
                :commands outline-commands
                :children outline-children})))

(g/defnode RootScope
  "There should be exactly one RootScope in the graph, with ID 1.
RootScope has no parent."
  (inherits Scope)
  (property tag t/Keyword (default :root)))

(defmethod print-method RootScope__
  [^RootScope__ v ^java.io.Writer w]
  (.write w (str "<RootScope{:_id " (:_id v) "}>")))

(defn make-graph []
  (let [root (dn/construct RootScope :_id 1)]
    (ig/add-labeled-node (ig/empty-graph) (ig/inputs root) (ig/outputs root) root)))

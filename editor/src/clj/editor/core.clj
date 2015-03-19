(ns editor.core
  "Essential node types"
  (:require [dynamo.graph :as g]
            [dynamo.node :as dn]
            [dynamo.types :as t]
            [internal.node :as in]))

;; TODO - lookup should not require any internal namespaces.

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :self output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes [t/Any])

  (property tag      t/Keyword)
  (property parent   (t/protocol t/NamingContext))

  (trigger dependency-injection :input-connections #'dn/inject-new-nodes)
  (trigger garbage-collection   :deleted  #'dn/dispose-nodes)

  t/NamingContext
  (lookup
   [this nm]
   (let [nodes (in/get-inputs (-> this :world-ref deref :graph) this :nodes)]
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

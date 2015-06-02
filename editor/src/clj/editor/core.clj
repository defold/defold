(ns editor.core
  "Essential node types"
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [dynamo.types :as t]
            [inflections.core :as inflect]))

;; ---------------------------------------------------------------------------
;; Dependency Injection
;; ---------------------------------------------------------------------------

;; TODO - inject-new-nodes should not require any internal namespaces


(defn compatible?
  [[out-node out-label in-node in-label]]
  (let [out-type          (-> out-node g/node-type (g/output-type out-label))
        in-type           (-> in-node  g/node-type (g/input-type in-label))
        in-cardinality    (-> in-node  g/node-type (g/input-cardinality in-label))
        type-compatible?  (g/type-compatible? out-type in-type)
        names-compatible? (or (= out-label in-label)
                              (and (= in-label(inflect/plural out-label)) (= in-cardinality :many)))]
    (and type-compatible? names-compatible?)))

(defn injection-candidates
  [in-nodes out-nodes]
  (into #{}
     (filter compatible?
        (for [in-node   in-nodes
              in-label  (g/injectable-inputs in-node)
              out-node  out-nodes
              out-label (keys (g/transform-types out-node))]
            [out-node out-label in-node in-label]))))

(defn inject-new-nodes
  "Implementation function that performs dependency injection for nodes.
This function should not be called directly."
  [transaction basis self label kind inputs-affected]
  (when (inputs-affected :nodes)
    (let [nodes-before-txn         (cons self (if-let [original-self (g/refresh self)]
                                                (g/node-value original-self :nodes)
                                                []))
          nodes-after-txn          (g/node-value basis self :nodes)
          nodes-before-txn-ids     (into #{} (map g/node-id nodes-before-txn))
          new-nodes-in-scope       (remove #(nodes-before-txn-ids (g/node-id %)) nodes-after-txn)
          out-from-new-connections (injection-candidates nodes-before-txn new-nodes-in-scope)
          in-to-new-connections    (injection-candidates new-nodes-in-scope nodes-before-txn)
          between-new-nodes        (injection-candidates new-nodes-in-scope new-nodes-in-scope)
          candidates               (set/union out-from-new-connections in-to-new-connections between-new-nodes)
          candidates               (remove
                                    (fn [[out out-label in in-label]]
                                      (or
                                       (= (g/node-id out) (g/node-id in))
                                       (g/connected? (g/transaction-basis transaction)
                                                     (g/node-id out) out-label
                                                     (g/node-id in)  in-label)))
                                    candidates)]
      (for [connection candidates]
        (apply g/connect connection)))))

;; ---------------------------------------------------------------------------
;; Cascading delete
;; ---------------------------------------------------------------------------

(defn dispose-nodes
  "Trigger to dispose nodes from a scope when the scope is
  destroyed. This should not be called directly."
  [transaction graph self label kind]
  (when (g/is-deleted? transaction self)
    (for [node-to-delete (g/node-value self :nodes)]
      (g/delete-node node-to-delete))))

;; ---------------------------------------------------------------------------
;; Bootstrapping the core node types
;; ---------------------------------------------------------------------------

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :self output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes t/Any :array)

  (property tag      t/Keyword (visible (g/fnk [] false)))
  (property parent   t/Any (visible (g/fnk [] false)))

  (trigger dependency-injection :input-connections #'inject-new-nodes)
  (trigger garbage-collection   :deleted           #'dispose-nodes))


(g/defnode Saveable
  "Mixin. Content root nodes (i.e., top level nodes for an editor tab) can inherit
this node to indicate that 'Save' is a meaningful action.

Inheritors are required to supply a production function for the :save output."
  (output save t/Keyword :abstract))


(g/defnode ResourceNode
  "Mixin. Any node loaded from the filesystem should inherit this."
  (property filename (t/protocol t/PathManipulation) (visible (g/fnk [] false)))

  (output content t/Any :abstract))


#_(g/defnode AutowireResources
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
                :node-ref (g/node-id this)
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

(defprotocol MultiNode
  (sub-nodes [self] "Return all contained nodes"))

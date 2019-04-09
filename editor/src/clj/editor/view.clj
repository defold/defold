(ns editor.view
  (:require [dynamo.graph :as g]))

(g/defnode WorkbenchView
  (input resource-node g/NodeID)
  (input node-id+resource g/Any :substitute nil)
  (input dirty? g/Bool :substitute false)
  (output view-data g/Any (g/fnk [_node-id node-id+resource]
                            [_node-id (when-let [[node-id resource] node-id+resource]
                                        {:resource-node node-id
                                         :resource resource})]))

  ;; we cache view-dirty? to avoid recomputing dirty? on the resource
  ;; node for every open tab whenever one resource changes
  (output view-dirty? g/Any :cached (g/fnk [_node-id dirty?] [_node-id dirty?]))

  ;; State to be stored with undo steps. We use it to restore the view to the
  ;; state it was in at the time when the action was performed. When registering
  ;; a view type, you can optionally provide a :set-state-fn that we will call
  ;; with the view-node and this view-state. It should return a two-element
  ;; vector containing the the transaction steps required to restore the state
  ;; of the view, as well as a boolean that signals if the view was drastically
  ;; changed. If so, we defer changes to the project graph until the next
  ;; invocation undo / redo command. The operation cannot not proceed as long as
  ;; this flag returns true.
  (output view-state g/Any (g/constantly nil)))

(defn connect-resource-node [view resource-node]
  (concat
    (g/connect resource-node :_node-id view :resource-node)
    (g/connect resource-node :valid-node-id+resource view :node-id+resource)
    (g/connect resource-node :dirty? view :dirty?)))

(defn connect-app-view [view app-view]
  (concat
    (g/connect view :view-data app-view :open-views)
    (g/connect view :view-dirty? app-view :open-dirty-views)))

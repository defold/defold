(ns editor.view
  (:require [dynamo.graph :as g]
            [editor.resource :as resource]))

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
  (output view-dirty? g/Any :cached (g/fnk [_node-id dirty?] [_node-id dirty?])))

(defn connect-resource-node [view resource-node]
  (concat
    (g/connect resource-node :_node-id view :resource-node)
    (g/connect resource-node :node-id+resource view :node-id+resource)
    (g/connect resource-node :dirty? view :dirty?)))

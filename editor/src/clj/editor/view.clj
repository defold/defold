(ns editor.view
  (:require [dynamo.graph :as g]
            [editor.resource :as resource]))

(g/defnode WorkbenchView
  (input resource-node g/NodeID)
  (input node-id+resource g/Any :substitute nil)
  (output view-data g/Any (g/fnk [_node-id node-id+resource]
                            [_node-id (when-let [[node-id resource] node-id+resource]
                                        {:resource-node node-id
                                         :resource resource})])))

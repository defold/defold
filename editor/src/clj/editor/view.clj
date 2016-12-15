(ns editor.view
  (:require [dynamo.graph :as g]))

(g/defnode WorkbenchView
  (input resource-node g/NodeID)
  (input node-id+resource g/Any)
  (output view-data g/Any (g/fnk [_node-id node-id+resource] [_node-id (when node-id+resource
                                                                         {:resource-node (first node-id+resource)
                                                                          :resource (second node-id+resource)})])))

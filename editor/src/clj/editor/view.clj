(ns editor.view
  (:require [dynamo.graph :as g]))

(g/defnode WorkbenchView
  (input node-id+resource g/Any)
  (output view-data g/Any (g/fnk [_node-id node-id+resource] [_node-id {:resource-node (first node-id+resource)
                                                                        :resource (second node-id+resource)}])))

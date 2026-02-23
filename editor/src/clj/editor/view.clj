;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.view
  (:require [dynamo.graph :as g]))

(def default-sidebar-panes [:outline-pane :properties-pane])

(g/defnode WorkbenchView
  (input resource-node g/NodeID)
  (input node-id+type+resource g/Any :substitute nil)
  (input dirty g/Bool :substitute false)
  ;; Overridable output describing right sidebar panes for this view, in display order.
  ;; Each value can be :outline-pane, :properties-pane, or a cljfx description.
  (output sidebar-panes g/Any (g/constantly default-sidebar-panes))
  (output view-data g/Any (g/fnk [_node-id node-id+type+resource]
                            [_node-id (when-let [[node-id type resource] node-id+type+resource]
                                        {:resource-node node-id
                                         :resource-node-type type
                                         :resource resource})]))
  ;; TODO(save-value-cleanup): Merge dirty state into view-data?
  (output view-dirty g/Any (g/fnk [_node-id dirty] [_node-id dirty]))
  (output view-sidebar-panes g/Any (g/fnk [_node-id sidebar-panes] [_node-id sidebar-panes])))

(defn connect-resource-node [view resource-node]
  (concat
    (g/connect resource-node :_node-id view :resource-node)
    (g/connect resource-node :valid-node-id+type+resource view :node-id+type+resource)
    (g/connect resource-node :dirty view :dirty)))

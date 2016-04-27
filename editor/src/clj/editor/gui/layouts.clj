(ns editor.gui.layouts
  (:require [dynamo.graph :as g]
            [editor.outline :as outline]
            [editor.gui.util :as gutil]
            [editor.gui.icons :as icons]
            [editor.defold-project :as project]))

(g/defnode LayoutsNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines] (gutil/gen-outline-fnk-body _node-id child-outlines "Layouts" 4 false []))))

(g/defnode LayoutNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property nodes g/Any)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon icons/layout-icon}))
  (output pb-msg g/Any
          (g/fnk [name nodes]
                 {:name name
                  :nodes nodes})))

(defn attach-layout [self layouts-node layout]
  (concat
    (g/connect layout :_node-id self :nodes)
    (g/connect layout :node-outline layouts-node :child-outlines)
    (g/connect layout :name self :layout-names)
    (g/connect layout :pb-msg self :layout-msgs)))

(defn add-layout-handler [project {:keys [scene parent node-type]}]
  (let [name (outline/resolve-id "layout" (g/node-value scene :layout-names))]
    (g/transact
      (concat
        (g/operation-label "Add Layout")
        (g/make-nodes (g/node-id->graph-id scene) [node [LayoutNode :name name]]
          (attach-layout scene parent node)
          (project/select project [node]))))))

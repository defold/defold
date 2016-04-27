(ns editor.gui.layers
  (:require [dynamo.graph :as g]
            [editor.gui.util :as gutil]
            [editor.outline :as outline]
            [editor.gui.icons :as icons]
            [editor.defold-project :as project]))

(g/defnode LayersNode
  (inherits outline/OutlineNode)
  (input child-indices g/Int :array)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines] (gutil/gen-outline-fnk-body _node-id child-outlines "Layers" 3 true []))))

(g/defnode LayerNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property index g/Int (dynamic visible false) (default 0))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name index]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon icons/layer-icon
                                                           :index index}))
  (output pb-msg g/Any (g/fnk [name index]
                              {:name name
                               :index index}))
  (output layer-id {g/Str g/NodeID} (g/fnk [_node-id name] {name _node-id})))


(defn attach-layer [self layers-node layer]
  (concat
    (g/connect layer :_node-id self :nodes)
    (g/connect layer :layer-id self :layer-ids)
    (g/connect layer :pb-msg self :layer-msgs)
    (g/connect layer :name self :layer-names)
    (g/connect layer :node-outline layers-node :child-outlines)
    (g/connect layer :index layers-node :child-indices)))

(defn add-layer-handler [project {:keys [scene parent node-type]}]
  (let [index (inc (reduce max 0 (g/node-value parent :child-indices)))
        name (outline/resolve-id "layer" (g/node-value scene :layer-names))]
    (g/transact
      (concat
        (g/operation-label "Add Layer")
        (g/make-nodes (g/node-id->graph-id scene) [node [LayerNode :name name :index index]]
          (attach-layer scene parent node)
          (project/select project [node]))))))

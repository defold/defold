(ns editor.gui.fonts
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.graph-util :as gu]
            [editor.gui.util :as gutil]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.validation :as validation]
            [editor.gui.icons :as icons])
  (:import editor.gl.shader.ShaderLifecycle))

(g/defnode FontsNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines] (gutil/gen-outline-fnk-body _node-id child-outlines "Fonts" 2 false []))))


(g/defnode FontNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property font resource/Resource
            (value (gu/passthrough font-resource))
            (set (project/gen-resource-setter [[:resource :font-resource]
                                               [:font-map :font-map]
                                               [:font-data :font-data]
                                               [:gpu-texture :gpu-texture]
                                               [:material-shader :font-shader]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource font)))

  (input font-resource resource/Resource)
  (input font-map g/Any)
  (input font-data font/FontData)
  (input font-shader ShaderLifecycle)
  (input gpu-texture g/Any)

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any :cached (g/fnk [dep-build-targets] dep-build-targets))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon icons/font-icon}))
  (output pb-msg g/Any (g/fnk [name font-resource]
                              {:name name
                               :font (gutil/proj-path font-resource)}))
  (output font-map g/Any (g/fnk [font-map] font-map))
  (output font-data font/FontData (g/fnk [font-data] font-data))
  (output gpu-texture g/Any (g/fnk [gpu-texture] gpu-texture))
  (output font-shader ShaderLifecycle (g/fnk [font-shader] font-shader))
  (output font-id {g/Str g/NodeID} (g/fnk [_node-id name] {name _node-id})))

(defn attach-font
  ([self fonts-node font]
    (attach-font self fonts-node font false))
  ([self fonts-node font default?]
    (concat
      (g/connect font :_node-id self :nodes)
      (g/connect font :font-id self :font-ids)
      (g/connect font :dep-build-targets self :dep-build-targets)
      (if (not default?)
        (concat
          (g/connect font :name self :font-names)
          (g/connect font :pb-msg self :font-msgs)
          (g/connect font :node-outline fonts-node :child-outlines))
        []))))

(defn add-font-handler [project {:keys [scene parent node-type]}]
  (when-let [resource (gutil/browse project ["font"])]
    (let [name (outline/resolve-id (gutil/resource->id resource) (g/node-value scene :font-names))]
      (g/transact
        (concat
          (g/operation-label "Add Font")
          (g/make-nodes (g/node-id->graph-id scene) [node [FontNode :name name :font resource]]
            (attach-font scene parent node)
            (project/select project [node])))))))

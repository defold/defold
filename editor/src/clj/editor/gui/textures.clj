(ns editor.gui.textures
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.gui.util :as gutil]
            [editor.outline :as outline]
            [editor.material :as material]
            [editor.gl.texture :as texture]
            [editor.validation :as validation]
            [editor.resource :as resource]
            [editor.gui.gui-scene-node :as gsn]
            [editor.gui.icons :as icons]
            [editor.defold-project :as project])
  (:import java.awt.image.BufferedImage
           com.defold.editor.pipeline.TextureSetGenerator$UVTransform))

(g/defnode TexturesNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines] (gutil/gen-outline-fnk-body _node-id child-outlines "Textures" 1 false []))))

(g/defnode TextureNode
  (inherits outline/OutlineNode)

  (property name g/Str)
  (property texture resource/Resource
            (value (gu/passthrough texture-resource))
            (set (project/gen-resource-setter [[:resource :texture-resource]
                                               [:packed-image :image]
                                               [:anim-data :anim-data]
                                               [:anim-ids :anim-ids]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource texture)))

  (input texture-resource resource/Resource)
  (input image BufferedImage)
  (input anim-data g/Any)
  (input anim-ids g/Any)
  (input image-texture g/NodeID :cascade-delete)
  (input samplers [{g/Keyword g/Any}])

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (g/fnk [dep-build-targets] dep-build-targets))

  (output anim-data g/Any :cached (g/fnk [_node-id name anim-data]
                                    (into {} (map (fn [[id data]] [(if id (format "%s/%s" name id) name) data]) anim-data))))
  (output node-outline outline/OutlineData (g/fnk [_node-id name]
                                             {:node-id _node-id
                                              :label name
                                              :icon icons/texture-icon}))
  (output pb-msg g/Any (g/fnk [name texture-resource]
                         {:name name
                          :texture (gutil/proj-path texture-resource)}))
  (output texture-id {g/Str g/NodeID} :cached (g/fnk [_node-id anim-ids name]
                                                     (let [texture-ids (if anim-ids
                                                                         (map #(format "%s/%s" name %) anim-ids)
                                                                         [name])]
                                                       (zipmap texture-ids (repeat _node-id)))))
  (output gpu-texture g/Any :cached (g/fnk [_node-id image samplers]
                                           (let [params (material/sampler->tex-params (first samplers))]
                                             (texture/set-params (texture/image-texture _node-id image) params)))))

(g/defnode ImageTextureNode
  (input image BufferedImage)
  (output packed-image BufferedImage (g/fnk [image] image))
  (output anim-data g/Any (g/fnk [^BufferedImage image]
                            {nil {:width (.getWidth image)
                                  :height (.getHeight image)
                                  :frames [{:tex-coords [[0 1] [0 0] [1 0] [1 1]]}]
                                  :uv-transforms [(TextureSetGenerator$UVTransform.)]}})))

(defn attach-texture [self textures-node texture]
  (concat
    (g/connect texture :_node-id self :nodes)
    (g/connect texture :texture-id self :texture-ids)
    (g/connect texture :dep-build-targets self :dep-build-targets)
    (g/connect texture :pb-msg self :texture-msgs)
    (g/connect texture :name self :texture-names)
    (g/connect texture :node-outline textures-node :child-outlines)
    (g/connect self :samplers texture :samplers)))

(defn make-texture-node [self parent name resource]
  (g/make-nodes (g/node-id->graph-id self) [texture [TextureNode
                                                     :name name
                                                     :texture resource]]
                (attach-texture self parent texture)))

(defn add-texture-handler [project {:keys [scene parent node-type]}]
  (when-let [resource (gutil/browse project ["atlas" "tilesource"])]
    (let [name (outline/resolve-id (gutil/resource->id resource) (g/node-value scene :texture-names))]
      (g/transact
        (concat
          (g/operation-label "Add Texture")
          (g/make-nodes (g/node-id->graph-id scene) [node [TextureNode :name name :texture resource]]
            (attach-texture scene parent node)
            (project/select project [node])))))))

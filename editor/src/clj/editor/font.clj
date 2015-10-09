(ns editor.font
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.pipeline.font-gen :as font-gen]
            [editor.image :as image]
            [editor.resource :as resource]
            [internal.render.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
           [com.dynamo.render.proto Font$FontDesc Font$FontMap]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def font-icon "icons/32/Icons_28-AT-Font.png")

; Node defs

(g/defnk produce-save-data [resource font-resource material-resource pb]
  (let [pb (assoc pb
             :font (resource/resource->proj-path font-resource)
             :material (resource/resource->proj-path material-resource))]
    {:resource resource
     :content (protobuf/map->str Font$FontDesc pb)}))

(defn- build-font [self basis resource dep-resources user-data]
  (let [project (project/get-project self)
        workspace (project/workspace project)
        font-map (assoc (:font-map user-data) :textures [(workspace/proj-path (second (first dep-resources)))])]
    {:resource resource :content (protobuf/map->bytes Font$FontMap font-map)}))

(g/defnk produce-build-targets [_node-id resource font-resource pb dep-build-targets]
  (let [project        (project/get-project _node-id)
        workspace      (project/workspace project)
        resolver       (partial workspace/resolve-workspace-resource workspace)
        ; Should be separate production function for rendering etc
        result         (font-gen/generate pb font-resource resolver)
        texture-target (image/make-texture-build-target workspace _node-id (:image result))]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-font
      :user-data {:font-map (:font-map result)}
      :deps (cons texture-target (flatten dep-build-targets))}]))

(g/defnode FontSourceNode
  (inherits project/ResourceNode))

(g/defnode FontNode
  (inherits project/ResourceNode)

  (property font (g/protocol resource/Resource)
    (value (g/fnk [font-resource] font-resource))
    (set (fn [basis self _ new-value]
           (if new-value
             (let [project (project/get-project self)]
               (project/connect-resource-node project new-value self [[:resource :font-resource]]))
             (g/disconnect-sources basis self :font-resource)))))
  (property material (g/protocol resource/Resource)
    (value (g/fnk [material-resource] material-resource))
    (set (fn [basis self _ new-value]
           (if new-value
             (let [project (project/get-project self)]
               (project/connect-resource-node project new-value self [[:resource :material-resource]
                                                                      [:build-targets :dep-build-targets]]))
             (for [label [:material-resource :dep-build-targets]]
               (g/disconnect-sources basis self label))))))
  (property pb g/Any)

  (input dep-build-targets g/Any :array)
  (input font-resource (g/protocol resource/Resource))
  (input material-resource (g/protocol resource/Resource))

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Font" :icon font-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-font [project self resource]
  (let [font (protobuf/read-text Font$FontDesc resource)]
    (concat
     (g/set-property self :pb font)
     (g/set-property self :font (workspace/resolve-resource resource (:font font)))
     (g/set-property self :material (workspace/resolve-resource resource (:material font))))))

(defn register-resource-types [workspace]
  (concat
    (workspace/register-resource-type workspace
                                     :ext "font"
                                     :label "Font"
                                     :node-type FontNode
                                     :load-fn load-font
                                     :icon font-icon)
    (workspace/register-resource-type workspace
                                      :ext ["ttf" "fnt"]
                                      :label "Font"
                                      :node-type FontSourceNode
                                      :icon font-icon
                                      :view-types [:default])))

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

(g/defnk produce-save-data [resource pb]
  {:resource resource
   :content (protobuf/map->str Font$FontDesc pb)})

(defn- build-font [self basis resource dep-resources user-data]
  (let [project (project/get-project self)
        workspace (project/workspace project)
        font-map (assoc (:font-map user-data) :textures [(workspace/proj-path (second (first dep-resources)))])]
    {:resource resource :content (protobuf/map->bytes Font$FontMap font-map)}))

(g/defnk produce-build-targets [_node-id resource pb dep-build-targets]
  (let [; Should use a separate resource node to obtain the font file
        font-resource  (workspace/resolve-resource resource (:font pb))
        project        (project/get-project _node-id)
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

(g/defnode FontNode
  (inherits project/ResourceNode)

  (property pb g/Any)

  (input dep-build-targets g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Font" :icon font-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-font [project self resource]
  (let [font (protobuf/read-text Font$FontDesc resource)]
    (concat
     (g/set-property self :pb font)
     (for [ref [:font :material]]
       (project/connect-resource-node project (workspace/resolve-resource resource (ref font)) self [[:build-targets :dep-build-targets]])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "font"
                                    :label "Font"
                                    :node-type FontNode
                                    :load-fn load-font
                                    :icon font-icon))

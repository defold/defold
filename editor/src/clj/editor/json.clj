(ns editor.json
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [clojure.data.json :as json]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

; TODO - missing icon
(def json-icon "icons/32/Icons_29-AT-Unkown.png")

(g/defnk produce-content [resource]
  (let [content (slurp resource)]
    (json/read-str content)))

(g/defnode JsonNode
  (inherits project/ResourceNode)

  (output content g/Any :cached produce-content))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "json"
                                    :node-type JsonNode
                                    :icon json-icon))

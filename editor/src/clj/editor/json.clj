(ns editor.json
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [clojure.data.json :as json]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)

;; TODO - missing icon
(def json-icon "icons/32/Icons_29-AT-Unknown.png")

(defonce ^:private json-loaders (atom {}))

(g/defnode JsonNode
  (inherits resource-node/ResourceNode)
  (property content g/Any)
  (input structure g/Any)
  (output structure g/Any (g/fnk [structure] structure)))

(defn load-json [project self resource]
  (let [content (-> (slurp resource)
                  json/read-str)
        [load-fn new-content] (some (fn [[loader-id {:keys [accept-fn load-fn]}]]
                                      (when-let [new-content (accept-fn content)]
                                        [load-fn new-content]))
                                    @json-loaders)]
    (if load-fn
      (concat
        (g/set-property self :content new-content)
        (load-fn self new-content))
      (g/set-property self :content content))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "json"
                                    :node-type JsonNode
                                    :load-fn load-json
                                    :icon json-icon))

(defn register-json-loader [loader-id accept-fn load-fn]
  (swap! json-loaders assoc loader-id {:accept-fn accept-fn :load-fn load-fn}))

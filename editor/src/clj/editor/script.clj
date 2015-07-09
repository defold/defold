(ns editor.script
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
            [internal.render.pass :as pass])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.google.protobuf ByteString]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def script-defs [{:ext "script"
                   :icon "icons/pictures.png"
                   :tags #{:component}}
                  {:ext "render_script"
                   :icon "icons/pictures.png"}
                  {:ext "gui_script"
                   :icon "icons/pictures.png"}
                  {:ext "lua"
                   :icon "icons/pictures.png"}
                  {:ext "java"
                   :icon "icons/pictures.png"}])

(g/defnk produce-save-data [resource content]
  {:resource resource
   :content content})

(defn- build-script [self basis resource dep-resources user-data]
  {:resource resource :content (protobuf/map->bytes Lua$LuaModule
                                                    {:source {:script (ByteString/copyFromUtf8 (:content user-data))
                                                              :filename (workspace/proj-path (:resource resource))}
                                                     ; TODO - fix this
                                                     :modules []
                                                     :resources []
                                                     ; TODO - properties
                                                     })})

(g/defnk produce-build-targets [node-id resource content]
  [{:node-id node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-script
    :user-data {:content content}}])

(g/defnode ScriptNode
  (inherits project/ResourceNode)

  (property content g/Any (dynamic visible (g/always false)))

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-script [project self input]
  (let [content (slurp input)
        resource (:resource self)]
    (concat
      (g/set-property self :content content))))

(defn- register [workspace def]
  (workspace/register-resource-type workspace
                                    :ext (:ext def)
                                    :node-type ScriptNode
                                    :load-fn load-script
                                    :icon (:icon def)
                                    :tags (:tags def)))

(defn register-resource-types [workspace]
  (for [def script-defs]
    (register workspace def)))

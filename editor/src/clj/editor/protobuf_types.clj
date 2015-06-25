(ns editor.protobuf-types
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
  (:import [com.dynamo.input.proto Input$InputBinding]
           [com.dynamo.render.proto Render$RenderPrototypeDesc]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def pb-types {:input-binding {:ext "input_binding"
                               :icon "icons/pictures.png"
                               :pb-class Input$InputBinding}
               :render {:ext "render"
                        :icon "icons/pictures.png"
                        :pb-class Render$RenderPrototypeDesc}})

(g/defnk produce-save-data [resource type pb]
  {:resource resource
   :content (protobuf/map->str (get-in pb-types [type :pb-class]) pb)})

(defn- build-pb [self basis resource dep-resources user-data]
  {:resource resource :content (protobuf/map->bytes (:pb-class user-data) (:pb user-data))})

(g/defnk produce-build-targets [node-id project-id resource pb type]
  [{:node-id node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-pb
    :user-data {:pb pb
                :pb-class (get-in pb-types [type :pb-class])}}])

(g/defnode ProtobufNode
  (inherits project/ResourceNode)

  (property pb g/Any (visible (g/fnk [] false)))
  (property type g/Any (visible (g/fnk [] false)))

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-pb [project self input type pb-class]
  (let [pb (protobuf/read-text pb-class input)
        resource (:resource self)]
    (concat
      (g/set-property self :pb pb)
      (g/set-property self :type type))))

(defn- register [workspace type]
  (let [pb-type (get pb-types type)]
    (workspace/register-resource-type workspace
                                     :ext (:ext pb-type)
                                     :node-type ProtobufNode
                                     :load-fn (fn [project self input] (load-pb project self input type (:pb-class pb-type)))
                                     :icon (:icon pb-type))))

(defn register-resource-types [workspace]
  (concat
    (register workspace :input-binding)
    (register workspace :render)))

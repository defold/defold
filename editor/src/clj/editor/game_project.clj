(ns editor.game-project
  (:require [clojure.java.io :as io]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.dialogs :as dialogs]
            [editor.outline-view :as outline-view])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc  GameObject$ComponentDesc GameObject$EmbeddedComponentDesc GameObject$PrototypeDesc$Builder]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.workspace BuildResource]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def game-object-icon "icons/brick.png")

(g/defnk produce-save-data [resource content]
  {:resource resource
   :content content})

(defn- build-game-project [])

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property content t/Str)

  (output outline t/Any :cached (g/fnk [node-id] {:node-id node-id :label "Game Project" :icon game-object-icon}))
  (output save-data t/Any :cached produce-save-data)
  (output build-targets t/Any :cached (g/fnk [node-id resource content] [{:node-id node-id
                                                                          :resource (BuildResource. resource nil)
                                                                          :build-fn (fn [self basis resource dep-resources user-data]
                                                                                      {:resource resource :content (:content user-data)})
                                                                          :user-data {:content content}}])))

(defn load-game-project [project self input]
  (let [content (slurp input)]
    (g/set-property self :content content)))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :build-ext "projectc"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-object-icon
                                    :view-types [:text]))

(ns editor.game-object
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [internal.render.pass :as pass])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]
           [java.io PushbackReader]))

(def game-object-icon "icons/brick.png")

(g/defnode ComponentNode
  (property id t/Str)
  (property embedded t/Bool (visible false))

  (input source t/Any)
  (input outline t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id})))
  (output render-setup t/Any (g/fnk [source]
                                    (when-let [resource-type (project/get-resource-type source)]
                                      {:self source :setup-rendering-fn (:setup-rendering-fn resource-type)}))))

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input aabbs [AABB])
  (input outline [t/Any])
  (input render-setups [t/Any])

  (output outline t/Any (g/fnk [self outline] {:self self :label "Game Object" :icon game-object-icon :children outline}))
  (output aabb AABB (g/fnk [aabbs] (reduce (fn [a b] (geom/aabb-union a b)) (geom/null-aabb) aabbs))))

(defn load-game-object [project self input]
  (let [project-graph (g/nref->gid (g/node-id self))
        prototype (protobuf/pb->map (protobuf/read-text GameObject$PrototypeDesc input))]
    (concat
      (for [component (:components prototype)]
        (g/make-nodes project-graph
                      [comp-node [ComponentNode :id (:id component)]]
                      (g/connect comp-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:component component))]
                        (concat
                          (g/connect comp-node :render-setup self :render-setups)
                          (g/connect source-node :self comp-node :source)
                          (if (:outline (g/outputs source-node))
                            (g/connect source-node :outline comp-node :outline)
                            []))
                        [])))
      (for [embedded (:embedded-components prototype)]
        (let [resource (project/make-embedded-resource project (:type embedded) (:data embedded))]
          (if-let [resource-type (and resource (workspace/resource-type resource))]
            (g/make-nodes project-graph
                          [comp-node [ComponentNode :id (:id embedded) :embedded true]
                           source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                          (g/connect source-node :self         comp-node :source)
                          (g/connect source-node :outline      comp-node :outline)
                          (g/connect comp-node   :outline      self      :outline)
                          (g/connect comp-node   :render-setup self      :render-setups))
            (g/make-nodes project-graph
                          [comp-node [ComponentNode :id (:id embedded) :embedded true]]
                          (g/connect comp-node   :outline      self      :outline))))))))

(defn setup-rendering [self view]
  (for [render-setup (g/node-value self :render-setups)
        :when (:setup-rendering-fn render-setup)
        :let [node (:self render-setup)
              render-fn (:setup-rendering-fn render-setup)]]
    (render-fn (g/refresh node) view)))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :setup-view-fn (fn [self view] (scene/setup-view view :grid true))
                                    :setup-rendering-fn setup-rendering
                                    :icon game-object-icon))

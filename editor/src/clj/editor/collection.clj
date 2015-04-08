(ns editor.collection
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
            [editor.game-object :as game-object]
            [internal.render.pass :as pass])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]
           [java.io PushbackReader]))

(def collection-icon "icons/bricks.png")

(g/defnode GameObjectInstanceNode
  (property id t/Str)
  (property path t/Str)
  (property embedded t/Bool (visible false))

  (input source t/Any)
  (input properties t/Any)
  (input outline t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id :icon game-object/game-object-icon})))
  (output render-setup t/Any (g/fnk [source]
                                    (let [resource-type (project/get-resource-type source)
                                          view-fns (:scene (:view-fns resource-type))]
                                      {:self source :setup-rendering-fn (:setup-rendering-fn view-fns)}))))

(g/defnode CollectionNode
  (inherits project/ResourceNode)

  (property name t/Str)

  (input aabbs [AABB])
  (input outline [t/Any])
  (input render-setups [t/Any])

  (output outline t/Any (g/fnk [self outline] {:self self :label "Collection" :icon collection-icon :children outline}))
  (output aabb AABB (g/fnk [aabbs] (reduce (fn [a b] (geom/aabb-union a b)) (geom/null-aabb) aabbs))))

(g/defnode CollectionInstanceNode
  (property id t/Str)
  (property path t/Str)
  
  (input source t/Any)
  (input aabbs [AABB])
  (input outline t/Any)
  
  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id :icon collection-icon})))
  (output render-setup t/Any (g/fnk [source]
                                    (let [resource-type (project/get-resource-type source)
                                          view-fns (:scene (:view-fns resource-type))]
                                      {:self source :setup-rendering-fn (:setup-rendering-fn view-fns)}))))

(defn load-collection [project self input]
  (let [collection (protobuf/pb->map (protobuf/read-text GameObject$CollectionDesc input))
        project-graph (g/nref->gid (g/node-id project))]
    (concat
      (for [game-object (:instances collection)]
        (g/make-nodes project-graph
                      [go-node [GameObjectInstanceNode :id (:id game-object) :path (:prototype game-object)]]
                      (g/connect go-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:prototype game-object))]
                        [(g/connect go-node :render-setup self :render-setups)
                         (g/connect source-node :self go-node :source)
                         (g/connect source-node :outline go-node :outline)]
                        [])))
      (for [embedded (:embedded-instances collection)]
        (let [resource (project/make-embedded-resource project "go" (:data embedded))]
          (if-let [resource-type (and resource (workspace/resource-type resource))]
            (g/make-nodes project-graph
                          [go-node [GameObjectInstanceNode :id (:id embedded) :embedded true]
                           source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                          (g/connect source-node :self         go-node :source)
                          (g/connect source-node :outline      go-node :outline)
                          (g/connect go-node   :outline      self      :outline)
                          (g/connect go-node   :render-setup self      :render-setups))
            (g/make-nodes project-graph
                          [go-node [GameObjectInstanceNode :id (:id embedded) :embedded true]]
                          (g/connect go-node :outline self :outline)))))
      (for [coll-instance (:collection-instances collection)]
        (g/make-nodes project-graph
                      [coll-node [CollectionInstanceNode :id (:id coll-instance) :path (:collection coll-instance)]]
                      (g/connect coll-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:collection coll-instance))]
                        [(g/connect coll-node :render-setup self :render-setups)
                         (g/connect source-node :self coll-node :source)
                         (g/connect source-node :outline coll-node :outline)]
                        []))))))

(defn setup-rendering [self view]
  (for [render-setup (g/node-value self :render-setups)
        :when (:setup-rendering-fn render-setup)
        :let [node (:self render-setup)
              render-fn (:setup-rendering-fn render-setup)]]
    (render-fn (g/refresh node) view)))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collection"
                                    :node-type CollectionNode
                                    :load-fn load-collection
                                    :icon collection-icon
                                    :view-types [:scene]
                                    :view-fns {:scene {:setup-view-fn (fn [self view] (scene/setup-view view :grid true))
                                                       :setup-rendering-fn setup-rendering}}))

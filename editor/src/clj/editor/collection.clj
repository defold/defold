(ns editor.collection
  (:require [clojure.pprint :refer [pprint]] 
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.game-object :as game-object]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(def collection-icon "icons/bricks.png")

(defn- gen-embed-ddf [id position rotation scale save-data]
  (-> (doto (GameObject$EmbeddedInstanceDesc/newBuilder)
        (.setId id)
        ; TODO children
        (.setData (:content save-data))
        (.setPosition (protobuf/vecmath->pb position))
        (.setRotation (protobuf/vecmath->pb rotation))
        ; TODO properties
        (.setScale scale))
    (.build)))

(defn- gen-ref-ddf [id position rotation scale save-data]
  (-> (doto (GameObject$InstanceDesc/newBuilder)
        (.setId id)
        ; TODO children
        (.setPrototype (workspace/proj-path (:resource save-data)))
        (.setPosition (protobuf/vecmath->pb position))
        (.setRotation (protobuf/vecmath->pb rotation))
        ; TODO properties
        (.setScale scale))
    (.build)))

(g/defnode GameObjectInstanceNode
  (property id t/Str)
  (property path t/Str)
  (property embedded t/Bool (visible false))
  (property position Point3d)
  (property rotation Quat4d)
  (property scale t/Num) ; TODO - non-uniform scale

  (input source t/Any)
  (input properties t/Any)
  (input outline t/Any)
  (input save-data t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id :icon game-object/game-object-icon})))
  (output render-setup t/Any (g/fnk [source]
                                    (let [resource-type (project/get-resource-type source)
                                          view-fns (:scene (:view-fns resource-type))]
                                      {:self source :setup-rendering-fn (:setup-rendering-fn view-fns)})))
  (output ddf-message t/Any :cached (g/fnk [id path embedded position rotation scale save-data]
                                           (if embedded (gen-embed-ddf id position rotation scale save-data) (gen-ref-ddf id position rotation scale save-data)))))

(g/defnk produce-save-data [resource name ref-inst-ddf embed-inst-ddf ref-coll-ddf]
  {:resource resource
   :content (protobuf/pb->str (.build (doto (GameObject$CollectionDesc/newBuilder)
                                        (.setName name)
                                        (.addAllInstances ref-inst-ddf)
                                        (.addAllEmbeddedInstances embed-inst-ddf)
                                        (.addAllCollectionInstances ref-coll-ddf))))})

(g/defnode CollectionNode
  (inherits project/ResourceNode)

  (property name t/Str)

  (input aabbs [AABB])
  (input outline [t/Any])
  (input render-setups [t/Any])
  (input ref-inst-ddf [t/Any])
  (input embed-inst-ddf [t/Any])
  (input ref-coll-ddf [t/Any])

  (output outline t/Any (g/fnk [self outline] {:self self :label "Collection" :icon collection-icon :children outline}))
  (output aabb AABB (g/fnk [aabbs] (reduce (fn [a b] (geom/aabb-union a b)) (geom/null-aabb) aabbs)))
  (output save-data t/Any :cached produce-save-data))

(g/defnode CollectionInstanceNode
  (property id t/Str)
  (property path t/Str)
  (property position Point3d)
  (property rotation Quat4d)
  (property scale t/Num) ; TODO - non-uniform scale

  (input source t/Any)
  (input aabbs [AABB])
  (input outline t/Any)
  (input save-data t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id :icon collection-icon})))
  (output render-setup t/Any (g/fnk [source]
                                    (let [resource-type (project/get-resource-type source)
                                          view-fns (:scene (:view-fns resource-type))]
                                      {:self source :setup-rendering-fn (:setup-rendering-fn view-fns)})))
  (output ddf-message t/Any :cached (g/fnk [id path position rotation scale]
                                           (.build (doto (GameObject$CollectionInstanceDesc/newBuilder)
                                                     (.setId id)
                                                     (.setCollection path)
                                                     (.setPosition (protobuf/vecmath->pb position))
                                                     (.setRotation (protobuf/vecmath->pb rotation))
                                                     (.setScale scale))))))

(defn load-collection [project self input]
  (let [collection (protobuf/pb->map (protobuf/read-text GameObject$CollectionDesc input))
        project-graph (g/node->graph-id project)]
    (concat
      (g/set-property self :name (:name collection))
      (for [game-object (:instances collection)]
        (g/make-nodes project-graph
                      [go-node [GameObjectInstanceNode :id (:id game-object) :path (:prototype game-object)
                                :position (:position game-object) :rotation (:rotation game-object) :scale (:scale game-object)]]
                      (g/connect go-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:prototype game-object))]
                        [(g/connect go-node :render-setup self :render-setups)
                         (g/connect go-node :ddf-message self :ref-inst-ddf)
                         (g/connect source-node :self go-node :source)
                         (g/connect source-node :outline go-node :outline)
                         (g/connect source-node :save-data go-node :save-data)]
                        [])))
      (for [embedded (:embedded-instances collection)]
        (let [resource (project/make-embedded-resource project "go" (:data embedded))]
          (if-let [resource-type (and resource (workspace/resource-type resource))]
            (g/make-nodes project-graph
                          [go-node [GameObjectInstanceNode :id (:id embedded) :embedded true
                                    :position (:position embedded) :rotation (:rotation embedded) :scale (:scale embedded)]
                           source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                          (g/connect source-node :self       go-node :source)
                          (g/connect source-node :outline    go-node :outline)
                          (g/connect source-node :save-data  go-node :save-data)
                          (g/connect go-node   :outline      self    :outline)
                          (g/connect go-node   :ddf-message  self    :embed-inst-ddf)
                          (g/connect go-node   :render-setup self    :render-setups))
            (g/make-nodes project-graph
                          [go-node [GameObjectInstanceNode :id (:id embedded) :embedded true]]
                          (g/connect go-node :outline self :outline)))))
      (for [coll-instance (:collection-instances collection)]
        (g/make-nodes project-graph
                      [coll-node [CollectionInstanceNode :id (:id coll-instance) :path (:collection coll-instance)
                                  :position (:position coll-instance) :rotation (:rotation coll-instance) :scale (:scale coll-instance)]]
                      (g/connect coll-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:collection coll-instance))]
                        [(g/connect coll-node   :render-setup self :render-setups)
                         (g/connect coll-node   :ddf-message  self :ref-coll-ddf)
                         (g/connect source-node :self         coll-node :source)
                         (g/connect source-node :outline      coll-node :outline)
                         (g/connect source-node :save-data    coll-node :save-data)]
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

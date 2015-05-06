(ns editor.game-object
  (:require [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc GameObject$ComponentDesc GameObject$EmbeddedComponentDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def game-object-icon "icons/brick.png")

(defn- gen-ref-ddf [id position rotation save-data]
  (.build (doto (GameObject$ComponentDesc/newBuilder)
            (.setId id)
            (.setPosition (protobuf/vecmath->pb (Point3d. position)))
            (.setRotation (protobuf/vecmath->pb rotation))
            (.setComponent (workspace/proj-path (:resource save-data))))))

(defn- gen-embed-ddf [id position rotation save-data]
  (.build (doto (GameObject$EmbeddedComponentDesc/newBuilder)
            (.setId id)
            (.setType (:ext (workspace/resource-type (:resource save-data))))
            (.setPosition (protobuf/vecmath->pb (Point3d. position)))
            (.setRotation (protobuf/vecmath->pb rotation))
            (.setData (:content save-data)))))

(g/defnode ComponentNode
  (inherits scene/SceneNode)

  (property id t/Str)
  (property embedded t/Bool (visible false))

  (input source t/Any)
  (input outline t/Any)
  (input save-data t/Any)
  (input scene t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id})))
  (output ddf-message t/Any :cached (g/fnk [id embedded position rotation save-data] (if embedded
                                                                                       (gen-embed-ddf id position rotation save-data)
                                                                                       (gen-ref-ddf id position rotation save-data))))
  (output scene t/Any :cached (g/fnk [self transform scene]
                                     (assoc scene
                                            :id (g/node-id self)
                                            :transform transform
                                            :aabb (geom/aabb-transform (:aabb scene) transform)))))

(g/defnk produce-save-data [resource ref-ddf embed-ddf]
  {:resource resource
   :content (protobuf/pb->str
              (let [builder (GameObject$PrototypeDesc/newBuilder)]
                (doseq [ddf ref-ddf]
                  (.addComponents builder ddf))
                (doseq [ddf embed-ddf]
                  (.addEmbeddedComponents builder ddf))
                (.build builder)))})

(g/defnk produce-scene [self child-scenes]
  {:id (g/node-id self)
   :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))
   :children child-scenes})

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input outline t/Any :array)
  (input ref-ddf t/Any :array)
  (input embed-ddf t/Any :array)
  (input child-scenes t/Any :array)

  (output outline t/Any (g/fnk [self outline] {:self self :label "Game Object" :icon game-object-icon :children outline}))
  (output save-data t/Any :cached produce-save-data)
  (output scene t/Any :cached produce-scene))

(defn- connect-if-output [out-node out-label in-node in-label]
  (if ((g/outputs out-node) out-label)
    (g/connect out-node out-label in-node in-label)
    []))

(defn load-game-object [project self input]
  (let [project-graph (g/node->graph-id self)
        prototype (protobuf/pb->map (protobuf/read-text GameObject$PrototypeDesc input))]
    (concat
      (for [component (:components prototype)]
        (g/make-nodes project-graph
                     [comp-node [ComponentNode :id (:id component) :position (t/Point3d->Vec3 (:position component)) :rotation (math/quat->euler (:rotation component))]]
                     (g/connect comp-node :outline self :outline)
                     (if-let [source-node (project/resolve-resource-node self (:component component))]
                       (concat
                         (g/connect comp-node :ddf-message self :ref-ddf)
                         (g/connect comp-node :scene self :child-scenes)
                         (g/connect source-node :self comp-node :source)
                         (connect-if-output source-node :outline comp-node :outline)
                         (connect-if-output source-node :save-data comp-node :save-data)
                         (connect-if-output source-node :scene comp-node :scene))
                       [])))
      (for [embedded (:embedded-components prototype)]
        (let [resource (project/make-embedded-resource project (:type embedded) (:data embedded))]
          (if-let [resource-type (and resource (workspace/resource-type resource))]
            (g/make-nodes project-graph
                         [comp-node [ComponentNode :id (:id embedded) :embedded true :position (t/Point3d->Vec3 (:position embedded)) :rotation (math/quat->euler (:rotation embedded))]
                          source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                         (g/connect source-node :self         comp-node :source)
                         (g/connect source-node :outline      comp-node :outline)
                         (g/connect source-node :save-data comp-node :save-data)
                         (g/connect source-node :scene comp-node :scene)
                         (g/connect comp-node   :outline      self      :outline)
                         (g/connect comp-node   :ddf-message  self      :embed-ddf)
                         (g/connect comp-node   :scene  self      :child-scenes))
            (g/make-nodes project-graph
                          [comp-node [ComponentNode :id (:id embedded) :embedded true]]
                          (g/connect comp-node   :outline      self      :outline))))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :icon game-object-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))

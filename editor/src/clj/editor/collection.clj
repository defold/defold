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
            [editor.scene-tools :as scene-tools]
            [editor.workspace :as workspace]
            [editor.math :as math])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]))

(def collection-icon "icons/bricks.png")

(defn- gen-embed-ddf [id child-ids ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
  (let [^DdfMath$Point3 protobuf-position (protobuf/vecmath->pb (Point3d. position))
        ^DdfMath$Quat protobuf-rotation (protobuf/vecmath->pb rotation)]
(-> (doto (GameObject$EmbeddedInstanceDesc/newBuilder)
         (.setId id)
         (.addAllChildren child-ids)
         (.setData (:content save-data))
         (.setPosition protobuf-position)
         (.setRotation protobuf-rotation)
                                        ; TODO properties
                                        ; TODO - fix non-uniform hax
         (.setScale (.x scale)))
       (.build))))

(defn- gen-ref-ddf [id child-ids ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
  (let [^DdfMath$Point3 protobuf-position (protobuf/vecmath->pb (Point3d. position))
        ^DdfMath$Quat protobuf-rotation (protobuf/vecmath->pb rotation)]
    (-> (doto (GameObject$InstanceDesc/newBuilder)
         (.setId id)
         (.addAllChildren child-ids)
         (.setPrototype (workspace/proj-path (:resource save-data)))
         (.setPosition protobuf-position)
         (.setRotation protobuf-rotation)
                                        ; TODO properties
                                        ; TODO - fix non-uniform hax
         (.setScale (.x scale)))
       (.build))))

(defn- assoc-deep [scene keyword new-value]
  (let [new-scene (assoc scene keyword new-value)]
    (if (:children scene)
      (assoc new-scene :children (map #(assoc-deep % keyword new-value) (:children scene)))
      new-scene)))

(g/defnk produce-transform [^Vector3d position ^Quat4d rotation ^Vector3d scale]
  (let [transform (Matrix4d. rotation position 1.0)
        s [(.x scale) (.y scale) (.z scale)]
        col (Vector4d.)]
    (doseq [^Integer i (range 3)
            :let [s (nth s i)]]
      (.getColumn transform i col)
      (.scale col s)
      (.setColumn transform i col))
    transform))

(g/defnode ScalableSceneNode
  (inherits scene/SceneNode)

  (property scale t/Vec3 (default [1 1 1]))

  (output scale Vector3d :cached (g/fnk [scale] (Vector3d. (double-array scale))))
  (output transform Matrix4d :cached produce-transform)

  scene-tools/Scalable
  (scene-tools/scale [self delta] (let [s (Vector3d. (double-array (:scale self)))
                                        ^Vector3d d delta]
                                    (.setX s (* (.x s) (.x d)))
                                    (.setY s (* (.y s) (.y d)))
                                    (.setZ s (* (.z s) (.z d)))
                                    (g/set-property self :scale [(.x s) (.y s) (.z s)]))))

(g/defnode GameObjectInstanceNode
  (inherits ScalableSceneNode)

  (property id t/Str)
  (property path (t/maybe t/Str))
  (property embedded (t/maybe t/Bool) (visible false))

  (input source t/Any)
  (input properties t/Any)
  (input outline t/Any)
  (input child-outlines t/Any :array)
  (input save-data t/Any)
  (input scene t/Any)
  (input child-scenes t/Any :array)
  (input child-ids t/Str :array)

  (output outline t/Any (g/fnk [self id outline child-outlines]
                               (merge-with concat
                                           (merge outline {:self self :label id :icon game-object/game-object-icon})
                                           {:children child-outlines})))
  (output ddf-message t/Any :cached (g/fnk [id child-ids path embedded ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
                                           (if embedded
                                             (gen-embed-ddf id child-ids position rotation scale save-data)
                                             (gen-ref-ddf id child-ids position rotation scale save-data))))
  (output scene t/Any :cached (g/fnk [self transform scene child-scenes]
                                     (let [aabb (geom/aabb-transform (geom/aabb-incorporate (:aabb scene) 0 0 0) transform)
                                           aabb (reduce #(geom/aabb-union %1 (geom/aabb-transform (:aabb %2) transform)) aabb child-scenes)]
                                       (merge-with concat
                                                   (assoc (assoc-deep scene :id (g/node-id self))
                                                          :transform transform
                                                          :aabb aabb)
                                                   {:children child-scenes})))))

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

  (input child-outlines t/Any :array)
  (input ref-inst-ddf t/Any :array)
  (input embed-inst-ddf t/Any :array)
  (input ref-coll-ddf t/Any :array)
  (input child-scenes t/Any :array)

  (output outline t/Any (g/fnk [self child-outlines] {:self self :label "Collection" :icon collection-icon :children child-outlines}))
  (output save-data t/Any :cached produce-save-data)
  (output scene t/Any :cached (g/fnk [self child-scenes]
                                     {:id (g/node-id self)
                                      :children child-scenes
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))})))

(g/defnode CollectionInstanceNode
  (inherits ScalableSceneNode)

  (property id t/Str)
  (property path t/Str)

  (input source t/Any)
  (input outline t/Any)
  (input save-data t/Any)
  (input scene t/Any)

  (output outline t/Any (g/fnk [self id outline] (merge outline {:self self :label id :icon collection-icon})))
  (output ddf-message t/Any :cached (g/fnk [id path ^Vector3d position ^Quat4d rotation ^Vector3d scale]
                                           (let [^DdfMath$Point3 protobuf-position (protobuf/vecmath->pb (Point3d. position))
                                                 ^DdfMath$Quat protobuf-rotation (protobuf/vecmath->pb rotation)]
                                            (.build (doto (GameObject$CollectionInstanceDesc/newBuilder)
                                                      (.setId id)
                                                      (.setCollection path)
                                                      (.setPosition protobuf-position)
                                                      (.setRotation protobuf-rotation)
                                                      ; TODO - fix non-uniform hax
                                                      (.setScale (.x scale)))))))
  (output scene t/Any :cached (g/fnk [self transform scene]
                                     (assoc scene
                                           :id (g/node-id self)
                                           :transform transform
                                           :aabb (geom/aabb-transform (:aabb scene) transform)))))

(defn load-collection [project self input]
  (let [collection (protobuf/pb->map (protobuf/read-text GameObject$CollectionDesc input))
        project-graph (g/node->graph-id project)]
    (concat
      (g/set-property self :name (:name collection))
      (let [tx-go-creation (flatten
                             (concat
                              (for [game-object (:instances collection)
                                 :let [; TODO - fix non-uniform hax
                                       scale (:scale game-object)]]
                                (g/make-nodes project-graph
                                              [go-node [GameObjectInstanceNode :id (:id game-object) :path (:prototype game-object)
                                                        :position (t/Point3d->Vec3 (:position game-object)) :rotation (math/quat->euler (:rotation game-object)) :scale [scale scale scale]]]
                                              (if-let [source-node (project/resolve-resource-node self (:prototype game-object))]
                                                [(g/connect go-node :ddf-message self :ref-inst-ddf)
                                                 (g/connect source-node :self go-node :source)
                                                 (g/connect source-node :outline go-node :outline)
                                                 (g/connect source-node :save-data go-node :save-data)
                                                 (g/connect source-node :scene go-node :scene)]
                                                [])))
                              (for [embedded (:embedded-instances collection)
                                    :let [; TODO - fix non-uniform hax
                                          scale (:scale embedded)]]
                                (let [resource (project/make-embedded-resource project "go" (:data embedded))]
                                  (if-let [resource-type (and resource (workspace/resource-type resource))]
                                    (g/make-nodes project-graph
                                                  [go-node [GameObjectInstanceNode :id (:id embedded) :embedded true
                                                            :position (t/Point3d->Vec3 (:position embedded)) :rotation (math/quat->euler (:rotation embedded)) :scale [scale scale scale]]
                                                   source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                                                  (g/connect source-node :self       go-node :source)
                                                  (g/connect source-node :outline    go-node :outline)
                                                  (g/connect source-node :save-data  go-node :save-data)
                                                  (g/connect source-node :scene  go-node :scene)
                                                  (g/connect go-node   :ddf-message  self    :embed-inst-ddf)
                                    (g/make-node project-graph GameObjectInstanceNode :id (:id embedded) :embedded true)))))))
            id->nid (into {} (map #(do [(get-in % [:node :id]) (g/node-id (:node %))]) (filter #(= :create-node (:type %)) tx-go-creation)))
            child->parent (into {} (map #(do [% nil]) (keys id->nid)))
            rev-child-parent-fn (fn [instances] (into {} (mapcat (fn [inst] (map #(do [% (:id inst)]) (:children inst))) instances)))
            child->parent (merge child->parent (rev-child-parent-fn (concat (:instances collection) (:embedded-instances collection))))]
        (concat
          tx-go-creation
          (for [[child parent] child->parent
                :let [child-id (id->nid child)
                      parent-id (if parent (id->nid parent) self)]]
            (concat
              (g/connect child-id :outline parent-id :child-outlines)
              (g/connect child-id :scene parent-id :child-scenes)
              (if parent
                (g/connect child-id :id parent-id :child-ids)
                [])))))
      (for [coll-instance (:collection-instances collection)
            :let [; TODO - fix non-uniform hax
                  scale (:scale coll-instance)]]
        (g/make-nodes project-graph
                      [coll-node [CollectionInstanceNode :id (:id coll-instance) :path (:collection coll-instance)
                                  :position (t/Point3d->Vec3 (:position coll-instance)) :rotation (math/quat->euler (:rotation coll-instance)) :scale [scale scale scale]]]
                      (g/connect coll-node :outline self :child-outlines)
                      (if-let [source-node (project/resolve-resource-node self (:collection coll-instance))]
                        [(g/connect coll-node   :ddf-message  self :ref-coll-ddf)
                         (g/connect coll-node   :scene  self :child-scenes)
                         (g/connect source-node :self         coll-node :source)
                         (g/connect source-node :outline      coll-node :outline)
                         (g/connect source-node :save-data    coll-node :save-data)
                         (g/connect source-node :scene    coll-node :scene)]
                        []))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collection"
                                    :node-type CollectionNode
                                    :load-fn load-collection
                                    :icon collection-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))

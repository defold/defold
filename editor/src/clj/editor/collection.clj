(ns editor.collection
  (:require [clojure.pprint :refer [pprint]]
            [clojure.java.io :as io]
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
            [editor.math :as math]
            [editor.handler :as handler]
            [editor.dialogs :as dialogs]
            [internal.render.pass :as pass])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc GameObject$InstanceDesc$Builder
            GameObject$EmbeddedInstanceDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [org.apache.commons.io FilenameUtils]))

(def collection-icon "icons/bricks.png")
(def path-sep "/")

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

  (output scale Vector3d :cached (g/fnk [^t/Vec3 scale] (Vector3d. (double-array scale))))
  (output transform Matrix4d :cached produce-transform)

  scene-tools/Scalable
  (scene-tools/scale [self delta] (let [s (Vector3d. (double-array (:scale self)))
                                        ^Vector3d d delta]
                                    (.setX s (* (.x s) (.x d)))
                                    (.setY s (* (.y s) (.y d)))
                                    (.setZ s (* (.z s) (.z d)))
                                    (g/set-property self :scale [(.x s) (.y s) (.z s)]))))

(defn- outline-sort-by-fn [v]
  [(:name (g/node-type (g/node-by-id (:node-id v)))) (:label v)])

(g/defnode GameObjectInstanceNode
  (inherits ScalableSceneNode)

  (property id t/Str)
  (property path (t/maybe t/Str))
  (property embedded (t/maybe t/Bool) (visible (g/fnk [] false)))

  (input source t/Any)
  (input properties t/Any)
  (input outline t/Any)
  (input child-outlines t/Any :array)
  (input save-data t/Any)
  (input build-targets t/Any)
  (input scene t/Any)
  (input child-scenes t/Any :array)
  (input child-ids t/Str :array)

  (output outline t/Any :cached (g/fnk [node-id id path embedded outline child-outlines]
                                       (let [suffix (if embedded "" (format " (%s)" path))]
                                         (merge-with concat
                                                     (merge outline {:node-id node-id :label (str id suffix) :icon game-object/game-object-icon :sort-by-fn outline-sort-by-fn})
                                                    {:children child-outlines}))))
  (output ddf-message t/Any :cached (g/fnk [id child-ids path embedded ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
                                           (if embedded
                                             (gen-embed-ddf id child-ids position rotation scale save-data)
                                             (gen-ref-ddf id child-ids position rotation scale save-data))))
  (output build-targets t/Any (g/fnk [build-targets ddf-message transform] [(assoc (first build-targets)
                                                                                   :user-data (assoc (get (first build-targets) :user-data)
                                                                                                     :instance-msg ddf-message
                                                                                                     :transform transform))]))
  (output scene t/Any :cached (g/fnk [node-id transform scene child-scenes embedded]
                                     (let [aabb (reduce #(geom/aabb-union %1 (:aabb %2)) (:aabb scene) child-scenes)
                                           aabb (geom/aabb-transform (geom/aabb-incorporate aabb 0 0 0) transform)]
                                       (merge-with concat
                                                   (assoc (assoc-deep scene :node-id node-id)
                                                          :transform transform
                                                          :aabb aabb
                                                          :renderable {:passes [pass/selection]})
                                                   {:children child-scenes})))))

(g/defnk produce-proto-msg [name ref-inst-ddf embed-inst-ddf ref-coll-ddf]
  (.build (doto (GameObject$CollectionDesc/newBuilder)
            (.setName name)
            (.addAllInstances ref-inst-ddf)
            (.addAllEmbeddedInstances embed-inst-ddf)
            (.addAllCollectionInstances ref-coll-ddf))))

(g/defnk produce-save-data [resource proto-msg]
  {:resource resource
   :content (protobuf/pb->str proto-msg)})

(defn- ->instance-builder [msg resource] ^GameObject$InstanceDesc$Builder
  (let [builder (-> (if (instance? GameObject$EmbeddedInstanceDesc msg)
                      (-> (GameObject$InstanceDesc/newBuilder)
                        (.setId (.getId msg))
                        (.addAllChildren (.getChildrenList msg))
                        (.setPosition (.getPosition msg))
                        (.setRotation (.getRotation msg))
                        (.addAllComponentProperties (.getComponentPropertiesList msg)))
                      (GameObject$InstanceDesc/newBuilder msg))
                  (.setPrototype (workspace/proj-path resource)))]
    (if (.hasScale3 msg)
      (.setScale3 builder (.getScale3 msg))
      (let [s (.getScale msg)]
        (.setScale3 builder (protobuf/vecmath->pb (Vector3d. s s s)))))))

(defn- externalize [inst-data resources]
  (map (fn [data]
         (let [[resource msg transform] data
               resource (get resources resource)
               builder (->instance-builder msg resource)
               pos (Point3d.)
               rot (Quat4d.)
               scale (Vector3d.)
               _ (math/split-mat4 transform pos rot scale)
               child-ids (.getChildrenList builder)]
           (.build (-> builder
                     (.setId (str path-sep (.getId builder)))
                     (.clearChildren)
                     (.addAllChildren (map #(str path-sep %) child-ids))
                     (.setPosition (protobuf/vecmath->pb pos))
                     (.setRotation (protobuf/vecmath->pb rot))
                     (.setScale3 (protobuf/vecmath->pb scale)))))) inst-data))

(defn build-collection [self basis resource dep-resources user-data]
  (let [{:keys [name instance-data]} user-data
        instance-msgs (externalize instance-data dep-resources)
        msg (.build
              (doto (GameObject$CollectionDesc/newBuilder)
                (.setName name)
                (.addAllInstances instance-msgs)))]
    {:resource resource :content (protobuf/pb->bytes msg)}))

(g/defnk produce-build-targets [node-id name resource proto-msg sub-build-targets dep-build-targets]
  (let [sub-build-targets (flatten sub-build-targets)
        dep-build-targets (flatten dep-build-targets)
        instance-data (map #(let [user-data (get % :user-data)]
                              [(:resource %) (get user-data :instance-msg) (get user-data :transform)]) dep-build-targets)
        instance-data (reduce concat instance-data (map #(get-in % [:user-data :instance-data]) sub-build-targets))]
    [{:node-id node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-collection
      :user-data {:name name :instance-data instance-data}
      :deps (vec (reduce into dep-build-targets (map :deps sub-build-targets)))}]))

(g/defnode CollectionNode
  (inherits project/ResourceNode)

  (property name t/Str)

  (input child-outlines t/Any :array)
  (input ref-inst-ddf t/Any :array)
  (input embed-inst-ddf t/Any :array)
  (input ref-coll-ddf t/Any :array)
  (input child-scenes t/Any :array)
  (input ids t/Str :array)
  (input sub-build-targets t/Any :array)
  (input dep-build-targets t/Any :array)

  (output proto-msg t/Any :cached produce-proto-msg)
  (output save-data t/Any :cached produce-save-data)
  (output build-targets t/Any :cached produce-build-targets)
  (output outline t/Any :cached (g/fnk [node-id child-outlines] {:node-id node-id :label "Collection" :icon collection-icon :children child-outlines :sort-by-fn outline-sort-by-fn}))
  (output scene t/Any :cached (g/fnk [node-id child-scenes]
                                     {:node-id node-id
                                      :children child-scenes
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))})))

(defn- flatten-instance-data [data base-id base-transform all-child-ids]
  (let [[resource msg ^Matrix4d transform] data
        builder (->instance-builder msg resource)
        is-child? (contains? all-child-ids (.getId builder))
        child-ids (.getChildrenList builder)
        builder (doto builder
                  (.setId (str base-id (.getId builder)))
                  (.clearChildren)
                  (.addAllChildren (map #(str base-id %) child-ids)))
        msg (.build builder)
        transform (if is-child?
                    transform
                    (doto (Matrix4d. transform) (.mul base-transform transform)))]
    [resource msg transform]))

(g/defnk produce-coll-inst-build-targets [id transform build-targets]
  (let [base-id (str id path-sep)
        instance-data (get-in build-targets [0 :user-data :instance-data])
        child-ids (reduce (fn [child-ids [_ msg _]] (into child-ids (.getChildrenList msg))) #{} instance-data)]
    (assoc-in build-targets [0 :user-data :instance-data] (map #(flatten-instance-data % base-id transform child-ids) instance-data))))

(g/defnode CollectionInstanceNode
  (inherits ScalableSceneNode)

  (property id t/Str)
  (property path t/Str)

  (input source t/Any)
  (input outline t/Any)
  (input save-data t/Any)
  (input scene t/Any)
  (input build-targets t/Any)

  (output outline t/Any :cached (g/fnk [node-id id path outline] (let [suffix (format " (%s)" path)]
                                                                   (merge outline {:node-id node-id :label (str id suffix) :icon collection-icon}))))
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
  (output scene t/Any :cached (g/fnk [node-id transform scene]
                                     (assoc scene
                                           :node-id node-id
                                           :transform transform
                                           :aabb (geom/aabb-transform (:aabb scene) transform)
                                           :renderable {:passes [pass/selection]})))
  (output build-targets t/Any produce-coll-inst-build-targets))

(defn- gen-instance-id [coll-node base]
  (let [ids (g/node-value coll-node :ids)]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- make-go [self source-node id position rotation scale]
  (let [path (if source-node (workspace/proj-path (:resource source-node)) "")]
    (g/make-nodes (g/node->graph-id self)
                  [go-node [GameObjectInstanceNode :id id :path path
                            :position position :rotation rotation :scale scale]]
                  (concat
                    (g/connect go-node :self self :nodes)
                    (if source-node
                      (concat
                        (g/connect go-node     :ddf-message   self    :ref-inst-ddf)
                        (g/connect go-node     :build-targets self    :dep-build-targets)
                        (g/connect go-node     :id            self    :ids)
                        (g/connect source-node :self          go-node :source)
                        (g/connect source-node :outline       go-node :outline)
                        (g/connect source-node :save-data     go-node :save-data)
                        (g/connect source-node :build-targets go-node :build-targets)
                        (g/connect source-node :scene         go-node :scene))
                      [])))))

(defn- single-selection? [selection]
  (= 1 (count selection)))

(defn- selected-collection? [selection]
  (= CollectionNode (g/node-type (g/node-by-id (first selection)))))

(defn- selected-embedded-instance? [selection]
  (let [node (g/node-by-id (first selection))]
    (and (= GameObjectInstanceNode (g/node-type node))
         (:embedded node))))

(handler/defhandler :add-from-file :global
  (enabled? [selection] (and (single-selection? selection)
                             (or (selected-collection? selection)
                                 (selected-embedded-instance? selection))))
  (run [selection] (if (selected-embedded-instance? selection)
                     (game-object/add-component-handler (g/node-value (first selection) :source))
                     (let [coll-node (g/node-by-id (first selection))
                           project (project/get-project coll-node)
                           workspace (:workspace (:resource coll-node))
                           ext "go"]
                       (when-let [; TODO - filter game object files
                                  resource (first (dialogs/make-resource-dialog workspace {}))]
                         (let [base (FilenameUtils/getBaseName (workspace/resource-name resource))
                               id (gen-instance-id coll-node base)
                               op-seq (gensym)
                               [go-node] (g/tx-nodes-added
                                           (g/transact
                                             (concat
                                               (g/operation-label "Add Game Object")
                                               (g/operation-sequence op-seq)
                                               (make-go coll-node (project/get-resource-node project resource) id [0 0 0] [0 0 0] [1 1 1]))))]
                           ; Selection
                           (g/transact
                             (concat
                               (g/operation-sequence op-seq)
                               (g/operation-label "Add Game Object")
                               (g/connect go-node :outline coll-node :child-outlines)
                               (g/connect go-node :scene coll-node :child-scenes)
                               (project/select project [go-node])))))))))

(defn- make-embedded-go [self project type data id position rotation scale]
  (let [resource (project/make-embedded-resource project type data)]
    (if-let [resource-type (and resource (workspace/resource-type resource))]
      (g/make-nodes (g/node->graph-id self)
                    [go-node [GameObjectInstanceNode :id id :embedded true
                              :position position :rotation rotation :scale scale]
                     source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                    (g/connect source-node :self          go-node :source)
                    (g/connect source-node :outline       go-node :outline)
                    (g/connect source-node :save-data     go-node :save-data)
                    (g/connect source-node :build-targets go-node :build-targets)
                    (g/connect source-node :scene         go-node :scene)
                    (g/connect go-node     :ddf-message   self    :embed-inst-ddf)
                    (g/connect go-node     :build-targets self    :dep-build-targets)
                    (g/connect go-node     :id            self    :ids)
                    (g/connect go-node     :self          self    :nodes))
      (g/make-nodes (g/node->graph-id self)
                    [go-node [GameObjectInstanceNode :id id :embedded true]]
                    (g/connect go-node     :self          self    :nodes)))))

(handler/defhandler :add :global
  (enabled? [selection] (and (single-selection? selection)
                             (or (selected-collection? selection)
                                 (selected-embedded-instance? selection))))
    (run [selection] (if (selected-embedded-instance? selection)
                       (game-object/add-embedded-component-handler (g/node-value (first selection) :source))
                       (let [coll-node (g/node-by-id (first selection))
                            project (project/get-project coll-node)
                            workspace (:workspace (:resource coll-node))
                            ext "go"
                            resource-type (workspace/get-resource-type workspace ext)
                            template (workspace/template resource-type)]
                        (let [id (gen-instance-id coll-node ext)
                              op-seq (gensym)
                              [go-node source-node] (g/tx-nodes-added
                                                      (g/transact
                                                        (concat
                                                          (g/operation-sequence op-seq)
                                                          (g/operation-label "Add Game Object")
                                                          (make-embedded-go coll-node project ext template id [0 0 0] [0 0 0] [1 1 1]))))]
                          (g/transact
                            (concat
                              (g/operation-sequence op-seq)
                              (g/operation-label "Add Game Object")
                              (g/connect go-node :outline coll-node :child-outlines)
                              (g/connect go-node :scene coll-node :child-scenes)
                              ((:load-fn resource-type) project source-node (io/reader (:resource source-node)))
                              (project/select project [go-node]))))))))

(defn- add-collection-instance [self source-node id position rotation scale]
  (let [path (if source-node (workspace/proj-path (:resource source-node)) "")]
    (g/make-nodes (g/node->graph-id self)
                  [coll-node [CollectionInstanceNode :id id :path path
                              :position position :rotation rotation :scale scale]]
                  (g/connect coll-node :outline self :child-outlines)
                  (g/connect coll-node :self    self :nodes)
                  (if source-node
                    [(g/connect coll-node   :ddf-message   self :ref-coll-ddf)
                     (g/connect coll-node   :id            self :ids)
                     (g/connect coll-node   :scene         self :child-scenes)
                     (g/connect coll-node   :build-targets self :sub-build-targets)
                     (g/connect source-node :self          coll-node :source)
                     (g/connect source-node :outline       coll-node :outline)
                     (g/connect source-node :save-data     coll-node :save-data)
                     (g/connect source-node :scene         coll-node :scene)
                     (g/connect source-node :build-targets coll-node :build-targets)]
                    []))))

(handler/defhandler :add-secondary-from-file :global
  (enabled? [selection] (and (single-selection? selection)
                             (selected-collection? selection)))
  (run [selection] (let [coll-node (g/node-by-id (first selection))
                         project (project/get-project coll-node)
                         workspace (:workspace (:resource coll-node))
                         ext "collection"
                         resource-type (workspace/get-resource-type workspace ext)]
                     (when-let [; TODO - filter collection files
                                resource (first (dialogs/make-resource-dialog workspace {}))]
                       (let [base (FilenameUtils/getBaseName (workspace/resource-name resource))
                             id (gen-instance-id coll-node base)
                             op-seq (gensym)
                             [coll-inst-node] (g/tx-nodes-added
                                                (g/transact
                                                  (concat
                                                    (g/operation-label "Add Collection")
                                                    (g/operation-sequence op-seq)
                                                    (add-collection-instance coll-node (project/get-resource-node project resource) id [0 0 0] [0 0 0] [1 1 1]))))]
                         ; Selection
                         (g/transact
                           (concat
                             (g/operation-sequence op-seq)
                             (g/operation-label "Add Collection")
                             (project/select project [coll-inst-node]))))))))

(defn load-collection [project self input]
  (let [collection (protobuf/pb->map (protobuf/read-text GameObject$CollectionDesc input))
        project-graph (g/node->graph-id project)]
    (concat
      (g/set-property self :name (:name collection))
      (let [tx-go-creation (flatten
                             (concat
                               (for [game-object (:instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale game-object)
                                           source-node (project/resolve-resource-node self (:prototype game-object))]]
                                 (make-go self source-node (:id game-object) (t/Point3d->Vec3 (:position game-object))
                                          (math/quat->euler (:rotation game-object)) [scale scale scale]))
                               (for [embedded (:embedded-instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale embedded)]]
                                 (make-embedded-go self project "go" (:data embedded) (:id embedded)
                                                   (t/Point3d->Vec3 (:position embedded))
                                                   (math/quat->euler (:rotation embedded))
                                                   [scale scale scale]))))
            new-instance-data (filter #(and (= :create-node (:type %)) (= GameObjectInstanceNode (g/node-type (:node %)))) tx-go-creation)
            id->nid (into {} (map #(do [(get-in % [:node :id]) (g/node-id (:node %))]) new-instance-data))
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
                  scale (:scale coll-instance)
                  source-node (project/resolve-resource-node self (:collection coll-instance))]]
        (add-collection-instance self source-node (:id coll-instance) (t/Point3d->Vec3 (:position coll-instance))
                                 (math/quat->euler (:rotation coll-instance)) [scale scale scale])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collection"
                                    :node-type CollectionNode
                                    :load-fn load-collection
                                    :icon collection-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))

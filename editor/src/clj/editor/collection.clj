(ns editor.collection
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [org.apache.commons.io FilenameUtils]))

(def collection-icon "icons/16/Icons_09-Collection.png")
(def path-sep "/")

(defn- gen-embed-ddf [id child-ids ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
  {:id id
   :children child-ids
   ; TODO properties
   :data (:content save-data)
   :position (math/vecmath->clj position)
   :rotation (math/vecmath->clj rotation)
   :scale3 (math/vecmath->clj scale)})

(defn- gen-ref-ddf [id child-ids ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
  {:id id
   :children child-ids
   ; TODO properties
   :prototype (workspace/proj-path (:resource save-data))
   :position (math/vecmath->clj position)
   :rotation (math/vecmath->clj rotation)
   :scale3 (math/vecmath->clj scale)})

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

  (property scale types/Vec3 (default [1 1 1]))

  (output scale Vector3d :cached (g/fnk [^types/Vec3 scale] (Vector3d. (double-array scale))))
  (output transform Matrix4d :cached produce-transform)

  scene-tools/Scalable
  (scene-tools/scale [self delta] (let [s (Vector3d. (double-array (:scale self)))
                                        ^Vector3d d delta]
                                    (.setX s (* (.x s) (.x d)))
                                    (.setY s (* (.y s) (.y d)))
                                    (.setZ s (* (.z s) (.z d)))
                                    (g/set-property (g/node-id self) :scale [(.x s) (.y s) (.z s)]))))

(defn- outline-sort-by-fn [v]
  [(:name (g/node-type (g/node-by-id (:node-id v)))) (:label v)])

(g/defnode GameObjectInstanceNode
  (inherits ScalableSceneNode)

  (property id g/Str)
  (property path g/Str (dynamic visible (g/fnk [embedded] (not embedded))))
  (property embedded g/Bool (dynamic visible (g/always false)))

  (input source g/Any)
  (input properties g/Any)
  (input outline g/Any)
  (input child-outlines g/Any :array)
  (input save-data g/Any)
  (input build-targets g/Any)
  (input scene g/Any)
  (input child-scenes g/Any :array)
  (input child-ids g/Str :array)

  (output outline g/Any :cached (g/fnk [_node-id id path embedded outline child-outlines]
                                       (let [suffix (if embedded "" (format " (%s)" path))]
                                         (merge-with concat
                                                     (merge outline {:node-id _node-id :label (str id suffix) :icon game-object/game-object-icon :sort-by-fn outline-sort-by-fn})
                                                    {:children child-outlines}))))
  (output ddf-message g/Any :cached (g/fnk [id child-ids path embedded ^Vector3d position ^Quat4d rotation ^Vector3d scale save-data]
                                           (if embedded
                                             (gen-embed-ddf id child-ids position rotation scale save-data)
                                             (gen-ref-ddf id child-ids position rotation scale save-data))))
  (output build-targets g/Any (g/fnk [build-targets ddf-message transform] (let [target (first build-targets)]
                                                                             [(assoc target :instance-data {:resource (:resource target)
                                                                                                            :instance-msg ddf-message
                                                                                                            :transform transform})])))

  (output scene g/Any :cached (g/fnk [_node-id transform scene child-scenes embedded]
                                     (let [aabb (reduce #(geom/aabb-union %1 (:aabb %2)) (:aabb scene) child-scenes)
                                           aabb (geom/aabb-transform (geom/aabb-incorporate aabb 0 0 0) transform)]
                                       (merge-with concat
                                                   (assoc (assoc-deep scene :node-id _node-id)
                                                          :transform transform
                                                          :aabb aabb
                                                          :renderable {:passes [pass/selection]})
                                                   {:children child-scenes})))))

(g/defnk produce-proto-msg [name ref-inst-ddf embed-inst-ddf ref-coll-ddf]
  {:name name
   :instances ref-inst-ddf
   :embedded-instances embed-inst-ddf
   :collection-instances ref-coll-ddf})

(g/defnk produce-save-data [resource proto-msg]
  {:resource resource
   :content (protobuf/map->str GameObject$CollectionDesc proto-msg)})

(defn- externalize [inst-data resources]
  (map (fn [data]
         (let [{:keys [resource instance-msg transform]} data
               instance-msg (dissoc instance-msg :data)
               resource (get resources resource)
               pos (Point3d.)
               rot (Quat4d.)
               scale (Vector3d.)
               _ (math/split-mat4 transform pos rot scale)]
           (merge instance-msg
                  {:id (str path-sep (:id instance-msg))
                   :prototype (workspace/proj-path resource)
                   :children (map #(str path-sep %) (:children instance-msg))
                   :position (math/vecmath->clj pos)
                   :rotation (math/vecmath->clj rot)
                   :scale3 (math/vecmath->clj scale)}))) inst-data))

(defn build-collection [self basis resource dep-resources user-data]
  (let [{:keys [name instance-data]} user-data
        instance-msgs (externalize instance-data dep-resources)
        msg {:name name
             :instances instance-msgs}]
    {:resource resource :content (protobuf/map->bytes GameObject$CollectionDesc msg)}))

(g/defnk produce-build-targets [_node-id name resource proto-msg sub-build-targets dep-build-targets]
  (let [sub-build-targets (flatten sub-build-targets)
        dep-build-targets (flatten dep-build-targets)
        instance-data (map :instance-data dep-build-targets)
        instance-data (reduce concat instance-data (map #(get-in % [:user-data :instance-data]) sub-build-targets))]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-collection
      :user-data {:name name :instance-data instance-data}
      :deps (vec (reduce into dep-build-targets (map :deps sub-build-targets)))}]))

(g/defnode CollectionNode
  (inherits project/ResourceNode)

  (property name g/Str)

  (input child-outlines g/Any :array)
  (input ref-inst-ddf g/Any :array)
  (input embed-inst-ddf g/Any :array)
  (input ref-coll-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input ids g/Str :array)
  (input sub-build-targets g/Any :array)
  (input dep-build-targets g/Any :array)

  (output proto-msg g/Any :cached produce-proto-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output outline g/Any :cached (g/fnk [_node-id child-outlines] {:node-id _node-id :label "Collection" :icon collection-icon :children child-outlines :sort-by-fn outline-sort-by-fn}))
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :children child-scenes
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))})))

(defn- flatten-instance-data [data base-id base-transform all-child-ids]
  (let [{:keys [resource instance-msg ^Matrix4d transform]} data
        is-child? (contains? all-child-ids (:id instance-msg))
        instance-msg {:id (str base-id (:id instance-msg))
                      :children (map #(str base-id %) (:children instance-msg))}
        transform (if is-child?
                    transform
                    (doto (Matrix4d. transform) (.mul base-transform transform)))]
    {:resource resource :instance-msg instance-msg :transform transform}))

(g/defnk produce-coll-inst-build-targets [id transform build-targets]
  (let [base-id (str id path-sep)
        instance-data (get-in build-targets [0 :user-data :instance-data])
        child-ids (reduce (fn [child-ids data] (into child-ids (:children (:instance-msg data)))) #{} instance-data)]
    (assoc-in build-targets [0 :user-data :instance-data] (map #(flatten-instance-data % base-id transform child-ids) instance-data))))

(g/defnode CollectionInstanceNode
  (inherits ScalableSceneNode)

  (property id g/Str)
  (property path g/Str)

  (input source g/Any)
  (input outline g/Any)
  (input save-data g/Any)
  (input scene g/Any)
  (input build-targets g/Any)

  (output outline g/Any :cached (g/fnk [_node-id id path outline] (let [suffix (format " (%s)" path)]
                                                                   (merge outline {:node-id _node-id :label (str id suffix) :icon collection-icon}))))
  (output ddf-message g/Any :cached (g/fnk [id path ^Vector3d position ^Quat4d rotation ^Vector3d scale]
                                           {:id id
                                            :collection path
                                            :position (math/vecmath->clj position)
                                            :rotation (math/vecmath->clj rotation)
                                            :scale3 (math/vecmath->clj scale)}))
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                     (assoc scene
                                           :node-id _node-id
                                           :transform transform
                                           :aabb (geom/aabb-transform (:aabb scene) transform)
                                           :renderable {:passes [pass/selection]})))
  (output build-targets g/Any produce-coll-inst-build-targets))

(defn- gen-instance-id [coll-node base]
  (let [ids (g/node-value coll-node :ids)]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- make-go [self project source-resource id position rotation scale]
  (let [path (workspace/proj-path source-resource)]
    (g/make-nodes (g/node->graph-id self)
                  [go-node [GameObjectInstanceNode :id id :path path
                            :position position :rotation rotation :scale scale]]
                  (concat
                    (g/connect go-node :_self         (g/node-id self) :nodes)
                    (g/connect go-node :ddf-message   (g/node-id self)    :ref-inst-ddf)
                    (g/connect go-node :build-targets (g/node-id self)    :dep-build-targets)
                    (g/connect go-node :id            (g/node-id self)    :ids)
                    (project/connect-resource-node project
                                                   source-resource go-node
                                                   [[:_self           :source]
                                                    [:outline        :outline]
                                                    [:save-data      :save-data]
                                                    [:build-targets  :build-targets]
                                                    [:scene          :scene]])))))

(defn- single-selection? [selection]
  (= 1 (count selection)))

(defn- selected-collection? [selection]
  (= CollectionNode (g/node-type (g/node-by-id (first selection)))))

(defn- selected-embedded-instance? [selection]
  (let [node (g/node-by-id (first selection))]
    (and (= GameObjectInstanceNode (g/node-type node))
         (:embedded node))))

(defn- add-game-object-file [selection]
  (let [coll-node (g/node-by-id (first selection))
        project (project/get-project coll-node)
        workspace (:workspace (:resource coll-node))
        ext "go"]
    (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext ext :title "Select Game Object File"}))]
      (let [base (FilenameUtils/getBaseName (workspace/resource-name resource))
            id (gen-instance-id coll-node base)
            op-seq (gensym)
            [go-node] (g/tx-nodes-added
                       (g/transact
                        (concat
                         (g/operation-label "Add Game Object")
                         (g/operation-sequence op-seq)
                         (make-go coll-node project resource id [0 0 0] [0 0 0] [1 1 1]))))]
        ; Selection
        (g/transact
         (concat
          (g/operation-sequence op-seq)
          (g/operation-label "Add Game Object")
          (g/connect (g/node-id go-node) :outline (g/node-id coll-node) :child-outlines)
          (g/connect (g/node-id go-node) :scene   (g/node-id coll-node) :child-scenes)
          (project/select project [go-node])))))))


(handler/defhandler :add-from-file :global
  (active? [selection] (and (single-selection? selection)
                            (or (selected-collection? selection)
                                (selected-embedded-instance? selection))))
  (label [selection] (cond
                       (selected-collection? selection) "Add Game Object File"
                       (selected-embedded-instance? selection) "Add Component File"))
  (run [selection] (cond
                     (selected-collection? selection) (add-game-object-file selection)
                     (selected-embedded-instance? selection) (game-object/add-component-handler (g/node-value (first selection) :source)))))

(defn- make-embedded-go [self project type data id position rotation scale]
  (let [resource (project/make-embedded-resource project type data)]
    (if-let [resource-type (and resource (workspace/resource-type resource))]
      (g/make-nodes (g/node->graph-id self)
                    [go-node [GameObjectInstanceNode :id id :embedded true
                              :position position :rotation rotation :scale scale]
                     source-node [(:node-type resource-type) :resource resource :project-id (g/node-id project)]]
                    (g/connect source-node :_self         go-node          :source)
                    (g/connect source-node :outline       go-node          :outline)
                    (g/connect source-node :save-data     go-node          :save-data)
                    (g/connect source-node :build-targets go-node          :build-targets)
                    (g/connect source-node :scene         go-node          :scene)
                    (g/connect source-node :_self         (g/node-id self) :nodes)
                    (g/connect go-node     :ddf-message   (g/node-id self) :embed-inst-ddf)
                    (g/connect go-node     :build-targets (g/node-id self) :dep-build-targets)
                    (g/connect go-node     :id            (g/node-id self) :ids)
                    (g/connect go-node     :_self         (g/node-id self) :nodes))
      (g/make-nodes (g/node->graph-id self)
                    [go-node [GameObjectInstanceNode :id id :embedded true]]
                    (g/connect go-node     :_self         (g/node-id self)    :nodes)))))

(defn- add-game-object [selection]
  (let [coll-node     (g/node-by-id (first selection))
        project       (project/get-project coll-node)
        workspace     (:workspace (:resource coll-node))
        ext           "go"
        resource-type (workspace/get-resource-type workspace ext)
        template      (workspace/template resource-type)]
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
        (g/connect (g/node-id go-node) :outline (g/node-id coll-node) :child-outlines)
        (g/connect (g/node-id go-node) :scene   (g/node-id coll-node) :child-scenes)
        ((:load-fn resource-type) project source-node (io/reader (g/node-value source-node :resource)))
        (project/select project [(g/node-id go-node)]))))))

(handler/defhandler :add :global
  (active? [selection] (and (single-selection? selection)
                            (or (selected-collection? selection)
                                (selected-embedded-instance? selection))))
  (label [selection] (cond
                       (selected-collection? selection) "Add Game Object"
                       (selected-embedded-instance? selection)  "Add Component"))
  (run [selection] (cond
                     (selected-collection? selection) (add-game-object selection)
                     (selected-embedded-instance? selection) (game-object/add-embedded-component-handler (g/node-value (first selection) :source)))))

(defn- add-collection-instance [self source-resource id position rotation scale]
  (let [project (g/node-by-id (:project-id self)) path (workspace/proj-path source-resource)]
    (g/make-nodes (g/node->graph-id self)
                  [coll-node [CollectionInstanceNode :id id :path path
                              :position position :rotation rotation :scale scale]]
                  (g/connect coll-node :outline       (g/node-id self) :child-outlines)
                  (g/connect coll-node :_self         (g/node-id self) :nodes)
                  (g/connect coll-node :ddf-message   (g/node-id self) :ref-coll-ddf)
                  (g/connect coll-node :id            (g/node-id self) :ids)
                  (g/connect coll-node :scene         (g/node-id self) :child-scenes)
                  (g/connect coll-node :build-targets (g/node-id self) :sub-build-targets)
                  (project/connect-resource-node project
                                                 source-resource coll-node
                                                 [[:_self          :source]
                                                  [:outline       :outline]
                                                  [:save-data     :save-data]
                                                  [:scene         :scene]
                                                  [:build-targets :build-targets]]))))

(handler/defhandler :add-secondary-from-file :global
  (active? [selection] (and (single-selection? selection) (selected-collection? selection)))
  (label [] "Add Collection File")
  (run [selection] (let [coll-node (g/node-by-id (first selection))
                         project (project/get-project coll-node)
                         workspace (:workspace (:resource coll-node))
                         ext "collection"
                         resource-type (workspace/get-resource-type workspace ext)]
                     (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext ext :title "Select Collection File"}))]
                       (let [base (FilenameUtils/getBaseName (workspace/resource-name resource))
                             id (gen-instance-id coll-node base)
                             op-seq (gensym)
                             [coll-inst-node] (g/tx-nodes-added
                                               (g/transact
                                                (concat
                                                 (g/operation-label "Add Collection")
                                                 (g/operation-sequence op-seq)
                                                 (add-collection-instance coll-node resource id [0 0 0] [0 0 0] [1 1 1]))))]
                                        ; Selection
                         (g/transact
                          (concat
                           (g/operation-sequence op-seq)
                           (g/operation-label "Add Collection")
                           (project/select project [coll-inst-node]))))))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(defn load-collection [project self input]
  (let [collection (protobuf/read-text GameObject$CollectionDesc input)
        project-graph (g/node->graph-id project)]
    (concat
      (g/set-property (g/node-id self) :name (:name collection))
      (let [tx-go-creation (flatten
                             (concat
                               (for [game-object (:instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale game-object)
                                           source-resource (workspace/resolve-resource (:resource self) (:prototype game-object))]]
                                 (make-go self project source-resource (:id game-object) (:position game-object)
                                          (v4->euler (:rotation game-object)) [scale scale scale]))
                               (for [embedded (:embedded-instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale embedded)]]
                                 (make-embedded-go self project "go" (:data embedded) (:id embedded)
                                                   (:position embedded)
                                                   (v4->euler (:rotation embedded))
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
                      parent-id (if parent (id->nid parent) (g/node-id self))]]
            (concat
             (g/connect child-id :outline parent-id :child-outlines)
             (g/connect child-id :scene parent-id :child-scenes)
             (if parent
               (g/connect child-id :id parent-id :child-ids)
               [])))))
      (for [coll-instance (:collection-instances collection)
            :let [; TODO - fix non-uniform hax
                  scale (:scale coll-instance)
                  source-resource (workspace/resolve-resource (:resource self) (:collection coll-instance))]]
        (add-collection-instance self source-resource (:id coll-instance) (:position coll-instance)
                                 (v4->euler (:rotation coll-instance)) [scale scale scale])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collection"
                                    :node-type CollectionNode
                                    :load-fn load-collection
                                    :icon collection-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}
                                    :template "templates/template.collection"))

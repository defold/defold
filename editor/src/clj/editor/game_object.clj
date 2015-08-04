(ns editor.game-object
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.types :as types]
            [editor.sound :as sound]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.properties :as properties])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]
           [org.apache.commons.io FilenameUtils]))

(def game-object-icon "icons/32/Icons_06-Game-object.png")

(defn- gen-ref-ddf [id ^Vector3d position ^Quat4d rotation properties user-properties save-data]
  (let [props (map (fn [[k v]] {:id k :type (get-in v [:edit-type :go-prop-type])}) user-properties)
        props (mapv (fn [p] (assoc p :value (properties/go-prop->str (get properties (:id p)) (:type p))))
                    (filter #(contains? properties (:id %)) props))]
    {:id id
     :position (math/vecmath->clj position)
     :rotation (math/vecmath->clj rotation)
     :component (or (and (:resource save-data) (workspace/proj-path (:resource save-data)))
                    ".unknown")
     :properties props}))

(defn- gen-embed-ddf [id ^Vector3d position ^Quat4d rotation save-data]
  {:id id
   :type (or (and (:resource save-data) (:ext (workspace/resource-type (:resource save-data))))
             "unknown")
   :position (math/vecmath->clj position)
   :rotation (math/vecmath->clj rotation)
   :data (or (:content save-data) "")})

(def sound-exts (into #{} (map :ext sound/sound-defs)))

(defn- wrap-if-raw-sound [_node-id project-id target]
  (let [source-path (workspace/proj-path (:resource (:resource target)))
        ext (FilenameUtils/getExtension source-path)]
    (if (sound-exts ext)
      (let [workspace (project/workspace project-id)
            res-type  (workspace/get-resource-type workspace "sound")
            pb        {:sound source-path}
            target    {:node-id  _node-id
                       :resource (workspace/make-build-resource (workspace/make-memory-resource workspace res-type
                                                                                                (protobuf/map->str Sound$SoundDesc pb)))
                       :build-fn (fn [self basis resource dep-resources user-data]
                                   (let [pb (:pb user-data)
                                         pb (assoc pb :sound (workspace/proj-path (second (first dep-resources))))]
                                     {:resource resource :content (protobuf/map->bytes Sound$SoundDesc pb)}))
                       :deps     [target]}]
        target)
      target)))

(g/defnode ComponentNode
  (inherits scene/SceneNode)

  (property id g/Str)

  (property embedded g/Bool (dynamic visible (g/always false)))
  (property path g/Str
            (dynamic visible (g/fnk [embedded] (false? embedded)))
            (dynamic enabled (g/always false)))
  (property properties g/Any
            (dynamic link (g/fnk [source-properties] source-properties))
            (dynamic override (g/fnk [user-properties] user-properties)))

  (display-order [:id :path scene/SceneNode])

  (input source-properties g/Any)
  (input user-properties g/Any)
  (input project-id g/NodeID)
  (input outline g/Any)
  (input save-data g/Any)
  (input scene g/Any)
  (input build-targets g/Any)

  (output outline g/Any :cached (g/fnk [_node-id embedded path id outline] (let [suffix (if embedded "" (format " (%s)" path))]
                                                                            (assoc outline :node-id _node-id :label (str id suffix)))))
  (output ddf-message g/Any :cached (g/fnk [id embedded position rotation properties user-properties save-data]
                                           (if embedded
                                             (gen-embed-ddf id position rotation save-data)
                                             (gen-ref-ddf id position rotation properties (:properties user-properties) save-data))))
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                     (assoc scene
                                            :node-id _node-id
                                            :transform transform
                                            :aabb (geom/aabb-transform (geom/aabb-incorporate (get scene :aabb (geom/null-aabb)) 0 0 0) transform))))
  (output build-targets g/Any :cached (g/fnk [_node-id project-id build-targets ddf-message transform]
                                             (if-let [target (first build-targets)]
                                               (let [target (wrap-if-raw-sound _node-id project-id target)]
                                                 [(assoc target :instance-data {:resource (:resource target)
                                                                               :instance-msg ddf-message
                                                                               :transform transform})])
                                               []))))

(g/defnk produce-proto-msg [ref-ddf embed-ddf]
  {:components ref-ddf
   :embedded-components embed-ddf})

(g/defnk produce-save-data [resource proto-msg]
  {:resource resource
   :content (protobuf/map->str GameObject$PrototypeDesc proto-msg)})

(defn- externalize [inst-data resources]
  (map (fn [data]
         (let [{:keys [resource instance-msg transform]} data
               resource (get resources resource)
               instance-msg (dissoc instance-msg :type :data)]
           (merge instance-msg
                  {:component (workspace/proj-path resource)})))
       inst-data))

(defn- build-props [component]
  (let [properties (mapv #(assoc % :value (properties/str->go-prop (:value %) (:type %))) (:properties component))]
    (assoc component :property-decls (properties/properties->decls properties))))

(defn- build-game-object [self basis resource dep-resources user-data]
  (let [instance-msgs (externalize (:instance-data user-data) dep-resources)
        instance-msgs (mapv build-props instance-msgs)
        msg {:components instance-msgs}]
    {:resource resource :content (protobuf/map->bytes GameObject$PrototypeDesc msg)}))

(g/defnk produce-build-targets [_node-id resource proto-msg dep-build-targets]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-game-object
    :user-data {:proto-msg proto-msg :instance-data (map :instance-data (flatten dep-build-targets))}
    :deps (flatten dep-build-targets)}])

(g/defnk produce-scene [_node-id child-scenes]
  {:node-id _node-id
   :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))
   :children child-scenes})

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input outline g/Any :array)
  (input ref-ddf g/Any :array)
  (input embed-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input child-ids g/Str :array)
  (input dep-build-targets g/Any :array)
  (input component-properties g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id outline] {:node-id _node-id :label "Game Object" :icon game-object-icon :children outline}))
  (output proto-msg g/Any :cached produce-proto-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene))

(defn- connect-if-output [out-node out-label in-node in-label]
  (if ((-> out-node g/node-type g/output-labels) out-label)
    (g/connect (g/node-id out-node) out-label (g/node-id in-node) in-label)
    []))

(defn- gen-component-id [go-node base]
  (let [ids (g/node-value go-node :child-ids)]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- add-component [self project source-resource id position rotation properties]
  (let [path (workspace/proj-path source-resource)
        properties (into {} (map (fn [p] [(:id p) (properties/str->go-prop (:value p) (:type p))]) properties))]
    (g/make-nodes (g/node-id->graph-id self)
                  [comp-node [ComponentNode :id id :position position :rotation rotation :path path :properties properties]]
                  (concat
                    (for [[src tgt] [[:outline :outline]
                                     [:properties :component-properties]
                                     [:_id :nodes]
                                     [:build-targets :dep-build-targets]
                                     [:ddf-message :ref-ddf]
                                     [:id :child-ids]
                                     [:scene :child-scenes]]]
                      (g/connect comp-node src self tgt))
                    (project/connect-resource-node project
                                                   source-resource comp-node
                                                   [[:outline :outline]
                                                    [:user-properties :user-properties]
                                                    [:save-data :save-data]
                                                    [:scene :scene]
                                                    [:build-targets :build-targets]
                                                    [:project-id :project-id]])))))

(defn add-component-handler [self]
  (let [project (project/get-project self)
        workspace (:workspace (g/node-value self :resource))
        component-exts (map :ext (workspace/get-resource-types workspace :component))]
    (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext component-exts :title "Select Component File"}))]
      (let [id (gen-component-id self (:ext (workspace/resource-type resource)))
            op-seq (gensym)
            [comp-node] (g/tx-nodes-added
                          (g/transact
                            (concat
                              (g/operation-label "Add Component")
                              (g/operation-sequence op-seq)
                              (add-component self project resource id [0 0 0] [0 0 0] {}))))]
        ; Selection
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (g/operation-label "Add Component")
            (project/select project [comp-node])))))))

(handler/defhandler :add-from-file :global
  (active? [selection] (and (= 1 (count selection)) (= GameObjectNode (g/node-type (g/node-by-id (first selection))))))
  (label [] "Add Component File")
  (run [selection] (add-component-handler (first selection))))

(defn- add-embedded-component [self project type data id position rotation]
  (let [resource (project/make-embedded-resource project type data)]
    (if-let [resource-type (and resource (workspace/resource-type resource))]
      (g/make-nodes (g/node-id->graph-id self)
                    [comp-node [ComponentNode :id id :embedded true :position position :rotation rotation]
                     source-node [(:node-type resource-type) :resource resource :project-id project]]
                    (g/connect source-node :_properties   comp-node :source-properties)
                    (g/connect source-node :outline       comp-node :outline)
                    (g/connect source-node :save-data     comp-node :save-data)
                    (g/connect source-node :scene         comp-node :scene)
                    (g/connect source-node :build-targets comp-node :build-targets)
                    (g/connect source-node :project-id    comp-node :project-id)
                    (g/connect source-node :_id           self      :nodes)
                    (g/connect comp-node   :outline       self      :outline)
                    (g/connect comp-node   :ddf-message   self      :embed-ddf)
                    (g/connect comp-node   :id            self      :child-ids)
                    (g/connect comp-node   :scene         self      :child-scenes)
                    (g/connect comp-node   :_id           self      :nodes)
                    (g/connect comp-node   :build-targets self      :dep-build-targets))
      (g/make-nodes (g/node-id->graph-id self)
                    [comp-node [ComponentNode :id id :embedded true]]
                    (g/connect comp-node   :outline      self       :outline)
                    (g/connect comp-node   :_id          self       :nodes)))))

(defn add-embedded-component-handler
  ([self]
   (let [workspace (:workspace (g/node-value self :resource))
         component-type (first (workspace/get-resource-types workspace :component))]
     (add-embedded-component-handler self component-type)))
  ([self component-type]
   (let [project (project/get-project self)
         template (workspace/template component-type)]
    (let [id (gen-component-id self (:ext component-type))
          op-seq (gensym)
          [comp-node source-node] (g/tx-nodes-added
                                   (g/transact
                                    (concat
                                     (g/operation-sequence op-seq)
                                     (g/operation-label "Add Component")
                                     (add-embedded-component self project (:ext component-type) template id [0 0 0] [0 0 0]))))]
      (g/transact
       (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Add Component")
        ((:load-fn component-type) project source-node (io/reader (g/node-value source-node :resource)))
        (project/select project [comp-node])))))))

(handler/defhandler :add :global
  (label [user-data] (if-not user-data
                       "Add Component"
                       (let [rt (:resource-type user-data)]
                         (or (:label rt) (:ext rt)))))
  (active? [selection] (and (= 1 (count selection)) (= GameObjectNode (g/node-type (g/node-by-id (first selection))))))
  (run [user-data] (add-embedded-component-handler (:_id user-data) (:resource-type user-data)))
  (options [selection user-data]
           (when (not user-data)
             (let [self (first selection)
                   project (project/get-project self)
                   workspace (:workspace (g/node-value self :resource))
                   resource-types (workspace/get-resource-types workspace :component)]
               (mapv (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                                     :icon (:icon res-type)
                                     :command :add
                                     :user-data {:_id self :resource-type res-type}}) resource-types)))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(defn load-game-object [project self input]
  (let [project-graph (g/node-id->graph-id self)
        prototype     (protobuf/read-text GameObject$PrototypeDesc input)]
    (concat
      (for [component (:components prototype)
            :let [source-resource (workspace/resolve-resource (g/node-value self :resource) (:component component))]]
        (add-component self project source-resource (:id component) (:position component) (v4->euler (:rotation component)) (:properties component)))
      (for [embedded (:embedded-components prototype)]
        (add-embedded-component self project (:type embedded) (:data embedded) (:id embedded) (:position embedded) (v4->euler (:rotation embedded)))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :label "Game Object"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :icon game-object-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))

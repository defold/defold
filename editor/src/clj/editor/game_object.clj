(ns editor.game-object
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.graph-util :as gu]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.types :as types]
            [editor.sound :as sound]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.properties :as properties]
            [editor.validation :as validation]
            [editor.outline :as outline])
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

(set! *warn-on-reflection* true)

(def game-object-icon "icons/32/Icons_06-Game-object.png")
(def unknown-icon "icons/32/Icons_29-AT-Unkown.png") ; spelling...

(defn- gen-ref-ddf
  ([id position ^Quat4d rotation-q4 path]
    (gen-ref-ddf id position rotation-q4 path {}))
  ([id position ^Quat4d rotation-q4 path ddf-properties ddf-property-decls]
      {:id id
       :position position
       :rotation (math/vecmath->clj rotation-q4)
       :component (resource/resource->proj-path path)
     :properties ddf-properties
     :property-decls ddf-property-decls}))

(defn- gen-embed-ddf [id position ^Quat4d rotation-q4 save-data]
  {:id id
   :type (or (and (:resource save-data) (:ext (resource/resource-type (:resource save-data))))
             "unknown")
   :position position
   :rotation (math/vecmath->clj rotation-q4)
   :data (or (:content save-data) "")})

(def sound-exts (into #{} (map :ext sound/sound-defs)))

(defn- wrap-if-raw-sound [_node-id target]
  (let [source-path (resource/proj-path (:resource (:resource target)))
        ext (FilenameUtils/getExtension source-path)]
    (if (sound-exts ext)
      (let [workspace (project/workspace (project/get-project _node-id))
            res-type  (workspace/get-resource-type workspace "sound")
            pb        {:sound source-path}
            target    {:node-id  _node-id
                       :resource (workspace/make-build-resource (resource/make-memory-resource workspace res-type (protobuf/map->str Sound$SoundDesc pb)))
                       :build-fn (fn [self basis resource dep-resources user-data]
                                   (let [pb (:pb user-data)
                                         pb (assoc pb :sound (resource/proj-path (second (first dep-resources))))]
                                     {:resource resource :content (protobuf/map->bytes Sound$SoundDesc pb)}))
                       :deps     [target]}]
        target)
      target)))

(defn- source-outline-subst [err]
  ;; TODO: embed error
  {:node-id 0
   :icon ""
   :label ""})

(g/defnode ComponentNode
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property id g/Str)
  (property url g/Str
            (value (g/fnk [base-url id] (format "%s#%s" (or base-url "") id)))
            (dynamic read-only? (g/fnk [] true)))

  (display-order [:id :url :path scene/SceneNode])

  (input source-resource resource/Resource)
  (input source-properties g/Properties :substitute {:properties {}})
  (input scene g/Any)
  (input build-targets g/Any)
  (input base-url g/Str)

  (input source-outline outline/OutlineData :substitute source-outline-subst)

  (output component-id g/IdPair (g/fnk [_node-id id] [id _node-id]))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id node-outline-label id source-outline source-properties]
      (let [source-outline (or source-outline {:icon unknown-icon})
            overridden? (boolean (some (fn [[_ p]] (contains? p :original-value)) (:properties source-properties)))]
        (assoc source-outline :node-id _node-id :label node-outline-label
               :outline-overridden? overridden?))))
  (output node-outline-label g/Str (g/fnk [id] id))
  (output ddf-message g/Any :cached (g/fnk [rt-ddf-message] (dissoc rt-ddf-message :property-decls)))
  (output rt-ddf-message g/Any :abstract)
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                     (-> scene
                                       (assoc :node-id _node-id
                                              :transform transform
                                              :aabb (geom/aabb-transform (geom/aabb-incorporate (get scene :aabb (geom/null-aabb)) 0 0 0) transform))
                                       (update :node-path (partial cons _node-id)))))
  (output build-targets g/Any :cached (g/fnk [_node-id build-targets rt-ddf-message transform]
                                             (if-let [target (first build-targets)]
                                               (let [target (wrap-if-raw-sound _node-id target)]
                                                 [(assoc target :instance-data {:resource (:resource target)
                                                                                :instance-msg rt-ddf-message
                                                                                :transform transform})])
                                               []))))

(g/defnode EmbeddedComponent
  (inherits ComponentNode)

  (input save-data g/Any :cascade-delete)
  (output rt-ddf-message g/Any :cached (g/fnk [id position ^Quat4d rotation-q4 save-data]
                                           (gen-embed-ddf id position rotation-q4 save-data)))
  (output _properties g/Properties :cached (g/fnk [_declared-properties source-properties]
                                                  (merge-with into _declared-properties source-properties))))

(g/defnode ReferencedComponent
  (inherits ComponentNode)

  (property path g/Any
            (dynamic edit-type (g/fnk [source-resource]
                                      {:type (s/protocol resource/Resource)
                                       :ext (some-> source-resource resource/resource-type :ext)
                                       :to-type (fn [v] (:resource v))
                                       :from-type (fn [r] {:resource r :overrides {}})}))
            (value (g/fnk [source-resource ddf-properties]
                          {:resource source-resource
                           :overrides (into {} (map (fn [p] [(properties/user-name->key (:id p)) (properties/str->go-prop (:value p) (:type p))])
                                                    ddf-properties))}))
            (set (fn [basis self old-value new-value]
                   (concat
                    (if-let [old-source (g/node-value self :source-id :basis basis)]
                      (g/delete-node old-source)
                      [])
                    (let [new-resource (:resource new-value)
                          resource-type (and new-resource (resource/resource-type new-resource))
                          override? (contains? (:tags resource-type) :overridable-properties)]
                      (if override?
                        (let [project (project/get-project self)]
                          (project/connect-resource-node project new-resource self []
                                                         (fn [comp-node]
                                                            (let [override (g/override basis comp-node {:traverse? (constantly true)})
                                                                 id-mapping (:id-mapping override)
                                                                 or-node (get id-mapping comp-node)]
                                                             (concat
                                                              (:tx-data override)
                                                              (let [outputs (g/output-labels (:node-type (resource/resource-type new-resource)))]
                                                                (for [[from to] [[:_node-id :source-id]
                                                                                 [:resource :source-resource]
                                                                                 [:node-outline :source-outline]
                                                                                 [:_properties :source-properties]
                                                                                 [:scene :scene]
                                                                                 [:build-targets :build-targets]]
                                                                      :when (contains? outputs from)]
                                                                  (g/connect or-node from self to)))
                                                              (for [[label value] (:overrides new-value)]
                                                                (g/set-property or-node label value)))))))
                        (project/resource-setter basis self (:resource old-value) (:resource new-value)
                                                 [:resource :source-resource]
                                                 [:node-outline :source-outline]
                                                 [:user-properties :user-properties]
                                                 [:scene :scene]
                                                 [:build-targets :build-targets]))))))
            (validate (g/fnk [path]
                             (when (nil? path)
                               (g/error-warning "Missing component")))))

  (input source-id g/NodeID :cascade-delete)
  (output ddf-properties g/Any :cached
          (g/fnk [source-properties]
                 (let [prop-order (into {} (map-indexed (fn [i k] [k i]) (:display-order source-properties)))]
                   (->> source-properties
                     :properties
                     (filter (fn [[key p]] (contains? p :original-value)))
                     (sort-by (comp prop-order first))
                     (mapv (fn [[key p]]
                             {:id (properties/key->user-name key)
                              :type (:go-prop-type p)
                              :value (properties/go-prop->str (:value p) (:go-prop-type p))}))))))
  (output ddf-property-decls g/Any :cached (g/fnk [ddf-properties] (properties/properties->decls ddf-properties)))
  (output node-outline-label g/Str :cached (g/fnk [id source-resource]
                                                  (format "%s - %s" id (resource/resource->proj-path source-resource))))
  (output rt-ddf-message g/Any :cached (g/fnk [id position ^Quat4d rotation-q4 source-resource ddf-properties ddf-property-decls]
                                              (gen-ref-ddf id position rotation-q4 source-resource ddf-properties ddf-property-decls)))
  ;; TODO - cache it, not possible now because of dynamic prop invalidation bug
  (output _properties g/Properties #_:cached (g/fnk [_declared-properties source-properties]
                                                    (merge-with into _declared-properties source-properties))))

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
                  {:component (resource/proj-path resource)})))
       inst-data))

(defn- build-game-object [self basis resource dep-resources user-data]
  (let [instance-msgs (externalize (:instance-data user-data) dep-resources)
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

(defn- attach-component [self-id comp-id ddf-input]
  (concat
    (for [[from to] [[:node-outline :child-outlines]
               [:_node-id :nodes]
               [:build-targets :dep-build-targets]
                     [:ddf-message ddf-input]
                     [:component-id :component-ids]
               [:scene :child-scenes]]]
      (g/connect comp-id from self-id to))
    (for [[from to] [[:base-url :base-url]]]
      (g/connect self-id from comp-id to))))

(defn- attach-ref-component [self-id comp-id]
  (concat
    (attach-component self-id comp-id :ref-ddf)))

(defn- attach-embedded-component [self-id comp-id]
  (attach-component self-id comp-id :embed-ddf))

(g/defnk produce-go-outline [_node-id child-outlines]
  {:node-id _node-id
   :label "Game Object"
   :icon game-object-icon
   :children child-outlines
   :child-reqs [{:node-type ReferencedComponent
                 :tx-attach-fn attach-ref-component}
                {:node-type EmbeddedComponent
                 :tx-attach-fn attach-embedded-component}]})

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input ref-ddf g/Any :array)
  (input embed-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input component-ids g/IdPair :array)
  (input dep-build-targets g/Any :array)
  (input base-url g/Str)

  (output base-url g/Str (g/fnk [base-url] base-url))
  (output node-outline outline/OutlineData :cached produce-go-outline)
  (output proto-msg g/Any :cached produce-proto-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output component-ids g/Dict :cached (g/fnk [component-ids] (reduce conj {} component-ids)))
  (output ddf-component-properties g/Any :cached
          (g/fnk [ref-ddf]
                 (reduce (fn [props m]
                           (if (empty? (:properties m))
                             props
                             (conj props (-> m
                                           (select-keys [:id :properties])
                                           (assoc :property-decls (properties/properties->decls (:properties m)))))))
                         [] ref-ddf))))

(defn- gen-component-id [go-node base]
  (let [ids (map first (g/node-value go-node :component-ids))]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- add-component [self project source-resource id position rotation properties]
  (let [resource-type (and source-resource (resource/resource-type source-resource))
        path {:resource source-resource
              :overrides properties}]
    (g/make-nodes (g/node-id->graph-id self)
                  [comp-node [ReferencedComponent :id id :position position :rotation rotation :path path]]
                  (attach-ref-component self comp-node))))

(defn add-component-file [go-id resource]
  (let [project (project/get-project go-id)
        id (gen-component-id go-id (:ext (resource/resource-type resource)))
            op-seq (gensym)
            [comp-node] (g/tx-nodes-added
                          (g/transact
                            (concat
                              (g/operation-label "Add Component")
                              (g/operation-sequence op-seq)
                          (add-component go-id project resource id [0 0 0] [0 0 0] {}))))]
        ; Selection
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (g/operation-label "Add Component")
        (project/select project [comp-node])))))

(defn add-component-handler [workspace go-id]
  (let [component-exts (map :ext (workspace/get-resource-types workspace :component))]
    (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext component-exts :title "Select Component File"}))]
      (add-component-file go-id resource))))

(handler/defhandler :add-from-file :global
  (active? [selection] (and (= 1 (count selection)) (g/node-instance? GameObjectNode (first selection))))
  (label [] "Add Component File")
  (run [workspace selection]
       (add-component-handler workspace (first selection))))

(defn- add-embedded-component [self project type data id position rotation select?]
  (let [graph (g/node-id->graph-id self)
        resource (project/make-embedded-resource project type data)]
    (g/make-nodes graph [comp-node [EmbeddedComponent :id id :position position :rotation rotation]]
      (g/connect comp-node :_node-id self :nodes)
      (if select?
        (project/select project [comp-node])
        [])
      (let [tx-data (project/make-resource-node graph project resource true {comp-node [[:resource :source-resource]
                                                                                        [:_properties :source-properties]
                                                                                        [:node-outline :source-outline]
                                                                                        [:save-data :save-data]
                                                                                        [:scene :scene]
                                                                                        [:build-targets :build-targets]]
                                                                             self [[:_node-id :nodes]]})]
        (concat
          tx-data
          (if (empty? tx-data)
            []
            (attach-embedded-component self comp-node)))))))

(defn add-embedded-component-handler [user-data]
  (let [self (:_node-id user-data)
        project (project/get-project self)
        component-type (:resource-type user-data)
        template (workspace/template component-type)]
    (let [id (gen-component-id self (:ext component-type))
          op-seq (gensym)
          [comp-node source-node] (g/tx-nodes-added
                                   (g/transact
                                    (concat
                                     (g/operation-sequence op-seq)
                                     (g/operation-label "Add Component")
                                     (add-embedded-component self project (:ext component-type) template id [0 0 0] [0 0 0] true))))]
      (g/transact
       (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Add Component")
        ((:load-fn component-type) project source-node (io/reader (g/node-value source-node :resource)))
        (project/select project [comp-node]))))))

(defn add-embedded-component-label [user-data]
  (if-not user-data
    "Add Component"
    (let [rt (:resource-type user-data)]
      (or (:label rt) (:ext rt)))))

(defn add-embedded-component-options [self workspace user-data]
  (when (not user-data)
    (let [resource-types (workspace/get-resource-types workspace :component)]
      (mapv (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                            :icon (:icon res-type)
                            :command :add
                            :user-data {:_node-id self :resource-type res-type}})
            resource-types))))

(handler/defhandler :add :global
  (label [user-data] (add-embedded-component-label user-data))
  (active? [selection] (and (= 1 (count selection)) (g/node-instance? GameObjectNode (first selection))))
  (run [user-data] (add-embedded-component-handler user-data))
  (options [selection user-data]
           (let [self (first selection)
                 workspace (:workspace (g/node-value self :resource))]
             (add-embedded-component-options self workspace user-data))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(defn load-game-object [project self resource]
  (let [project-graph (g/node-id->graph-id self)
        prototype     (protobuf/read-text GameObject$PrototypeDesc resource)]
    (concat
      (for [component (:components prototype)
            :let [source-resource (workspace/resolve-resource resource (:component component))
                  properties (into {} (map (fn [p] [(properties/user-name->key (:id p)) (properties/str->go-prop (:value p) (:type p))]) (:properties component)))]]
        (add-component self project source-resource (:id component) (:position component) (v4->euler (:rotation component)) properties))
      (for [embedded (:embedded-components prototype)]
        (add-embedded-component self project (:type embedded) (:data embedded) (:id embedded) (:position embedded) (v4->euler (:rotation embedded)) false)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :label "Game Object"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :icon game-object-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}))

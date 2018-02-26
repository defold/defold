(ns editor.collection
  (:require [clojure.java.io :as io]
            [editor.core :as core]
            [schema.core :as s]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.protobuf :as protobuf]
            [editor.graph-util :as gu]
            [editor.dialogs :as dialogs]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.code.script :as script]
            [editor.outline :as outline]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [editor.util :as util]
            [service.log :as log]
            [clojure.string :as str])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc GameObject$CollectionDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io StringReader PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def collection-icon "icons/32/Icons_09-Collection.png")
(def path-sep "/")

(defn- gen-embed-ddf [id child-ids position rotation scale proto-msg]
  {:id id
   :children (sort child-ids)
   :data (protobuf/map->str GameObject$PrototypeDesc proto-msg false)
   :position position
   :rotation rotation
   :scale3 scale})

(defn- gen-ref-ddf [id child-ids position rotation scale path ddf-component-properties]
  {:id id
   :children (sort child-ids)
   :prototype (resource/resource->proj-path path)
   :position position
   :rotation rotation
   :scale3 scale
   :component-properties ddf-component-properties})

(g/defnode InstanceNode
  (inherits outline/OutlineNode)
  (inherits core/Scope)
  (property id g/Str
            (dynamic error (g/fnk [_node-id id id-counts]
                             (validation/prop-error :fatal _node-id :id (partial validation/prop-id-duplicate? id-counts) id)))
            (dynamic read-only? (g/fnk [_node-id]
                                  (g/override? _node-id))))
  (property url g/Str
            (value (g/fnk [base-url id] (format "%s/%s" (or base-url "") id)))
            (dynamic read-only? (g/constantly true)))
  (input base-url g/Str)
  (input source-id g/NodeID :cascade-delete)
  (input id-counts g/Any))

(defn- child-go-go [go-id child-id]
  (for [[from to] [[:_node-id :nodes]
                   [:id :child-ids]
                   [:node-outline :child-outlines]
                   [:scene :child-scenes]]]
    (g/connect child-id from go-id to)))

(defn- child-coll-any [coll-id child-id]
  (for [[from to] [[:_node-id :nodes]
                   [:node-outline :child-outlines]
                   [:scene :child-scenes]]]
    (g/connect child-id from coll-id to)))

(defn- attach-coll-go [coll-id child-id ddf-label]
  (concat
    (for [[from to] [[:ddf-message ddf-label]
                     [:build-targets :dep-build-targets]
                     [:id :ids]
                     [:go-inst-ids :go-inst-ids]
                     [:ddf-properties :ddf-properties]]]
      (g/connect child-id from coll-id to))
    (for [[from to] [[:base-url :base-url]
                     [:id-counts :id-counts]]]
      (g/connect coll-id from child-id to))))

(defn- attach-coll-ref-go [coll-id child-id]
  (attach-coll-go coll-id child-id :ref-inst-ddf))

(defn- attach-coll-embedded-go [coll-id child-id]
  (attach-coll-go coll-id child-id :embed-inst-ddf))

(defn- attach-coll-coll [coll-id child-id]
  (concat
    (for [[from to] [[:_node-id :nodes]
                     [:ddf-message :ref-coll-ddf]
                     [:id :ids]
                     [:build-targets :sub-build-targets]
                     [:go-inst-ids :go-inst-ids]
                     [:sub-ddf-properties :ddf-properties]]]
      (g/connect child-id from coll-id to))
    (for [[from to] [[:base-url :base-url]
                     [:id-counts :id-counts]]]
      (g/connect coll-id from child-id to))))

(declare EmbeddedGOInstanceNode ReferencedGOInstanceNode CollectionNode)

(defn- go-id->node-ids [go-id]
  (let [collection (core/scope-of-type go-id CollectionNode)]
    (g/node-value collection :ids)))

(g/defnk produce-go-outline [_node-id id source-outline source-resource child-outlines node-outline-extras]
  (-> {:node-id _node-id
       :label id
       :icon (or (not-empty (:icon source-outline)) game-object/game-object-icon)
       :children (into (outline/natural-sort child-outlines) (:children source-outline))
       :child-reqs [{:node-type ReferencedGOInstanceNode
                     :tx-attach-fn (fn [self-id child-id]
                                     (let [coll-id (core/scope-of-type self-id CollectionNode)]
                                       (concat
                                         (g/update-property child-id :id outline/resolve-id (go-id->node-ids self-id))
                                         (attach-coll-ref-go coll-id child-id)
                                         (child-go-go self-id child-id))))}
                    {:node-type EmbeddedGOInstanceNode
                     :tx-attach-fn (fn [self-id child-id]
                                     (let [coll-id (core/scope-of-type self-id CollectionNode)]
                                       (concat
                                         (g/update-property child-id :id outline/resolve-id (go-id->node-ids self-id))
                                         (attach-coll-embedded-go coll-id child-id)
                                         (child-go-go self-id child-id))))}]}
      (merge node-outline-extras)
      (cond->
        (resource/openable-resource? source-resource) (assoc :link source-resource :outline-reference? true))))

(defn- source-outline-subst [err]
  ;; TODO: embed error so can warn in outline
  ;; outline content not really used, only children if any.
  {:node-id 0
   :icon ""
   :label ""})

(g/defnode GameObjectInstanceNode
  (inherits scene/SceneNode)
  (inherits InstanceNode)

  (input properties g/Any)
  (input source-build-targets g/Any)
  (input source-resource resource/Resource)
  (input scene g/Any)
  (input child-scenes g/Any :array)
  (input child-ids g/Str :array)

  (input ddf-component-properties g/Any :substitute [])
  (input source-outline outline/OutlineData :substitute source-outline-subst)
  (output source-outline outline/OutlineData (gu/passthrough source-outline))

  (output transform-properties g/Any scene/produce-scalable-transform-properties)
  (output node-outline outline/OutlineData :cached produce-go-outline)
  (output ddf-message g/Any :abstract)
  (output node-outline-extras g/Any (g/constantly {}))
  (output build-resource resource/Resource (g/fnk [source-build-targets] (:resource (first source-build-targets))))
  (output build-targets g/Any (g/fnk [build-resource source-build-targets build-error ddf-message transform]
                                     (let [target (assoc (first source-build-targets)
                                                         :resource build-resource)]
                                       [(assoc target :instance-data {:resource (:resource target)
                                                                      :instance-msg ddf-message
                                                                      :transform transform})])))
  (output build-error g/Err (g/constantly nil))

  (output scene g/Any :cached (g/fnk [_node-id transform scene child-scenes]
                                     (let [aabb (reduce #(geom/aabb-union %1 (:aabb %2)) (or (:aabb scene) (geom/null-aabb)) child-scenes)
                                           aabb (geom/aabb-transform (geom/aabb-incorporate aabb 0 0 0) transform)]
                                       (-> (scene/claim-scene scene _node-id)
                                         (assoc :transform transform
                                                :aabb aabb
                                                :renderable {:passes [pass/selection]})
                                         (update :children (fn [s] (reduce conj (or s []) child-scenes)))))))
  (output go-inst-ids g/Any :cached (g/fnk [_node-id id]
                                           {id _node-id}))
  (output ddf-properties g/Any :cached (g/fnk [id ddf-component-properties] {:id id :properties ddf-component-properties})))

(g/defnode EmbeddedGOInstanceNode
  (inherits GameObjectInstanceNode)

  (display-order [:id :url scene/SceneNode])

  (input proto-msg g/Any)
  (input source-save-data g/Any)
  (output node-outline-extras g/Any (g/fnk [source-outline]
                                           {:alt-outline source-outline}))
  (output build-resource resource/Resource (g/fnk [source-resource source-save-data]
                                                  (some-> source-resource
                                                     (assoc :data (:content source-save-data))
                                                     workspace/make-build-resource)))
  (output ddf-message g/Any :cached (g/fnk [id child-ids position rotation scale proto-msg]
                                           (gen-embed-ddf id child-ids position rotation scale proto-msg))))

(defn- component-resource [comp-id basis]
  (cond
    (g/node-instance? basis game-object/ReferencedComponent comp-id)
    (some-> (g/node-value comp-id :path (g/make-evaluation-context {:basis basis}))
            :resource)

    (g/node-instance? basis game-object/ComponentNode comp-id)
    (g/node-value comp-id :source-resource (g/make-evaluation-context {:basis basis}))))

(defn- overridable-component? [basis node-id]
  (some-> node-id
    (component-resource basis)
    resource/resource-type
    :tags
    (contains? :overridable-properties)))

(defn- or-go-traverse? [basis [src-id src-label tgt-id tgt-label]]
  (or
    (overridable-component? basis src-id)
    (g/node-instance? basis resource-node/ResourceNode src-id)))

(defn- path-error [node-id resource]
  (or (validation/prop-error :fatal node-id :path validation/prop-nil? resource "Path")
      (validation/prop-error :fatal node-id :path validation/prop-resource-not-exists? resource "Path")))

(g/defnode ReferencedGOInstanceNode
  (inherits GameObjectInstanceNode)

  (property path g/Any
            (dynamic edit-type (g/fnk [source-resource]
                                      {:type resource/Resource
                                       :ext (some-> source-resource resource/resource-type :ext)
                                       :to-type (fn [v] (:resource v))
                                       :from-type (fn [r] {:resource r :overrides {}})}))
            (value (g/fnk [source-resource ddf-component-properties]
                          {:resource source-resource
                           :overrides ddf-component-properties}))
            (set (fn [evaluation-context self old-value new-value]
                   (concat
                     (if-let [old-source (g/node-value self :source-id evaluation-context)]
                       (g/delete-node old-source)
                       [])
                     (let [new-resource (:resource new-value)]
                       (let [project (project/get-project self)]
                         (project/connect-resource-node project new-resource self []
                                                        (fn [go-node]
                                                          (let [component-overrides (into {} (map (fn [m]
                                                                                                    [(:id m) (into {} (map (fn [p] [(properties/user-name->key (:id p)) [(:type p) (properties/str->go-prop (:value p) (:type p))]])
                                                                                                                           (:properties m)))])
                                                                                                  (:overrides new-value)))
                                                                override (g/override (:basis evaluation-context) go-node {:traverse? or-go-traverse?})
                                                                id-mapping (:id-mapping override)
                                                                or-node (get id-mapping go-node)
                                                                component-ids (g/node-value go-node :component-ids evaluation-context)
                                                                id-mapping (fn [id]
                                                                             (-> id
                                                                                 component-ids
                                                                                 (g/node-value :source-id evaluation-context)
                                                                                 id-mapping))]
                                                            (concat
                                                              (:tx-data override)
                                                              (let [outputs (g/output-labels (:node-type (resource/resource-type new-resource)))]
                                                                (for [[from to] [[:_node-id                 :source-id]
                                                                                 [:resource                 :source-resource]
                                                                                 [:node-outline             :source-outline]
                                                                                 [:scene                    :scene]
                                                                                 [:ddf-component-properties :ddf-component-properties]]
                                                                      :when (contains? outputs from)]
                                                                  (g/connect or-node from self to)))
                                                              (for [[from to] [[:build-targets :source-build-targets]]]
                                                                (g/connect go-node from self to))
                                                              (for [[from to] [[:url :base-url]]]
                                                                (g/connect self from or-node to))
                                                              (for [[id p] component-overrides
                                                                    [key [type value]] p
                                                                    :let [comp-id (id-mapping id)]]
                                                                (let [refd-component (component-ids id)
                                                                      refd-component-props (:properties (g/node-value refd-component :_properties evaluation-context))
                                                                      original-type (get-in refd-component-props [key :type])
                                                                      override-type (script/go-prop-type->property-types type)]
                                                                  (when (= original-type override-type)
                                                                    (g/set-property comp-id key value)))))))))))))
            (dynamic error (g/fnk [_node-id source-resource]
                                  (path-error _node-id source-resource))))

  (display-order [:id :url :path scene/SceneNode])

  (output ddf-message g/Any :cached (g/fnk [id child-ids source-resource position rotation scale ddf-component-properties]
                                           (gen-ref-ddf id child-ids position rotation scale source-resource ddf-component-properties)))
  (output build-error g/Err (g/fnk [_node-id source-resource]
                                   (path-error _node-id source-resource))))

(g/defnk produce-proto-msg [name scale-along-z ref-inst-ddf embed-inst-ddf ref-coll-ddf]
  {:name name
   :scale-along-z (if scale-along-z 1 0)
   :instances ref-inst-ddf
   :embedded-instances embed-inst-ddf
   :collection-instances ref-coll-ddf})

(defn- clean-comp-props [props]
  (mapv #(dissoc % :property-decls) props))

(defn- clean-inst-ddfs [instances]
  (mapv #(update % :component-properties clean-comp-props) instances))

(defn- clean-prop-ddfs [instances]
  (mapv #(update % :properties clean-comp-props) instances))

(defn- clean-coll-inst-ddfs [instances]
  (mapv #(update % :instance-properties clean-prop-ddfs) instances))

(g/defnk produce-save-value [resource proto-msg]
  (-> proto-msg
    (update :instances clean-inst-ddfs)
    (update :embedded-instances clean-inst-ddfs)
    (update :collection-instances clean-coll-inst-ddfs)))

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
                   :prototype (resource/proj-path resource)
                   :children (map #(str path-sep %) (:children instance-msg))
                   :position (math/vecmath->clj pos)
                   :rotation (math/vecmath->clj rot)
                   :scale3 (math/vecmath->clj scale)}))) inst-data))

(defn build-collection [resource dep-resources user-data]
  (let [{:keys [name instance-data scale-along-z]} user-data
        instance-msgs (externalize instance-data dep-resources)
        msg {:name name
             :instances instance-msgs
             :scale-along-z (if scale-along-z 1 0)}]
    {:resource resource :content (protobuf/map->bytes GameObject$CollectionDesc msg)}))

(g/defnk produce-build-targets [_node-id name resource proto-msg sub-build-targets dep-build-targets id-counts scale-along-z]
  (or (let [dup-ids (keep (fn [[id count]] (when (> count 1) id)) id-counts)]
        (when (not-empty dup-ids)
          (g/->error _node-id :build-targets :fatal nil (format "the following ids are not unique: %s" (str/join ", " dup-ids)))))
    (let [sub-build-targets (flatten sub-build-targets)
         dep-build-targets (flatten dep-build-targets)
         instance-data (map :instance-data dep-build-targets)
         instance-data (reduce concat instance-data (map #(get-in % [:user-data :instance-data]) sub-build-targets))]
     [{:node-id _node-id
       :resource (workspace/make-build-resource resource)
       :build-fn build-collection
       :user-data {:name name :instance-data instance-data :scale-along-z scale-along-z}
       :deps (vec (reduce into dep-build-targets (map :deps sub-build-targets)))}])))

(declare CollectionInstanceNode)

(g/defnk produce-coll-outline [_node-id child-outlines]
  (let [[go-outlines coll-outlines] (let [outlines (group-by #(g/node-instance? CollectionInstanceNode (:node-id %)) child-outlines)]
                                      [(get outlines false) (get outlines true)])]
    {:node-id _node-id
     :label "Collection"
     :icon collection-icon
     :children (into (outline/natural-sort coll-outlines) (outline/natural-sort go-outlines))
     :child-reqs [{:node-type ReferencedGOInstanceNode
                   :tx-attach-fn (fn [self-id child-id]
                                   (concat
                                     (g/update-property child-id :id outline/resolve-id (g/node-value self-id :ids))
                                     (attach-coll-ref-go self-id child-id)
                                     (child-coll-any self-id child-id)))}
                  {:node-type EmbeddedGOInstanceNode
                   :tx-attach-fn (fn [self-id child-id]
                                   (concat
                                     (g/update-property child-id :id outline/resolve-id (g/node-value self-id :ids))
                                     (attach-coll-embedded-go self-id child-id)
                                     (child-coll-any self-id child-id)))}
                  {:node-type CollectionInstanceNode
                   :tx-attach-fn (fn [self-id child-id]
                                   (concat
                                     (g/update-property child-id :id outline/resolve-id (g/node-value self-id :ids))
                                     (attach-coll-coll self-id child-id)
                                     (child-coll-any self-id child-id)))}]}))

(g/defnode CollectionNode
  (inherits resource-node/ResourceNode)

  (property name g/Str)
  ;; This property is legacy and purposefully hidden
  ;; The feature is only useful for uniform scaling, we use non-uniform now
  (property scale-along-z g/Bool
            (dynamic visible (g/constantly false)))

  (input ref-inst-ddf g/Any :array)
  (input embed-inst-ddf g/Any :array)
  (input ref-coll-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input ids g/Str :array)
  (input sub-build-targets g/Any :array)
  (input dep-build-targets g/Any :array)
  (input base-url g/Str)
  (input go-inst-ids g/Any :array)
  (input ddf-properties g/Any :array)

  (output base-url g/Str (gu/passthrough base-url))
  (output proto-msg g/Any :cached produce-proto-msg)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output node-outline outline/OutlineData :cached produce-coll-outline)
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :children child-scenes
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))}))
  (output go-inst-ids g/Any :cached (g/fnk [go-inst-ids] (reduce merge {} go-inst-ids)))
  (output ddf-properties g/Any (g/fnk [ddf-properties] (reduce (fn [props m]
                                                                 (if (empty? (:properties m))
                                                                   props
                                                                   (conj props m)))
                                                               [] (flatten ddf-properties))))
  (output id-counts g/Any :cached (g/fnk [ids]
                                         (reduce (fn [res id]
                                                   (update res id (fn [id] (inc (or id 0)))))
                                                 {} ids))))

(defn- merge-component-properties
  [original-properties overridden-properties]
  (let [xf (comp cat (map (juxt :id identity)))]
    (-> (into {} xf [original-properties overridden-properties])
        (vals)
        (vec))))

(defn- flatten-instance-data [data base-id ^Matrix4d base-transform all-child-ids ddf-properties]
  (let [{:keys [resource instance-msg ^Matrix4d transform]} data
        {:keys [id children component-properties]} instance-msg
        is-child? (contains? all-child-ids id)
        instance-msg {:id (str base-id id)
                      :children (map #(str base-id %) children)
                      :component-properties (merge-component-properties component-properties (ddf-properties id))}
        transform (if is-child?
                    transform
                    (doto (Matrix4d. transform) (.mul base-transform transform)))]
    {:resource resource :instance-msg instance-msg :transform transform}))

(g/defnk produce-coll-inst-build-targets [_node-id source-resource id transform build-targets ddf-properties]
  (or (path-error _node-id source-resource)
      (let [ddf-properties (into {} (map (fn [m] [(:id m) (:properties m)]) ddf-properties))
            base-id (str id path-sep)
            instance-data (get-in build-targets [0 :user-data :instance-data])
            child-ids (reduce (fn [child-ids data] (into child-ids (:children (:instance-msg data)))) #{} instance-data)]
        (assoc-in build-targets [0 :user-data :instance-data] (map #(flatten-instance-data % base-id transform child-ids ddf-properties) instance-data)))))

(g/defnk produce-coll-inst-outline [_node-id id source-resource source-outline source-id source-resource]
  (-> {:node-id _node-id
       :label id
       :icon (or (not-empty (:icon source-outline)) collection-icon)
       :children (:children source-outline)}
    (cond->
      (resource/openable-resource? source-resource)
      (assoc :link source-resource
             :outline-reference? true
             :alt-outline source-outline))))

(defn- or-coll-traverse? [basis [src-id src-label tgt-id tgt-label]]
  (or
    (overridable-component? basis src-id)
    (g/node-instance? basis resource-node/ResourceNode src-id)
    (g/node-instance? basis InstanceNode src-id)))

(g/defnode CollectionInstanceNode
  (inherits scene/SceneNode)
  (inherits InstanceNode)

  (property path g/Any
    (value (g/fnk [source-resource ddf-properties]
                  {:resource source-resource
                   :overrides ddf-properties}))
    (set (fn [evaluation-context self old-value new-value]
           (concat
             (if-let [old-source (g/node-value self :source-id evaluation-context)]
               (g/delete-node old-source)
               [])
             (let [new-resource (:resource new-value)]
               (let [project (project/get-project self)]
                 (project/connect-resource-node project new-resource self []
                                                (fn [coll-node]
                                                  (let [override (g/override (:basis evaluation-context) coll-node {:traverse? or-coll-traverse?})
                                                        id-mapping (:id-mapping override)
                                                        go-inst-ids (g/node-value coll-node :go-inst-ids evaluation-context)
                                                        component-overrides (for [{:keys [id properties]} (:overrides new-value)
                                                                                  :let [comp-ids (-> id
                                                                                                   go-inst-ids
                                                                                                   (g/node-value :source-id evaluation-context)
                                                                                                   (g/node-value :component-ids evaluation-context))]
                                                                                  :when comp-ids
                                                                                  {:keys [id properties]} properties
                                                                                  :let [comp-id (-> id
                                                                                                  comp-ids
                                                                                                  (g/node-value :source-id evaluation-context))]
                                                                                  p properties]
                                                                              [(id-mapping comp-id) (properties/user-name->key (:id p)) (properties/str->go-prop (:value p) (:type p))])
                                                        or-node (get id-mapping coll-node)]
                                                    (concat
                                                      (:tx-data override)
                                                      (let [outputs (g/output-labels (:node-type (resource/resource-type new-resource)))]
                                                        (for [[from to] [[:_node-id       :source-id]
                                                                         [:resource       :source-resource]
                                                                         [:node-outline   :source-outline]
                                                                         [:scene          :scene]
                                                                         [:ddf-properties :ddf-properties]
                                                                         [:go-inst-ids    :go-inst-ids]]
                                                              :when (contains? outputs from)]
                                                          (g/connect or-node from self to)))
                                                      (for [[from to] [[:build-targets :build-targets]]]
                                                        (g/connect coll-node from self to))
                                                      (for [[from to] [[:url :base-url]]]
                                                        (g/connect self from or-node to))
                                                      (for [[comp-id label value] component-overrides]
                                                        (g/set-property comp-id label value)))))))))))
    (dynamic error (g/fnk [_node-id source-resource]
                          (path-error _node-id source-resource)))
    (dynamic edit-type (g/fnk [source-resource]
                              {:type resource/Resource
                               :ext "collection"
                               :to-type (fn [v] (:resource v))
                               :from-type (fn [r] {:resource r :overrides {}})})))

  (display-order [:id :url :path scene/SceneNode])

  (input source-resource resource/Resource)
  (input ddf-properties g/Any :substitute [])
  (input scene g/Any)
  (input build-targets g/Any)
  (input go-inst-ids g/Any)

  (input source-outline outline/OutlineData :substitute source-outline-subst)
  (output source-outline outline/OutlineData (gu/passthrough source-outline))

  (output transform-properties g/Any scene/produce-scalable-transform-properties)
  (output node-outline outline/OutlineData :cached produce-coll-inst-outline)
  (output ddf-message g/Any :cached (g/fnk [id source-resource position rotation scale ddf-properties]
                                           {:id id
                                            :collection (resource/resource->proj-path source-resource)
                                            :position position
                                            :rotation rotation
                                            :scale3 scale
                                            :instance-properties ddf-properties}))
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                     (assoc (scene/claim-scene scene _node-id)
                                            :transform transform
                                            :aabb (geom/aabb-transform (or (:aabb scene) (geom/null-aabb)) transform)
                                            :renderable {:passes [pass/selection]})))
  (output build-targets g/Any :cached produce-coll-inst-build-targets)
  (output sub-ddf-properties g/Any :cached (g/fnk [id ddf-properties]
                                                  (map (fn [m] (update m :id (fn [s] (format "%s/%s" id s)))) ddf-properties)))
  (output go-inst-ids g/Any :cached (g/fnk [id go-inst-ids] (into {} (map (fn [[k v]] [(format "%s/%s" id k) v]) go-inst-ids)))))

(defn- gen-instance-id [coll-node base]
  (let [ids (g/node-value coll-node :ids)]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))


(defn- make-ref-go [self project source-resource id position rotation scale parent overrides]
  (let [path {:resource source-resource
              :overrides overrides}]
    (g/make-nodes (g/node-id->graph-id self)
                  [go-node [ReferencedGOInstanceNode :id id :path path :position position :rotation rotation :scale scale]]
                  (attach-coll-ref-go self go-node)
                  (if parent
                    (if (= self parent)
                      (child-coll-any self go-node)
                      (child-go-go parent go-node))
                    []))))

(defn- selection->collection [selection]
  (g/override-root (if-some [collection-instance (handler/adapt-single selection CollectionInstanceNode)]
                     (g/node-feeding-into collection-instance :source-resource)
                     (handler/adapt-single selection CollectionNode))))

(defn- selection->game-object-instance [selection]
  (g/override-root (handler/adapt-single selection GameObjectInstanceNode)))

(defn add-game-object-file [coll-node parent resource select-fn]
  (let [project (project/get-project coll-node)
        base (resource/base-name resource)
        id (gen-instance-id coll-node base)
        op-seq (gensym)
        [go-node] (g/tx-nodes-added
                    (g/transact
                      (concat
                        (g/operation-label "Add Game Object")
                        (g/operation-sequence op-seq)
                        (make-ref-go coll-node project resource id [0 0 0] [0 0 0 1] [1 1 1] parent {}))))]
    ;; Selection
    (g/transact
      (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Add Game Object")
        (select-fn [go-node])))))

(defn- select-go-file [workspace project]
  (first (dialogs/make-resource-dialog workspace project {:ext "go" :title "Select Game Object File"})))

(handler/defhandler :add-from-file :workbench
  (active? [selection] (selection->collection selection))
  (label [selection] "Add Game Object File")
  (run [workspace project app-view selection]
    (let [collection (selection->collection selection)]
      (when-let [resource (first (dialogs/make-resource-dialog workspace project {:ext "go" :title "Select Game Object File"}))]
        (add-game-object-file collection collection resource (fn [node-ids] (app-view/select app-view node-ids)))))))

(defn- make-embedded-go [self project type data id position rotation scale parent select-fn]
  (let [graph (g/node-id->graph-id self)
        resource (project/make-embedded-resource project type data)]
    (g/make-nodes graph [go-node [EmbeddedGOInstanceNode :id id :position position :rotation rotation :scale scale]]
      (if select-fn
        (select-fn [go-node])
        [])
      (let [tx-data (project/make-resource-node graph project resource true
                                                {go-node [[:_node-id :source-id]
                                                          [:resource :source-resource]
                                                          [:node-outline :source-outline]
                                                          [:proto-msg :proto-msg]
                                                          [:save-data :source-save-data]
                                                          [:build-targets :source-build-targets]
                                                          [:scene :scene]
                                                          [:ddf-component-properties :ddf-component-properties]]
                                                 self [[:_node-id :nodes]]}
                                                (fn [n] (g/connect go-node :url n :base-url)))]
        (concat
          tx-data
          (if (empty? tx-data)
            []
            (concat
              (attach-coll-embedded-go self go-node)
              (if parent
                (if (= parent self)
                  (child-coll-any self go-node)
                  (child-go-go parent go-node))
                []))))))))

(defn add-game-object [workspace project coll-node parent select-fn]
  (let [ext           "go"
        resource-type (workspace/get-resource-type workspace ext)
        template      (workspace/template resource-type)
        id (gen-instance-id coll-node ext)]
    (g/transact
      (concat
        (g/operation-label "Add Game Object")
        (make-embedded-go coll-node project ext template id [0 0 0] [0 0 0 1] [1 1 1] parent select-fn)))))

(handler/defhandler :add :workbench
  (active? [selection] (selection->collection selection))
  (label [selection user-data] "Add Game Object")
  (run [selection workspace project user-data app-view]
    (let [collection (selection->collection selection)]
      (add-game-object workspace project collection collection (fn [node-ids] (app-view/select app-view node-ids))))))

(defn add-collection-instance [self source-resource id position rotation scale overrides]
  (let [project (project/get-project self)]
    (g/make-nodes (g/node-id->graph-id self)
                  [coll-node [CollectionInstanceNode :id id :path {:resource source-resource :overrides overrides}
                              :position position :rotation rotation :scale scale]]
                  (attach-coll-coll self coll-node)
                  (child-coll-any self coll-node))))

(handler/defhandler :add-secondary :workbench
  (active? [selection] (selection->game-object-instance selection))
  (label [] "Add Game Object")
  (run [selection project workspace app-view]
       (let [go-node (selection->game-object-instance selection)
             collection (core/scope-of-type go-node CollectionNode)]
         (add-game-object workspace project collection go-node (fn [node-ids] (app-view/select app-view node-ids))))))

(handler/defhandler :add-secondary-from-file :workbench
  (active? [selection] (or (selection->collection selection)
                         (selection->game-object-instance selection)))
  (label [selection] (if (selection->collection selection)
                       "Add Collection File"
                       "Add Game Object File"))
  (run [selection workspace project app-view]
       (if-let [coll-node (selection->collection selection)]
         (let [ext           "collection"
               resource-type (workspace/get-resource-type workspace ext)]
           (when-let [resource (first (dialogs/make-resource-dialog workspace project {:ext ext :title "Select Collection File"}))]
             (let [base (resource/base-name resource)
                   id (gen-instance-id coll-node base)
                   op-seq (gensym)
                   [coll-inst-node] (g/tx-nodes-added
                                      (g/transact
                                        (concat
                                          (g/operation-label "Add Collection")
                                          (g/operation-sequence op-seq)
                                          (add-collection-instance coll-node resource id [0 0 0] [0 0 0 1] [1 1 1] []))))]
               ; Selection
               (g/transact
                 (concat
                   (g/operation-sequence op-seq)
                   (g/operation-label "Add Collection")
                   (app-view/select app-view [coll-inst-node]))))))
         (when-let [resource (select-go-file workspace project)]
           (let [go-node (selection->game-object-instance selection)
                 coll-node (core/scope-of-type go-node CollectionNode)]
             (add-game-object-file coll-node go-node resource (fn [node-ids] (app-view/select app-view node-ids))))))))

(defn load-collection [project self resource collection]
  (concat
    (g/set-property self :name (:name collection))
    (g/set-property self :scale-along-z (not= 0 (:scale-along-z collection)))
    (let [tx-go-creation (flatten
                           (concat
                             (for [game-object (:instances collection)
                                   :let [source-resource (workspace/resolve-resource resource (:prototype game-object))]]
                               (make-ref-go self project source-resource (:id game-object) (:position game-object)
                                            (:rotation game-object) (:scale3 game-object) nil (:component-properties game-object)))
                             (for [embedded (:embedded-instances collection)]
                               (make-embedded-go self project "go" (:data embedded) (:id embedded)
                                                 (:position embedded)
                                                 (:rotation embedded)
                                                 (:scale3 embedded)
                                                 nil nil))))
          new-instance-data (filter #(and (= :create-node (:type %)) (g/node-instance*? GameObjectInstanceNode (:node %))) tx-go-creation)
          id->nid (into {} (map #(do [(get-in % [:node :id]) (g/node-id (:node %))]) new-instance-data))
          child->parent (into {} (map #(do [% nil]) (keys id->nid)))
          rev-child-parent-fn (fn [instances] (into {} (mapcat (fn [inst] (map #(do [% (:id inst)]) (:children inst))) instances)))
          child->parent (merge child->parent (rev-child-parent-fn (concat (:instances collection) (:embedded-instances collection))))]
      (concat
        tx-go-creation
        (for [[child parent] child->parent
              :let [child-id (id->nid child)
                    parent-id (if parent (id->nid parent) self)]]
          (if parent
            (child-go-go parent-id child-id)
            (child-coll-any self child-id)))))
    (for [coll-instance (:collection-instances collection)
          :let [source-resource (workspace/resolve-resource resource (:collection coll-instance))]]
      (add-collection-instance self source-resource (:id coll-instance) (:position coll-instance)
                               (:rotation coll-instance) (:scale3 coll-instance) (:instance-properties coll-instance)))))

(defn- read-scale3-or-scale
  [{:keys [scale3 scale] :as pb-map}]
  ;; scale is the legacy uniform scale
  ;; check if scale3 has default value and if so, use legacy uniform scale
  (if (and (= scale3 [0.0 0.0 0.0]) (some? scale) (not= scale 0.0))
    [scale scale scale]
    scale3))

(defn- uniform->non-uniform-scale [v]
  (-> v
    (assoc :scale3 (read-scale3-or-scale v))
    (dissoc :scale)))

(defn- sanitize-instances [is]
  (mapv uniform->non-uniform-scale is))

(defn- sanitize-collection [c]
  (reduce (fn [c f] (update c f sanitize-instances)) c [:instances :embedded-instances :collection-instances]))

(defn- make-dependencies-fn [workspace]
  (let [default-dependencies-fn (resource-node/make-ddf-dependencies-fn GameObject$CollectionDesc)]
    (fn [source-value]
      (let [go-resource-type (workspace/get-resource-type workspace "go")
            read-go (:read-fn go-resource-type)
            go-dependencies-fn (:dependencies-fn go-resource-type)
            embedded-instances (:embedded-instances source-value)]
        (into (default-dependencies-fn source-value)
              (mapcat #(try
                         (go-dependencies-fn (read-go (StringReader. (:data %))))
                         (catch Exception e
                           (log/warn :msg (format "Couldn't determine dependencies for embedded instance %s" (:id %))
                                     :exception e)
                           [])))
              embedded-instances)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "collection"
                                    :label "Collection"
                                    :node-type CollectionNode
                                    :ddf-type GameObject$CollectionDesc
                                    :load-fn load-collection
                                    :dependencies-fn (make-dependencies-fn workspace)
                                    :sanitize-fn sanitize-collection
                                    :icon collection-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}))

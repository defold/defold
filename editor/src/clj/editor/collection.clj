(ns editor.collection
  (:require [clojure.java.io :as io]
            [editor.core :as core]
            [schema.core :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.dialogs :as dialogs]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.outline :as outline]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [clojure.string :as str])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc GameObject$CollectionDesc]
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

(set! *warn-on-reflection* true)

(def collection-icon "icons/32/Icons_09-Collection.png")
(def path-sep "/")

(defn- comp-overrides->ddf [o]
  (reduce-kv (fn [r k v]
               (if (empty? v)
                 r
                 (conj r {:id k :properties (mapv (fn [[_ v]] (second v)) v)})))
             [] o))

(defn- strip-properties [v]
  (mapv #(dissoc % :properties) v))

(defn- gen-embed-ddf [id child-ids position ^Quat4d rotation-q4 scale proto-msg ddf-component-properties]
  (let [proto-msg (-> proto-msg
                    (update :components strip-properties))]
    {:id id
     :children child-ids
     :data (protobuf/map->str GameObject$PrototypeDesc proto-msg)
     :position position
     :rotation (math/vecmath->clj rotation-q4)
     :scale3 scale
     :component-properties ddf-component-properties}))

(defn- gen-ref-ddf [id child-ids position ^Quat4d rotation-q4 scale path ddf-component-properties]
  {:id id
   :children child-ids
   :prototype (resource/resource->proj-path path)
   :position position
   :rotation (math/vecmath->clj rotation-q4)
   :scale3 scale
   :component-properties ddf-component-properties})

(defn- assoc-deep [scene keyword new-value]
  (let [new-scene (assoc scene keyword new-value)]
    (if (:children scene)
      (assoc new-scene :children (map #(assoc-deep % keyword new-value) (:children scene)))
      new-scene)))

(defn- label-sort-by-fn [v]
  (when-let [label (:label v)] (str/lower-case label)))

(g/defnode InstanceNode
  (inherits outline/OutlineNode)
  (property id g/Str)
  (property url g/Str
            (value (g/fnk [base-url id] (format "%s/%s" (or base-url "") id)))
            (dynamic read-only? (g/always true)))
  (input base-url g/Str)
  (input source-id g/Any :cascade-delete))

(defn- child-go-go [go-id child-id]
  (for [[from to] [[:id :child-ids]
                   [:node-outline :child-outlines]
                   [:scene :child-scenes]]]
    (g/connect child-id from go-id to)))

(defn- child-coll-any [coll-id child-id]
  (for [[from to] [[:node-outline :child-outlines]
                   [:scene :child-scenes]]]
    (g/connect child-id from coll-id to)))

(defn- attach-coll-go [coll-id child-id ddf-label]
  (concat
    (for [[from to] [[:_node-id :nodes]
                     [:ddf-message ddf-label]
                     [:build-targets :dep-build-targets]
                     [:id :ids]
                     [:go-inst-ids :go-inst-ids]
                     [:ddf-properties :ddf-properties]]]
      (g/connect child-id from coll-id to))
    (for [[from to] [[:base-url :base-url]]]
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
    (for [[from to] [[:base-url :base-url]]]
      (g/connect coll-id from child-id to))))

(def EmbeddedGOInstanceNode nil)
(def ReferencedGOInstanceNode nil)

(defn- go-id->node-ids [go-id]
  (let [collection (core/scope go-id)]
    (g/node-value collection :ids)))

(g/defnk produce-go-outline [_node-id id node-outline-label source-outline child-outlines]
  (let [coll-id (core/scope _node-id)]
    (-> source-outline
      (merge {:node-id _node-id
              :label node-outline-label
              :icon game-object/game-object-icon
              :children (into (vec (sort-by label-sort-by-fn (:children source-outline))) child-outlines)})
      (update :child-reqs (fn [c] (conj (if c c [])
                                        {:node-type ReferencedGOInstanceNode
                                         :tx-attach-fn (fn [self-id child-id]
                                                         (concat
                                                           (g/update-property child-id :id outline/resolve-id (go-id->node-ids self-id))
                                                           (attach-coll-ref-go coll-id child-id)
                                                           (child-go-go self-id child-id)))}
                                        {:node-type EmbeddedGOInstanceNode
                                         :tx-attach-fn (fn [self-id child-id]
                                                         (concat
                                                           (g/update-property child-id :id outline/resolve-id (go-id->node-ids self-id))
                                                           (attach-coll-embedded-go coll-id child-id)
                                                           (child-go-go self-id child-id)))}))))))

(defn- source-outline-subst [err]
  ;; TODO: embed error so can warn in outline
  ;; outline content not really used, only children if any.
  {:node-id 0
   :icon ""
   :label ""})

(g/defnode GameObjectInstanceNode
  (inherits scene/ScalableSceneNode)
  (inherits InstanceNode)

  (input properties g/Any)
  (input build-targets g/Any)
  (input scene g/Any)
  (input child-scenes g/Any :array)
  (input child-ids g/Str :array)

  (input ddf-component-properties g/Any :substitute [])
  (input source-outline outline/OutlineData :substitute source-outline-subst)
  (output source-outline outline/OutlineData (gu/passthrough source-outline))

  (output node-outline outline/OutlineData :cached produce-go-outline)
  (output ddf-message g/Any :abstract)
  (output node-outline-label g/Str (gu/passthrough id))
  (output build-targets g/Any (g/fnk [build-targets ddf-message transform] (let [target (first build-targets)]
                                                                             [(assoc target :instance-data {:resource (:resource target)
                                                                                                            :instance-msg ddf-message
                                                                                                            :transform transform})])))

  (output scene g/Any :cached (g/fnk [_node-id transform scene child-scenes]
                                     (let [aabb (reduce #(geom/aabb-union %1 (:aabb %2)) (or (:aabb scene) (geom/null-aabb)) child-scenes)
                                           aabb (geom/aabb-transform (geom/aabb-incorporate aabb 0 0 0) transform)]
                                       (merge-with concat
                                                   (assoc (assoc-deep scene :node-id _node-id)
                                                          :transform transform
                                                          :aabb aabb
                                                          :renderable {:passes [pass/selection]})
                                                   {:children child-scenes}))))
  (output go-inst-ids g/Any :cached (g/fnk [_node-id id]
                                           {id _node-id}))
  (output ddf-properties g/Any :cached (g/fnk [id ddf-component-properties] {:id id :properties ddf-component-properties})))

(g/defnode EmbeddedGOInstanceNode
  (inherits GameObjectInstanceNode)

  (property overrides g/Any
              (dynamic visible (g/always false))
              (value (gu/passthrough ddf-component-properties))
              (set (fn [basis self old-value new-value]
                     (let [go (g/node-value self :source-id :basis basis)
                           component-ids (g/node-value go :component-ids :basis basis)]
                       (for [{:keys [id properties]} new-value
                             p properties
                             :let [src-id (-> id
                                            component-ids
                                            (g/node-value :source-id :basis basis))
                                   key (properties/user-name->key (:id p))
                                   val (properties/str->go-prop (:value p) (:type p))]
                             :when src-id]
                         (g/set-property src-id key val))))))
  (display-order [:id :url scene/ScalableSceneNode])

  (input proto-msg g/Any)

  (output ddf-message g/Any :cached (g/fnk [id child-ids position ^Quat4d rotation-q4 scale proto-msg ddf-component-properties]
                                           (gen-embed-ddf id child-ids position rotation-q4 scale proto-msg ddf-component-properties))))

(defn- component-resource [comp-id basis]
  (cond
    (g/node-instance? basis game-object/ReferencedComponent comp-id)
    (some-> (g/node-value comp-id :path :basis basis)
      :resource)

    (g/node-instance? basis game-object/ComponentNode comp-id)
    (g/node-value comp-id :source-resource :basis basis)))

(defn- overridable-component? [basis node-id]
  (some-> node-id
    (component-resource basis)
    resource/resource-type
    :tags
    (contains? :overridable-properties)))

(defn- or-go-traverse? [basis [src-id src-label tgt-id tgt-label]]
  (or
    (overridable-component? basis src-id)
    (g/node-instance? basis project/ResourceNode src-id)))

(g/defnode ReferencedGOInstanceNode
  (inherits GameObjectInstanceNode)

  (property path g/Any
            (dynamic edit-type (g/fnk [source-resource]
                                      {:type resource/resource-edit-type
                                       :ext (some-> source-resource resource/resource-type :ext)
                                       :to-type (fn [v] (:resource v))
                                       :from-type (fn [r] {:resource r :overrides {}})}))
            (value (g/fnk [source-resource ddf-component-properties]
                          {:resource source-resource
                           :overrides ddf-component-properties}))
            (set (fn [basis self old-value new-value]
                   (concat
                     (if-let [old-source (g/node-value self :source-id :basis basis)]
                       (g/delete-node old-source)
                       [])
                     (let [new-resource (:resource new-value)]
                       (let [project (project/get-project self)]
                         (project/connect-resource-node project new-resource self []
                                                        (fn [go-node]
                                                          (let [component-overrides (into {} (map (fn [m]
                                                                                                    [(:id m) (into {} (map (fn [p] [(properties/user-name->key (:id p)) (properties/str->go-prop (:value p) (:type p))])
                                                                                                                           (:properties m)))])
                                                                                                  (:overrides new-value)))
                                                                override (g/override basis go-node {:traverse? or-go-traverse?})
                                                                id-mapping (:id-mapping override)
                                                                or-node (get id-mapping go-node)
                                                                component-ids (g/node-value go-node :component-ids :basis basis)
                                                                id-mapping (fn [id]
                                                                             (-> id
                                                                               component-ids
                                                                               (g/node-value :source-id :basis basis)
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
                                                              (for [[from to] [[:build-targets :build-targets]]]
                                                                (g/connect go-node from self to))
                                                              (for [[from to] [[:url :base-url]]]
                                                                (g/connect self from or-node to))
                                                              (for [[id p] component-overrides
                                                                    [label value] p
                                                                    :let [comp-id (id-mapping id)]]
                                                                (g/set-property comp-id label value)))))))))))
            (validate (g/fnk [path]
                             (when (nil? path)
                               (g/error-warning "Missing prototype")))))

  (display-order [:id :url :path scene/ScalableSceneNode])

  (input source-resource resource/Resource)
  (output node-outline-label g/Str (g/fnk [id source-resource] (format "%s (%s)" id (resource/resource->proj-path source-resource))))
  (output ddf-message g/Any :cached (g/fnk [id child-ids source-resource position ^Quat4d rotation-q4 scale ddf-component-properties]
                                           (gen-ref-ddf id child-ids position rotation-q4 scale source-resource ddf-component-properties))))

(g/defnk produce-proto-msg [name ref-inst-ddf embed-inst-ddf ref-coll-ddf]
  {:name name
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

(g/defnk produce-save-data [resource proto-msg]
  (let [msg (-> proto-msg
              (update :instances clean-inst-ddfs)
              (update :embedded-instances clean-inst-ddfs)
              (update :collection-instances clean-coll-inst-ddfs))]
    {:resource resource
     :content (protobuf/map->str GameObject$CollectionDesc msg)}))

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

(def CollectionInstanceNode nil)

(defn- outline-sort-by-fn [v]
  [(not (g/node-instance? CollectionInstanceNode (:node-id v))) (label-sort-by-fn v)])

(g/defnk produce-coll-outline [_node-id child-outlines]
  {:node-id _node-id
   :label "Collection"
   :icon collection-icon
   :children (vec (sort-by outline-sort-by-fn child-outlines))
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
                                   (child-coll-any self-id child-id)))}]})

(g/defnode CollectionNode
  (inherits project/ResourceNode)

  (property name g/Str)

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
  (output save-data g/Any :cached produce-save-data)
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
                                                               [] (flatten ddf-properties)))))

(defn- flatten-instance-data [data base-id ^Matrix4d base-transform all-child-ids ddf-properties]
  (let [{:keys [resource instance-msg ^Matrix4d transform]} data
        is-child? (contains? all-child-ids (:id instance-msg))
        instance-msg {:id (str base-id (:id instance-msg))
                      :children (map #(str base-id %) (:children instance-msg))
                      :component-properties (ddf-properties (:id instance-msg))}
        transform (if is-child?
                    transform
                    (doto (Matrix4d. transform) (.mul base-transform transform)))]
    {:resource resource :instance-msg instance-msg :transform transform}))

(g/defnk produce-coll-inst-build-targets [id transform build-targets ddf-properties]
  (let [ddf-properties (into {} (map (fn [m] [(:id m) (:properties m)]) ddf-properties))
        base-id (str id path-sep)
        instance-data (get-in build-targets [0 :user-data :instance-data])
        child-ids (reduce (fn [child-ids data] (into child-ids (:children (:instance-msg data)))) #{} instance-data)]
    (assoc-in build-targets [0 :user-data :instance-data] (map #(flatten-instance-data % base-id transform child-ids ddf-properties) instance-data))))

(g/defnk produce-coll-inst-outline [_node-id id source-resource source-outline source-id]
  (-> source-outline
    (assoc :node-id _node-id
      :label (format "%s (%s)" id (resource/resource->proj-path source-resource))
      :icon collection-icon)
    (update :child-reqs (fn [reqs]
                          (mapv (fn [req] (update req :tx-attach-fn #(fn [self-id child-id]
                                                                       (% source-id child-id))))
                            ; TODO - temp blocked because of risk for graph cycles
                            ; If it's dropped on another instance referencing the same collection, it blows up
                            (filter (fn [req] (not= CollectionInstanceNode (:node-type req))) reqs))))))

(defn- or-coll-traverse? [basis [src-id src-label tgt-id tgt-label]]
  (or
    (overridable-component? basis src-id)
    (g/node-instance? basis project/ResourceNode src-id)
    (g/node-instance? basis InstanceNode src-id)))

(g/defnode CollectionInstanceNode
  (inherits scene/ScalableSceneNode)
  (inherits InstanceNode)

  (property path g/Any
    (value (g/fnk [source-resource ddf-properties]
                  {:resource source-resource
                   :overrides ddf-properties}))
    (set (fn [basis self old-value new-value]
           (concat
             (if-let [old-source (g/node-value self :source-id :basis basis)]
               (g/delete-node old-source)
               [])
             (let [new-resource (:resource new-value)]
               (let [project (project/get-project self)]
                 (project/connect-resource-node project new-resource self []
                                                (fn [coll-node]
                                                  (let [override (g/override basis coll-node {:traverse? or-coll-traverse?})
                                                        id-mapping (:id-mapping override)
                                                        go-inst-ids (g/node-value coll-node :go-inst-ids :basis basis)
                                                        component-overrides (for [{:keys [id properties]} (:overrides new-value)
                                                                                  :let [comp-ids (-> id
                                                                                                   go-inst-ids
                                                                                                   (g/node-value :source-id :basis basis)
                                                                                                   (g/node-value :component-ids :basis basis))]
                                                                                  :when comp-ids
                                                                                  {:keys [id properties]} properties
                                                                                  :let [comp-id (-> id
                                                                                                  comp-ids
                                                                                                  (g/node-value :source-id :basis basis))]
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
    (validate (validation/validate-resource path "Missing prototype" [scene])))

  (display-order [:id :url :path scene/ScalableSceneNode])

  (input source-resource resource/Resource)
  (input ddf-properties g/Any :substitute [])
  (input scene g/Any)
  (input build-targets g/Any)
  (input go-inst-ids g/Any)

  (input source-outline outline/OutlineData :substitute source-outline-subst)
  (output source-outline outline/OutlineData (gu/passthrough source-outline))

  (output node-outline outline/OutlineData :cached produce-coll-inst-outline)
  (output ddf-message g/Any :cached (g/fnk [id source-resource position ^Quat4d rotation-q4 scale ddf-properties]
                                           {:id id
                                            :collection (resource/resource->proj-path source-resource)
                                            :position position
                                            :rotation (math/vecmath->clj rotation-q4)
                                            :scale3 scale
                                            :instance-properties ddf-properties}))
  (output scene g/Any :cached (g/fnk [_node-id transform scene]
                                     (assoc (assoc-deep scene :node-id _node-id)
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


(defn- make-ref-go [self project source-resource id position rotation scale child? overrides]
  (let [path {:resource source-resource
              :overrides overrides}]
    (g/make-nodes (g/node-id->graph-id self)
                  [go-node [ReferencedGOInstanceNode :id id :path path :position position :rotation rotation :scale scale]]
                  (attach-coll-ref-go self go-node)
                  (if child?
                    (child-coll-any self go-node)
                    []))))

(defn- single-selection? [selection]
  (= 1 (count selection)))

(defn- selected-collection? [selection]
  (g/node-instance? CollectionNode (first selection)))

(defn- selected-embedded-instance? [selection]
  (let [node (first selection)]
    (g/node-instance? EmbeddedGOInstanceNode node)))

(defn add-game-object-file [coll-node resource]
  (let [project (project/get-project coll-node)
        base (FilenameUtils/getBaseName (resource/resource-name resource))
        id (gen-instance-id coll-node base)
        op-seq (gensym)
        [go-node] (g/tx-nodes-added
                    (g/transact
                      (concat
                        (g/operation-label "Add Game Object")
                        (g/operation-sequence op-seq)
                        (make-ref-go coll-node project resource id [0 0 0] [0 0 0] [1 1 1] true {}))))]
    ;; Selection
    (g/transact
      (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Add Game Object")
        (project/select project [go-node])))))

(handler/defhandler :add-from-file :global
  (active? [selection] (and (single-selection? selection)
                            (or (selected-collection? selection)
                                (selected-embedded-instance? selection))))
  (label [selection] (cond
                       (selected-collection? selection) "Add Game Object File"
                       (selected-embedded-instance? selection) "Add Component File"))
  (run [workspace selection]
       (cond
         (selected-collection? selection) (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext "go" :title "Select Game Object File"}))]
                                            (let [coll-node (first selection)]
                                              (add-game-object-file coll-node resource)))
         (selected-embedded-instance? selection) (game-object/add-component-handler workspace (g/node-value (first selection) :source-id)))))

(defn- make-embedded-go [self project type data id position rotation scale child? select? overrides]
  (let [graph (g/node-id->graph-id self)
        resource (project/make-embedded-resource project type data)]
    (g/make-nodes graph [go-node [EmbeddedGOInstanceNode :id id :position position :rotation rotation :scale scale]]
      (if select?
        (project/select project [go-node])
        [])
      (let [tx-data (project/make-resource-node graph project resource true
                                                {go-node [[:_node-id :source-id]
                                                          [:node-outline :source-outline]
                                                          [:proto-msg :proto-msg]
                                                          [:build-targets :build-targets]
                                                          [:scene :scene]
                                                          [:ddf-component-properties :ddf-component-properties]]
                                                 self [[:_node-id :nodes]]}
                                                (fn [n] (g/connect go-node :url n :base-url)))]
        (concat
          tx-data
          (if (empty? tx-data)
            []
            (concat
              (g/set-property go-node :overrides overrides)
              (attach-coll-embedded-go self go-node)
              (if child?
                (child-coll-any self go-node)
                []))))))))

(defn- add-game-object [selection]
  (let [coll-node     (first selection)
        project       (project/get-project coll-node)
        workspace     (:workspace (g/node-value coll-node :resource))
        ext           "go"
        resource-type (workspace/get-resource-type workspace ext)
        template      (workspace/template resource-type)
        id (gen-instance-id coll-node ext)]
    (g/transact
      (concat
        (g/operation-label "Add Game Object")
        (make-embedded-go coll-node project ext template id [0 0 0] [0 0 0] [1 1 1] true true {})))))

(handler/defhandler :add :global
  (active? [selection] (and (single-selection? selection)
                            (or (selected-collection? selection)
                                (selected-embedded-instance? selection))))
  (label [selection user-data] (cond
                                 (selected-collection? selection) "Add Game Object"
                                 (selected-embedded-instance? selection) (game-object/add-embedded-component-label user-data)))
  (options [selection user-data] (when (selected-embedded-instance? selection)
                                   (let [source (g/node-value (first selection) :source-id)
                                         workspace (:workspace (g/node-value source :resource))]
                                     (game-object/add-embedded-component-options source workspace user-data))))
  (run [selection user-data] (cond
                               (selected-collection? selection) (add-game-object selection)
                               (selected-embedded-instance? selection) (game-object/add-embedded-component-handler user-data))))

(defn- add-collection-instance [self source-resource id position rotation scale overrides]
  (let [project (project/get-project self)]
    (g/make-nodes (g/node-id->graph-id self)
                  [coll-node [CollectionInstanceNode :id id :path {:resource source-resource :overrides overrides}
                              :position position :rotation rotation :scale scale]]
                  (attach-coll-coll self coll-node)
                  (child-coll-any self coll-node))))

(handler/defhandler :add-secondary-from-file :global
  (active? [selection] (and (single-selection? selection) (selected-collection? selection)))
  (label [] "Add Collection File")
  (run [selection] (let [coll-node     (first selection)
                         project       (project/get-project coll-node)
                         workspace     (:workspace (g/node-value coll-node :resource))
                         ext           "collection"
                         resource-type (workspace/get-resource-type workspace ext)]
                     (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext ext :title "Select Collection File"}))]
                       (let [base (FilenameUtils/getBaseName (resource/resource-name resource))
                             id (gen-instance-id coll-node base)
                             op-seq (gensym)
                             [coll-inst-node] (g/tx-nodes-added
                                               (g/transact
                                                (concat
                                                 (g/operation-label "Add Collection")
                                                 (g/operation-sequence op-seq)
                                                 (add-collection-instance coll-node resource id [0 0 0] [0 0 0] [1 1 1] []))))]
                                        ; Selection
                         (g/transact
                          (concat
                           (g/operation-sequence op-seq)
                           (g/operation-label "Add Collection")
                           (project/select project [coll-inst-node]))))))))

(defn- v4->euler [v]
  (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v))))

(defn load-collection [project self resource]
  (let [collection (protobuf/read-text GameObject$CollectionDesc resource)
        project-graph (g/node-id->graph-id project)
        prototype-resources (concat
                              (map :prototype (:instances collection))
                              (map :collection (:collection-instances collection)))
        prototype-load-data  (project/load-resource-nodes project
                                                          (->> prototype-resources
                                                            (map #(project/get-resource-node project %))
                                                            (remove nil?))
                                                          progress/null-render-progress!)]
    (concat
      prototype-load-data
      (g/set-property self :name (:name collection))
      (let [tx-go-creation (flatten
                             (concat
                               (for [game-object (:instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale game-object)
                                           source-resource (workspace/resolve-resource resource (:prototype game-object))]]
                                 (make-ref-go self project source-resource (:id game-object) (:position game-object)
                                   (v4->euler (:rotation game-object)) [scale scale scale] false (:component-properties game-object)))
                               (for [embedded (:embedded-instances collection)
                                     :let [; TODO - fix non-uniform hax
                                           scale (:scale embedded)]]
                                 (make-embedded-go self project "go" (:data embedded) (:id embedded)
                                   (:position embedded)
                                   (v4->euler (:rotation embedded))
                                   [scale scale scale] false false (:component-properties embedded)))))
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
            :let [; TODO - fix non-uniform hax
                  scale (:scale coll-instance)
                  source-resource (workspace/resolve-resource resource (:collection coll-instance))]]
        (add-collection-instance self source-resource (:id coll-instance) (:position coll-instance)
          (v4->euler (:rotation coll-instance)) [scale scale scale] (:instance-properties coll-instance))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "collection"
                                    :label "Collection"
                                    :node-type CollectionNode
                                    :load-fn load-collection
                                    :icon collection-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}))

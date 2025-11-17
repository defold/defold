;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.model
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.gl.texture :as texture]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.image :as image]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.model-scene :as model-scene]
            [editor.pipeline :as pipeline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll])
  (:import [com.dynamo.gamesys.proto ModelProto$Material ModelProto$Model ModelProto$ModelDesc ModelProto$Texture]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(def ^:private supported-image-exts (conj image/exts "cubemap" "render_target"))

(g/defnk produce-animation-set-build-target-single [_node-id resource animations-resource animation-set]
  (let [is-single-anim (and (not (empty? animation-set))
                            (not (animation-set/is-animation-set? animations-resource)))]
    (when is-single-anim
      (rig/make-animation-set-build-target (resource/workspace resource) _node-id animation-set))))

(g/defnk produce-animation-ids [_node-id animations-resource animation-set-info animation-set animation-ids]
  (let [is-single-anim (or (empty? animation-set)
                           (not (animation-set/is-animation-set? animations-resource)))]
    (if is-single-anim
      (if animations-resource
        animation-ids
        [])
      (:animation-ids animation-set-info))))

(g/defnk produce-pb-msg [name mesh materials skeleton animations default-animation create-go-bones]
  (protobuf/make-map-without-defaults ModelProto$ModelDesc
    :mesh (resource/resource->proj-path mesh)
    :materials (mapv
                 (fn [material]
                   (-> material
                       (update :material resource/resource->proj-path) ; Required protobuf field.
                       (protobuf/sanitize-repeated :textures #(update % :texture resource/resource->proj-path))))
                 materials)
    :skeleton (resource/resource->proj-path skeleton)
    :animations (resource/resource->proj-path animations)
    :default-animation default-animation
    :name name
    :create-go-bones create-go-bones))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- validate-default-animation [_node-id default-animation animation-ids]
  (when (not (str/blank? default-animation))
    (validation/prop-error :fatal _node-id :default-animation validation/prop-member-of? default-animation (set animation-ids)
                           (format "Animation '%s' does not exist" default-animation))))

(defn- update-build-target-vertex-attributes [pb-msg material-binding-infos]
  (let [materials+attribute-build-data (mapv (fn [material+binding-infos]
                                               (let [material (first material+binding-infos)
                                                     material-binding-info (second material+binding-infos)
                                                     material-attributes (graphics/vertex-attribute-overrides->build-target
                                                                           (:vertex-attribute-overrides material-binding-info)
                                                                           (:vertex-attribute-bytes material-binding-info)
                                                                           (:material-attribute-infos material-binding-info))]
                                                 (protobuf/assign-repeated material :attributes material-attributes)))
                                             (map vector (:materials pb-msg) material-binding-infos))]
    (protobuf/assign-repeated pb-msg :materials materials+attribute-build-data)))

(g/defnk produce-save-value [pb-msg materials material-binding-infos]
  (protobuf/assign-repeated pb-msg
    :materials
    (mapv (fn [material material-binding-info]
            (let [material-attribute-infos (:material-attribute-infos material-binding-info)
                  vertex-attribute-overrides (:vertex-attribute-overrides material-binding-info)
                  vertex-attribute-save-values (graphics/vertex-attribute-overrides->save-values vertex-attribute-overrides material-attribute-infos)]
              (-> material
                  (update :material resource/resource->proj-path) ; Required protobuf field.
                  (protobuf/sanitize-repeated :textures #(update % :texture resource/resource->proj-path))
                  (protobuf/assign-repeated :attributes vertex-attribute-save-values))))
          materials
          material-binding-infos)))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets default-animation animation-ids animation-set-build-target animation-set-build-target-single mesh-set-build-target materials material-binding-infos skeleton-build-target animations mesh skeleton create-go-bones]
  (or (some->> (into [(prop-resource-error :fatal _node-id :mesh mesh "Mesh")
                      (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")
                      (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations "Animations")
                      (validate-default-animation _node-id default-animation animation-ids)
                      (validation/prop-error :fatal _node-id :materials validation/prop-empty? (:materials pb-msg) "Materials")]
                     (map (fn [{:keys [name material]}]
                            (validation/prop-error
                              :fatal _node-id
                              :materials validation/prop-resource-missing?
                              material name)))
                     materials)
               (filterv identity)
               not-empty
               g/error-aggregate)
      (let [workspace (resource/workspace resource)
            animation-set-build-target (if (nil? animation-set-build-target-single) animation-set-build-target animation-set-build-target-single)
            rig-scene-dep-build-targets {:animation-set animation-set-build-target
                                         :mesh-set mesh-set-build-target
                                         :skeleton skeleton-build-target}
            rig-scene-pb-msg {}
            rig-scene-build-target (rig/make-rig-scene-build-target workspace _node-id rig-scene-pb-msg dep-build-targets rig-scene-dep-build-targets)
            rt-pb-msg (-> {:rig-scene (:resource rig-scene-build-target)
                           :default-animation default-animation
                           :materials (:materials pb-msg)
                           :create-go-bones create-go-bones}
                          (update-build-target-vertex-attributes material-binding-infos))
            dep-build-targets (into [rig-scene-build-target] (flatten dep-build-targets))]
        [(pipeline/make-protobuf-build-target _node-id resource ModelProto$Model rt-pb-msg dep-build-targets)])))

(g/defnk produce-gpu-textures [_node-id samplers texture-binding-infos :as m]
  (let [sampler-name->gpu-texture-generator (into {}
                                                  (keep (fn [{:keys [sampler gpu-texture-generator]}]
                                                          (when gpu-texture-generator
                                                            [sampler gpu-texture-generator])))
                                                  texture-binding-infos)
        explicit-textures (into {}
                                (keep-indexed
                                  (fn [unit-index {:keys [name] :as sampler}]
                                    (when-let [{tex-fn :f tex-args :args} (sampler-name->gpu-texture-generator name)]
                                      (let [request-id [_node-id unit-index]
                                            params (material/sampler->tex-params sampler)
                                            texture (tex-fn tex-args request-id params unit-index)]
                                        [name texture]))))
                                samplers)
        fallback-texture (if (pos? (count explicit-textures))
                           (val (first explicit-textures))
                           @texture/black-pixel)]
    (reduce
      (fn [acc {:keys [name]}]
        (cond-> acc (not (acc name)) (assoc name fallback-texture)))
      explicit-textures
      samplers)))

(g/defnk produce-scene [_node-id scene material-name->material-scene-info]
  (if scene
    (model-scene/augment-scene scene _node-id "model" material-name->material-scene-info)
    {:aabb geom/empty-bounding-box
     :renderable {:passes [pass/selection]}}))

(g/defnk produce-bones [skeleton-bones animations-bones]
  (or animations-bones skeleton-bones))

(def TTexture
  {:sampler s/Str
   :texture (s/maybe (s/protocol resource/Resource))})

(def TVertexAttributes
  {s/Keyword s/Any})

(g/deftype Material
  {:name s/Str
   :material (s/maybe (s/protocol resource/Resource))
   :textures [TTexture]
   :attributes TVertexAttributes})

(g/defnode TextureBinding
  (property sampler g/Str) ; Required protobuf field.
  (property texture resource/Resource ; Required protobuf field.
            (value (gu/passthrough texture-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :texture-resource]
                                            [:gpu-texture-generator :gpu-texture-generator]
                                            [:build-targets :build-targets]))))
  (input texture-resource resource/Resource)
  (input gpu-texture-generator g/Any)
  (input build-targets g/Any :array)
  (output build-targets g/Any (gu/passthrough build-targets))
  (output texture-binding-info g/Any
          (g/fnk [_node-id sampler texture ^:try gpu-texture-generator :as info]
            (cond-> info (g/error-value? gpu-texture-generator) (dissoc :gpu-texture-generator)))))

(defn- detect-and-apply-renames [texture-binding-infos samplers]
  (util/detect-and-apply-renames texture-binding-infos :sampler samplers :name))

(g/defnode MaterialBinding
  (input copied-nodes g/Any :array :cascade-delete)
  (input dep-build-targets g/Any :array)
  (input shader ShaderLifecycle)
  (input vertex-space g/Keyword)
  (input samplers g/Any)

  (property name g/Str) ; Required protobuf field.
  (property material resource/Resource ; Required protobuf field.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:build-targets :dep-build-targets]
                                            [:samplers :samplers]
                                            [:shader :shader]
                                            [:attribute-infos :material-attribute-infos]
                                            [:vertex-space :vertex-space]))))
  (property material-index g/Num)
  (property vertex-attribute-overrides g/Any ; Always assigned in load-fn.
            (dynamic visible (g/constantly false)))
  (input material-resource resource/Resource)
  (input material-attribute-infos g/Any)
  (input texture-binding-infos g/Any :array)
  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))
  (output material-scene-info g/Any (g/fnk [shader vertex-space gpu-textures name material-attribute-infos vertex-attribute-bytes :as info] info))
  (output material-binding-info g/Any (g/fnk [_node-id name
                                              material
                                              ^:try material-attribute-infos
                                              vertex-attribute-overrides
                                              ^:try vertex-attribute-bytes
                                              ^:try samplers
                                              ^:try texture-binding-infos
                                              :as info]
                                        (let [info (cond-> info
                                                           (g/error-value? material-attribute-infos)
                                                           (assoc :material-attribute-infos [])
                                                           (g/error-value? vertex-attribute-bytes)
                                                           (assoc :vertex-attribute-bytes {}))]
                                          (cond
                                            (g/error-value? texture-binding-infos) (assoc info :texture-binding-infos [])
                                            (g/error-value? samplers) (dissoc info :samplers)
                                            :else (update info :texture-binding-infos detect-and-apply-renames samplers)))))
  (output vertex-attribute-bytes g/Any :cached (g/fnk [_node-id material-attribute-infos material-index vertex-attribute-overrides]
                                                 (graphics/attribute-bytes-by-attribute-key _node-id material-attribute-infos material-index vertex-attribute-overrides))))

(defmethod material/handle-sampler-names-changed ::MaterialBinding
  [evaluation-context material-binding-node old-name-index _new-name-index sampler-renames sampler-deletions]
  (let [texture-binding-infos (g/node-value material-binding-node :texture-binding-infos evaluation-context)
        texture-binding-name-index (util/name-index texture-binding-infos :sampler)
        implied-texture-binding-info-renames (util/detect-renames texture-binding-name-index old-name-index)]
    (into []
          (mapcat
            (fn [[name+order index]]
              ;; Texture binding could be implicitly renamed if its name does
              ;; not match the material sampler name (can happen on load)
              (let [name+order (implied-texture-binding-info-renames name+order name+order)]
                (concat
                  (when-let [[new-name] (sampler-renames name+order)]
                    (g/set-property (:_node-id (texture-binding-infos index)) :sampler new-name))
                  (when (sampler-deletions name+order)
                    (g/delete-node (:_node-id (texture-binding-infos index))))))))
          texture-binding-name-index)))

(defn- create-texture-binding-tx [material-binding sampler texture]
  (g/make-nodes (g/node-id->graph-id material-binding) [texture-binding [TextureBinding
                                                                         :sampler sampler
                                                                         :texture texture]]
    (g/connect texture-binding :_node-id material-binding :copied-nodes)
    (g/connect texture-binding :texture-binding-info material-binding :texture-binding-infos)
    (g/connect texture-binding :build-targets material-binding :dep-build-targets)))

(defn- create-material-binding-tx [model-node-id name material material-index textures vertex-attribute-overrides]
  (g/make-nodes (g/node-id->graph-id model-node-id) [material-binding [MaterialBinding
                                                                       :name name
                                                                       :material material
                                                                       :material-index material-index
                                                                       :vertex-attribute-overrides vertex-attribute-overrides]]
    (g/connect material-binding :_node-id model-node-id :copied-nodes)
    (g/connect material-binding :dep-build-targets model-node-id :dep-build-targets)
    (g/connect material-binding :material-scene-info model-node-id :material-scene-infos)
    (g/connect material-binding :material-binding-info model-node-id :material-binding-infos)
    (for [{:keys [sampler texture]} textures]
      (create-texture-binding-tx material-binding sampler texture))))

(def ^:private fake-resource
  (reify resource/Resource
    (children [_])
    (ext [_] "")
    (resource-type [_])
    (source-type [_])
    (exists? [_] false)
    (read-only? [_] true)
    (path [_] "")
    (abs-path [_] "")
    (proj-path [_] "")
    (resource-name [_] "")
    (workspace [_])
    (resource-hash [_])
    (openable? [_] false)
    (editable? [_] false)
    (loaded? [_] false)))

(g/defnk produce-model-properties [_node-id _declared-properties material-binding-infos mesh-material-ids]
  (let [model-node-id _node-id
        mesh-material-names (if (g/error-value? mesh-material-ids) #{} (set mesh-material-ids))
        proto-material-name->material-binding-info (into {} (map (juxt :name identity)) material-binding-infos)
        proto-material-names (into #{} (map :name) material-binding-infos)
        all-material-names (set/union mesh-material-names proto-material-names)
        new-props
        (into []
              (comp
                (map-indexed
                  (fn [material-index material-name]
                    (let [material-prop-key (keyword (str "__material__" material-index))]
                      (if-let [{:keys [_node-id material name texture-binding-infos material-attribute-infos vertex-attribute-overrides samplers]} (proto-material-name->material-binding-info material-name)]
                        ;; material exists
                        (let [sampler-name-index (util/name-index samplers :name)
                              texture-binding-name-index (util/name-index texture-binding-infos :sampler)
                              all-sampler-name+orders (set/union
                                                        (set (keys sampler-name-index))
                                                        (set (keys texture-binding-name-index)))
                              should-be-deleted (not (mesh-material-names name))
                              material-attribute-properties (graphics/attribute-property-entries _node-id material-attribute-infos material-index vertex-attribute-overrides)
                              material-binding-node-id _node-id
                              material-property [material-prop-key
                                                 (cond-> {:node-id material-binding-node-id
                                                          :label name
                                                          :type resource/Resource
                                                          :value (cond-> material should-be-deleted (or fake-resource))
                                                          :error (or
                                                                   (when should-be-deleted
                                                                     (g/->error material-binding-node-id :materials :warning material
                                                                                (format "'%s' is not defined in the mesh. Clear the field to delete it."
                                                                                        name)))
                                                                   (prop-resource-error :fatal material-binding-node-id :materials material "Material"))
                                                          :prop-kw :material
                                                          :edit-type {:type resource/Resource
                                                                      :ext "material"
                                                                      :clear-fn (fn [_ _]
                                                                                  (g/delete-node material-binding-node-id))}}
                                                         should-be-deleted
                                                         (assoc :original-value fake-resource))]
                              combined-material-properties (into [material-property]
                                                                 (map-indexed
                                                                   (fn [binding-index sampler-name+order]
                                                                     (let [texture-binding-prop-key (keyword (str "__sampler__" material-index "__" binding-index))]
                                                                       ;; texture binding exists
                                                                       (if-let [texture-binding-index (texture-binding-name-index sampler-name+order)]
                                                                         (let [{:keys [sampler texture _node-id]} (texture-binding-infos texture-binding-index)
                                                                               texture-binding-should-be-deleted (and samplers (not (sampler-name-index sampler-name+order)))]
                                                                           [texture-binding-prop-key
                                                                            (cond-> {:node-id _node-id
                                                                                     :label sampler
                                                                                     :type resource/Resource
                                                                                     :value (cond-> texture texture-binding-should-be-deleted (or fake-resource))
                                                                                     :prop-kw :texture
                                                                                     :error (when texture-binding-should-be-deleted
                                                                                              (g/->error _node-id :texture :warning texture
                                                                                                         (format "'%s' is not defined in the material. Clear the field to delete it."
                                                                                                                 sampler)))
                                                                                     :edit-type {:type resource/Resource
                                                                                                 :ext supported-image-exts
                                                                                                 :clear-fn (fn [_ _] (g/delete-node _node-id))}}
                                                                                    texture-binding-should-be-deleted
                                                                                    (assoc :original-value fake-resource))])
                                                                         ;; texture binding does not exist
                                                                         (let [sampler (key sampler-name+order)]
                                                                           [texture-binding-prop-key
                                                                            {:node-id material-binding-node-id
                                                                             :label sampler
                                                                             :value nil
                                                                             :type resource/Resource
                                                                             :edit-type {:type resource/Resource
                                                                                         :ext supported-image-exts
                                                                                         :set-fn (fn [_ _ _ new] (create-texture-binding-tx material-binding-node-id sampler new))}}])))))
                                                                 (sort-by key all-sampler-name+orders))]
                          (into combined-material-properties material-attribute-properties))
                        ;; material does not exist
                        [[material-prop-key
                          {:node-id _node-id
                           :label material-name
                           :value nil
                           :type resource/Resource
                           :error (prop-resource-error :fatal _node-id :material nil "Material")
                           :edit-type {:type resource/Resource
                                       :ext "material"
                                       :set-fn (fn [_evaluation-context _id _old new]
                                                 (create-material-binding-tx model-node-id material-name new material-index [] {}))}}]]))))
                cat)
              (sort all-material-names))]
    (-> _declared-properties
        (update :properties into new-props)
        (update :display-order into (map first) new-props))))

(g/defnode ModelNode
  (inherits resource-node/ResourceNode)

  (property name g/Str
            (default (protobuf/default ModelProto$ModelDesc :name))
            (dynamic visible (g/constantly false)))
  (property mesh resource/Resource ; Required protobuf field.
            (value (gu/passthrough mesh-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :mesh-resource]
                                            [:mesh-set-build-target :mesh-set-build-target]
                                            [:material-ids :mesh-material-ids]
                                            [:scene :scene])))
            (dynamic error (g/fnk [_node-id mesh]
                                  (prop-resource-error :fatal _node-id :mesh mesh "Mesh")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/model-file-types}))
            (dynamic label (properties/label-dynamic :model :mesh))
            (dynamic tooltip (properties/tooltip-dynamic :model :mesh)))
  (input copied-nodes g/Any :array :cascade-delete)
  (input material-binding-infos g/Any :array)
  (output materials [Material] :cached
          (g/fnk [material-binding-infos]
            (mapv
              (fn [{:keys [name material texture-binding-infos vertex-attribute-overrides]}]
                {:name name
                 :material material
                 :attributes vertex-attribute-overrides
                 :textures (into []
                                 (keep (fn [{:keys [sampler texture]}]
                                         (when texture
                                           {:sampler sampler :texture texture})))
                                 texture-binding-infos)})
              material-binding-infos)))
  (input scene g/Any)
  (input material-scene-infos g/Any :array)

  (output material-name->material-scene-info g/Any :cached
          (g/fnk [material-scene-infos]
            (model-scene/make-material-name->material-scene-info material-scene-infos)))

  (property create-go-bones g/Bool
            (default (protobuf/default ModelProto$ModelDesc :create-go-bones))
            (dynamic label (properties/label-dynamic :model :create-go-bones))
            (dynamic tooltip (properties/tooltip-dynamic :model :create-go-bones)))

  (property skeleton resource/Resource ; Nil is valid default.
            (value (gu/passthrough skeleton-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :skeleton-resource]
                                            [:bones :skeleton-bones]
                                            [:skeleton-build-target :skeleton-build-target])))
            (dynamic error (g/fnk [_node-id skeleton]
                                  (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/model-file-types}))
            (dynamic label (properties/label-dynamic :model :skeleton))
            (dynamic tooltip (properties/tooltip-dynamic :model :skeleton)))
  (property animations resource/Resource ; Nil is valid default.
            (value (gu/passthrough animations-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :animations-resource]
                                            [:bones :animations-bones]
                                            [:animation-ids :animation-ids]
                                            [:animation-info :animation-infos]
                                            [:animation-set-build-target :animation-set-build-target])))
            (dynamic error (g/fnk [_node-id animations]
                                  (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations "Animations")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/animation-file-types}))
            (dynamic label (properties/label-dynamic :model :animations))
            (dynamic tooltip (properties/tooltip-dynamic :model :animations)))
  (property default-animation g/Str
            (default (protobuf/default ModelProto$ModelDesc :default-animation))
            (dynamic error (g/fnk [_node-id default-animation animation-ids]
                                  (validate-default-animation _node-id default-animation animation-ids)))
            (dynamic edit-type (g/fnk [animation-ids]
                                      (properties/->choicebox (into [""] animation-ids))))
            (dynamic label (properties/label-dynamic :model :default-animation))
            (dynamic tooltip (properties/tooltip-dynamic :model :default-animation)))

  (input mesh-resource resource/Resource)
  (input mesh-set-build-target g/Any)
  (input mesh-material-ids g/Any)

  (input skeleton-resource resource/Resource)
  (input skeleton-build-target g/Any)
  (input animations-resource resource/Resource)
  (input animation-set-build-target g/Any)
  (input dep-build-targets g/Any :array)

  (input skeleton-bones g/Any)
  (input animations-bones g/Any)

  (input animation-infos g/Any :array)
  (input animation-ids g/Any)

  (output bones g/Any produce-bones)
  (output animation-resources g/Any (g/fnk [animations-resource] [animations-resource]))
  (output animation-info g/Any :cached animation-set/produce-animation-info)
  (output animation-set-info g/Any :cached animation-set/produce-animation-set-info)
  (output animation-set g/Any :cached animation-set/produce-animation-set)
  (output animation-ids g/Any :cached produce-animation-ids)

  ; if we're referencing a single animation file
  (output animation-set-build-target-single g/Any :cached produce-animation-set-build-target-single)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output _properties g/Properties :cached produce-model-properties))

(defn- migrated? [model-node-id model-desc evaluation-context]
  {:pre [(map? model-desc)]} ; ModelProto$ModelDesc in map format.
  (let [model-node-materials (g/node-value model-node-id :materials evaluation-context)]
    (if (g/error? model-node-materials)
      false
      (let [material-name->model-node-material (coll/pair-map-by :name model-node-materials)]
        (some (fn [model-desc-material]
                (let [material-name (:name model-desc-material)
                      model-node-material (material-name->model-node-material material-name)
                      model-desc-sampler-names (into #{} (map :sampler) (:textures model-desc-material))
                      model-node-sampler-names (into #{} (map :sampler) (:textures model-node-material))]
                  (not= model-desc-sampler-names model-node-sampler-names)))
              (:materials model-desc))))))

(defn- detect-and-flag-migrated! [evaluation-context model-node-id model-desc]
  {:pre [(map? model-desc)]} ; ModelProto$ModelDesc in map format.
  (when (migrated? model-node-id model-desc evaluation-context)
    (g/flag-nodes-as-migrated! evaluation-context [model-node-id])))

(defn load-model [_project self resource {:keys [materials] :as model-desc}]
  (concat
    (let [resolve-resource #(workspace/resolve-resource resource %)]
      (gu/set-properties-from-pb-map self ModelProto$ModelDesc model-desc
        name :name
        default-animation :default-animation
        mesh (resolve-resource :mesh)
        skeleton (resolve-resource :skeleton)
        animations (resolve-resource :animations)
        create-go-bones :create-go-bones))
    (map-indexed
      (fn [material-index {:keys [name material textures attributes]}]
        (let [material (workspace/resolve-resource resource material)
              textures (mapv (fn [{:keys [texture] :as texture-desc}]
                               (assoc texture-desc :texture (workspace/resolve-resource resource texture)))
                             textures)
              vertex-attribute-overrides (graphics/override-attributes->vertex-attribute-overrides attributes)]
          (create-material-binding-tx self name material material-index textures vertex-attribute-overrides)))
      materials)
    (g/callback-ec detect-and-flag-migrated! self model-desc)))

(defn- sanitize-model [{:keys [material textures materials] :as model-desc}]
  {:pre [(map? model-desc)]} ; ModelProto$ModelDesc in map format.
  (-> model-desc
      (dissoc :material :textures)
      (cond-> (and (zero? (count materials))
                   (or (pos? (count material))
                       (pos? (count textures))))
              (assoc :materials [(protobuf/make-map-without-defaults ModelProto$Material
                                   :name "default"
                                   :material material
                                   :textures (into []
                                                   (map-indexed
                                                     (fn [i tex-name]
                                                       (protobuf/make-map-without-defaults ModelProto$Texture
                                                         :sampler (.intern (str "tex" i))
                                                         :texture tex-name)))
                                                   textures))]))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "model"
    :label (localization/message "resource.type.model")
    :node-type ModelNode
    :ddf-type ModelProto$ModelDesc
    :load-fn load-model
    :sanitize-fn sanitize-model
    :icon model-icon
    :icon-class :design
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

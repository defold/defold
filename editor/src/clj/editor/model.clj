;; Copyright 2020-2026 The Defold Foundation
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
            [editor.dialogs :as dialogs]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.gltf :as gltf]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.image :as image]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.model-loader :as model-loader]
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

(def ^:private animations-message (properties/label-message :model :animations))
(def ^:private default-animation-message (properties/label-message :model :default-animation))
(def ^:private material-message (properties/label-message :material))
(def ^:private scene-message (properties/label-message :model :scene))
(def ^:private skeleton-message (properties/label-message :model :skeleton))

(def ^:private mesh-selection-file-types #{"glb" "gltf"})

(def ^:private auto-fill-gltf-material-indices-by-node-key
  ::auto-fill-gltf-material-indices-by-node)

(declare create-material-binding-tx)

(defn- gltf-source-resource? [resource]
  (and (resource/resource? resource)
       (contains? mesh-selection-file-types (resource/type-ext resource))))

(defn- resolve-selected-mesh [collision-meshes mesh-index]
  (when-not (g/error-value? collision-meshes)
    (coll/first-where #(= mesh-index (:index %))
                      (model-loader/named-meshes collision-meshes))))

(defn- selected-mesh-material-indices [selected-mesh]
  (into #{}
        (keep :material-index)
        (:primitives selected-mesh)))

(defn- source-collision-meshes [evaluation-context consumer-node-id source-resource]
  (when-let [project-node-id (project/get-project (:basis evaluation-context) consumer-node-id)]
    (when-let [source-node-id (project/get-resource-node project-node-id source-resource evaluation-context)]
      (g/node-value source-node-id :collision-meshes evaluation-context))))

(defn- multiple-selectable-meshes? [collision-meshes]
  (and (not (g/error-value? collision-meshes))
       (< 1 (count (model-loader/named-meshes collision-meshes)))))

(defn- gltf-auto-fill-candidate
  [evaluation-context node-id source-resource material-indices]
  (when (and (gltf-source-resource? source-resource)
             (coll/not-empty (gltf/material-binding-descriptors source-resource material-indices)))
    (let [material-binding-infos (g/node-value node-id :material-binding-infos evaluation-context)]
      {:node-id node-id
       :source-resource source-resource
       :material-indices material-indices
       :replaces-existing (boolean (and (not (g/error-value? material-binding-infos))
                                        (coll/not-empty material-binding-infos)))})))

(defn- confirm-gltf-auto-fill? [evaluation-context candidates]
  (let [{:keys [source-resource]} (candidates 0)
        localization-state (workspace/localization (resource/workspace source-resource) evaluation-context)
        replaces-existing (coll/any? :replaces-existing candidates)]
    (true?
      (dialogs/make-confirmation-dialog
        localization-state
        {:title (localization/message "dialog.model-auto-fill.title")
         :icon :icon/circle-question
         :header (localization/message "dialog.model-auto-fill.header"
                                       {"file" (resource/resource-name source-resource)})
         :content (localization/message (if replaces-existing
                                          "dialog.model-auto-fill.content-replace"
                                          "dialog.model-auto-fill.content"))
         :buttons [{:text (localization/message "dialog.model-auto-fill.button.keep-current")
                    :cancel-button true
                    :result false}
                   {:text (localization/message "dialog.model-auto-fill.button.auto-fill")
                    :default-button true
                    :result true}]}))))

(defn- prepare-gltf-auto-fill [evaluation-context candidates]
  (when (and (coll/not-empty candidates)
             (confirm-gltf-auto-fill? evaluation-context candidates))
    {auto-fill-gltf-material-indices-by-node-key
     (into {}
           (map (juxt :node-id :material-indices))
           candidates)}))

(defn- prepare-mesh-user-edit [evaluation-context _property set-operations]
  (let [candidates
        (into []
              (keep
                (fn [[node-id _prop-kw old-value new-value]]
                  (when (and (not= old-value new-value)
                             (gltf-source-resource? new-value))
                    (let [collision-meshes (source-collision-meshes evaluation-context node-id new-value)]
                      (when-not (multiple-selectable-meshes? collision-meshes)
                        (gltf-auto-fill-candidate evaluation-context node-id new-value nil))))))
              set-operations)]
    (prepare-gltf-auto-fill evaluation-context candidates)))

(defn- prepare-mesh-index-user-edit [evaluation-context _property set-operations]
  (let [candidates
        (into []
              (keep
                (fn [[node-id _prop-kw old-value new-value]]
                  (when (not= old-value new-value)
                    (let [source-resource (g/node-value node-id :mesh evaluation-context)
                          collision-meshes (g/node-value node-id :collision-meshes evaluation-context)
                          selected-mesh (resolve-selected-mesh collision-meshes new-value)]
                      (when (and (gltf-source-resource? source-resource)
                                 (multiple-selectable-meshes? collision-meshes)
                                 selected-mesh)
                        (gltf-auto-fill-candidate
                          evaluation-context
                          node-id
                          source-resource
                          (selected-mesh-material-indices selected-mesh)))))))
              set-operations)]
    (prepare-gltf-auto-fill evaluation-context candidates)))

(defn- auto-fill-material-indices-entry [evaluation-context node-id]
  (when-let [tx-data-context (:tx-data-context evaluation-context)]
    (find (get @tx-data-context auto-fill-gltf-material-indices-by-node-key)
          node-id)))

(defn- replace-gltf-material-bindings-tx
  [evaluation-context model-node-id source-resource material-indices]
  (let [material-binding-infos (g/node-value model-node-id :material-binding-infos evaluation-context)
        material-binding-node-ids (if (g/error-value? material-binding-infos)
                                    []
                                    (mapv :_node-id material-binding-infos))
        descriptors (gltf/material-binding-descriptors source-resource material-indices)
        initial-tx-data (cond-> []
                          (coll/not-empty material-binding-node-ids)
                          (into (g/delete-nodes material-binding-node-ids)))]
    (into initial-tx-data
          (mapcat
            (fn [{:keys [name material material-index textures]}]
              (create-material-binding-tx model-node-id name material material-index textures {})))
          descriptors)))

(defn- model-mesh-choicebox [collision-meshes]
  (let [collision-meshes (model-loader/named-meshes collision-meshes)
        name-counts (frequencies (into [] (map :name) collision-meshes))]
    {:type :choicebox
     :options (into [[-1 ""]]
                    (map (fn [{:keys [index name]}]
                           [index (if (= 1 (get name-counts name))
                                    name
                                    (format "%s (raw%d)" name index))]))
                    collision-meshes)}))

(defn- set-mesh-index [evaluation-context self _old-value new-value]
  (when (properties/user-edit? self :mesh-index evaluation-context)
    (let [collision-meshes (g/node-value self :collision-meshes evaluation-context)
          selected-mesh (resolve-selected-mesh collision-meshes new-value)
          tx-data (g/set-property self :mesh-name (or (:name selected-mesh) ""))]
      (if-let [[_ material-indices] (auto-fill-material-indices-entry evaluation-context self)]
        (into tx-data
              (replace-gltf-material-bindings-tx
                evaluation-context
                self
                (g/node-value self :mesh evaluation-context)
                material-indices))
        tx-data))))

(defn- set-mesh [evaluation-context self old-value new-value]
  (let [user-edit (properties/user-edit? self :mesh evaluation-context)
        resource-setter-tx-data
        (into []
              (project/resource-setter evaluation-context self old-value new-value
                                       [:resource :mesh-resource]
                                       [:mesh-set-build-target :mesh-set-build-target]
                                       [:content :mesh-content]
                                       [:material-ids :mesh-material-ids]
                                       [:collision-meshes :collision-meshes]
                                       [:source-scene :scene]))
        tx-data (into resource-setter-tx-data
                      (when user-edit
                        (g/set-properties self :mesh-name "" :mesh-index -1)))]
    (if-let [[_ material-indices] (and user-edit
                                       (auto-fill-material-indices-entry evaluation-context self))]
      (into tx-data
            (replace-gltf-material-bindings-tx evaluation-context self new-value material-indices))
      tx-data)))

(defn- model-mesh-selection-error [node-id mesh mesh-name mesh-index collision-meshes]
  (cond
    (and (str/blank? mesh-name)
         (= -1 mesh-index))
    nil

    (str/blank? mesh-name)
    (g/->error node-id :mesh-index :fatal mesh-index
               (localization/message "error.model-mesh-selection-name-required"))

    (not (contains? mesh-selection-file-types (some-> mesh resource/type-ext)))
    (g/->error node-id :mesh-index :fatal mesh-index
               (localization/message "error.model-mesh-selection-file-type"))

    (g/error-value? collision-meshes)
    collision-meshes

    :else
    (let [selected-mesh (model-loader/resolve-named-mesh collision-meshes mesh-name mesh-index)]
      (when (nil? selected-mesh)
        (g/->error node-id :mesh-index :fatal mesh-index
                   (localization/message "error.model-mesh-selection-missing" {"mesh" mesh-name}))))))

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

(g/defnk produce-pb-msg [name mesh mesh-name mesh-index materials skeleton animations default-animation create-go-bones]
  (cond-> (protobuf/make-map-without-defaults ModelProto$ModelDesc
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
            :create-go-bones create-go-bones)
    (not (str/blank? mesh-name))
    (assoc :mesh-name mesh-name)

    (not= -1 mesh-index)
    (assoc :mesh-index mesh-index)))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- resource-format-message [resource]
  (let [ext (resource/type-ext resource)]
    (if (str/blank? ext)
      (localization/message "error.no-file-extension")
      (str "." ext))))

(defn- prop-resource-unsupported-format? [v name supported-exts]
  (when (and v
             (not (contains? (set supported-exts) (resource/type-ext v))))
    (localization/message "error.model-unsupported-file-format"
                          {"property" name
                           "resource" (resource/resource->proj-path v)
                           "format" (resource-format-message v)
                           "supported_formats" (validation/format-ext-message supported-exts)})))

(defn- prop-resource-format-error [_node-id prop-kw prop-value prop-name supported-exts]
  (validation/prop-error :fatal _node-id prop-kw prop-resource-unsupported-format? prop-value prop-name supported-exts))

(defn- validate-default-animation [_node-id default-animation animation-ids]
  (when (not (str/blank? default-animation))
    (validation/prop-error :fatal _node-id :default-animation validation/prop-member-of? default-animation (set animation-ids)
                           (localization/message "error.animation-not-found" {"animation" default-animation
                                                                              "property" default-animation-message}))))

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

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets default-animation animation-ids animation-set-build-target animation-set-build-target-single mesh-content mesh-set-build-target materials material-binding-infos skeleton-build-target animations mesh mesh-name mesh-index collision-meshes skeleton create-go-bones]
  (or (some->> (into [(prop-resource-error :fatal _node-id :mesh mesh scene-message)
                      (prop-resource-format-error _node-id :mesh mesh scene-message model-scene/model-file-types)
                      (model-mesh-selection-error _node-id mesh mesh-name mesh-index collision-meshes)
                      (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton skeleton-message)
                      (prop-resource-format-error _node-id :skeleton skeleton skeleton-message model-scene/model-file-types)
                      (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations animations-message)
                      (prop-resource-format-error _node-id :animations animations animations-message model-scene/animation-file-types)
                      (validate-default-animation _node-id default-animation animation-ids)
                      (validation/prop-error :fatal _node-id :materials validation/prop-empty? (:materials pb-msg) material-message)]
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
            mesh-set-build-target
            (if (str/blank? mesh-name)
              mesh-set-build-target
              (let [selected-mesh-set (update (:mesh-set mesh-content) :raw-models
                                              #(into [] (filter (fn [raw-model]
                                                                  (= mesh-index (:mesh-index raw-model)))) %))]
                (rig/make-mesh-set-build-target workspace
                                                _node-id
                                                selected-mesh-set
                                                (:morph-target-textures mesh-content))))
            animation-set-build-target (if (nil? animation-set-build-target-single) animation-set-build-target animation-set-build-target-single)
            rig-scene-dep-build-targets {:animation-set animation-set-build-target
                                         :mesh-set mesh-set-build-target
                                         :skeleton skeleton-build-target}
            rig-scene-pb-msg {}
            rig-scene-build-target (rig/make-rig-scene-build-target workspace _node-id rig-scene-pb-msg dep-build-targets rig-scene-dep-build-targets)
            rt-pb-msg (cond-> {:rig-scene (:resource rig-scene-build-target)
                               :default-animation default-animation
                               :materials (:materials pb-msg)
                               :create-go-bones create-go-bones}
                        (not (str/blank? mesh-name))
                        (assoc :mesh-index mesh-index)

                        true
                        (update-build-target-vertex-attributes material-binding-infos))
            dep-build-targets (into [rig-scene-build-target] (flatten dep-build-targets))]
        [(pipeline/make-protobuf-build-target _node-id resource ModelProto$Model rt-pb-msg dep-build-targets)])))

(g/defnk produce-scene [_node-id scene mesh-name mesh-index material-name->material-scene-info skeleton-resource]
  (if scene
    (model-scene/augment-scene scene
                               _node-id
                               "model"
                               material-name->material-scene-info
                               (some? skeleton-resource)
                               (if (str/blank? mesh-name) -1 mesh-index))
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
  (output gpu-textures g/Any :cached model-scene/produce-gpu-textures)
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
    (into []
          (map (fn [{:keys [sampler texture]}]
                 (create-texture-binding-tx material-binding sampler texture)))
          textures)))

(def ^:private fake-resource
  (reify resource/Resource
    (children [_])
    (ext [_] "")
    (resource-type [_])
    (source-type [_])
    (exists? [_] false)
    (read-only? [_] true)
    (symlink? [_] false)
    (path [_] "")
    (abs-path [_] "")
    (proj-path [_] "")
    (resource-name [_] "")
    (workspace [_])
    (resource-hash [_])
    (openable? [_] false)
    (editable? [_] false)
    (loaded? [_] false)))

(defn- relevant-mesh-material-ids [mesh-material-ids collision-meshes mesh-name mesh-index]
  (if (or (str/blank? mesh-name)
          (g/error-value? collision-meshes))
    (set mesh-material-ids)
    (if-let [selected-mesh (resolve-selected-mesh collision-meshes mesh-index)]
      (into #{}
            (keep #(get mesh-material-ids %))
            (selected-mesh-material-indices selected-mesh))
      (set mesh-material-ids))))

(g/defnk produce-model-properties [_node-id _declared-properties material-binding-infos mesh-material-ids collision-meshes mesh-name mesh-index]
  (let [model-node-id _node-id
        mesh-material-names (if (g/error-value? mesh-material-ids)
                              #{}
                              (relevant-mesh-material-ids mesh-material-ids collision-meshes mesh-name mesh-index))
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
                                                                                (localization/message "error.material-not-defined-in-mesh" {"material" name})))
                                                                   (prop-resource-error :fatal material-binding-node-id :materials material material-message))
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
                                                                                                         (localization/message "error.sampler-not-defined-in-material" {"sampler" sampler})))
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
                           :error (prop-resource-error :fatal _node-id :material nil material-message)
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
            (set set-mesh)
            (dynamic error (g/fnk [_node-id mesh ^:try scene]
                             (if (g/error-value? scene)
                               scene
                               (or (prop-resource-error :fatal _node-id :mesh mesh scene-message)
                                   (prop-resource-format-error _node-id :mesh mesh scene-message model-scene/model-file-types)))))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/model-file-types
                                              :prepare-user-edit-fn prepare-mesh-user-edit}))
            (dynamic label (properties/label-dynamic :model :scene))
            (dynamic tooltip (properties/tooltip-dynamic :model :scene)))
  (property mesh-name g/Str
            (default "")
            (dynamic visible (g/constantly false)))
  (property mesh-index g/Int
            (default -1)
            (value (g/fnk [^:try collision-meshes mesh-index mesh-name]
                     (if (g/error-value? collision-meshes)
                       mesh-index
                       (or (:index (model-loader/resolve-named-mesh collision-meshes mesh-name mesh-index))
                           mesh-index))))
            (set set-mesh-index)
            (dynamic visible (g/fnk [mesh]
                               (contains? mesh-selection-file-types (some-> mesh resource/type-ext))))
            (dynamic read-only? (g/fnk [mesh ^:try collision-meshes]
                                  (or (nil? mesh)
                                      (g/error-value? collision-meshes))))
            (dynamic edit-type (g/fnk [^:try collision-meshes]
                                 (assoc (if (g/error-value? collision-meshes)
                                          (model-mesh-choicebox [])
                                          (model-mesh-choicebox collision-meshes))
                                   :prepare-user-edit-fn prepare-mesh-index-user-edit)))
            (dynamic error (g/fnk [_node-id mesh mesh-name mesh-index ^:try collision-meshes]
                             (model-mesh-selection-error _node-id mesh mesh-name mesh-index collision-meshes)))
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
            (dynamic error (g/fnk [_node-id skeleton ^:try skeleton-bones]
                             (if (g/error-value? skeleton-bones)
                               skeleton-bones
                               (or (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton skeleton-message)
                                   (prop-resource-format-error _node-id :skeleton skeleton skeleton-message model-scene/model-file-types)))))
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
            (dynamic error (g/fnk [_node-id animations ^:try animations-bones]
                             (if (g/error-value? animations-bones)
                               animations-bones
                               (or (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations animations-message)
                                   (prop-resource-format-error _node-id :animations animations animations-message model-scene/animation-file-types)))))
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
  (input mesh-content g/Any)
  (input mesh-set-build-target g/Any)
  (input mesh-material-ids g/Any)
  (input collision-meshes g/Any)

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
  (let [basis (g/now)
        resolve-resource #(workspace/resolve-resource basis resource %)]
    (concat
      (gu/set-properties-from-pb-map self ModelProto$ModelDesc model-desc
        name :name
        default-animation :default-animation
        mesh (resolve-resource :mesh)
        mesh-name :mesh-name
        mesh-index (:mesh-index :or -1)
        skeleton (resolve-resource :skeleton)
        animations (resolve-resource :animations)
        create-go-bones :create-go-bones)
      (map-indexed
        (fn [material-index {:keys [name material textures attributes]}]
          (let [material (resolve-resource material)
                textures (mapv (fn [{:keys [texture] :as texture-desc}]
                                 (assoc texture-desc :texture (resolve-resource texture)))
                               textures)
                vertex-attribute-overrides (graphics/override-attributes->vertex-attribute-overrides attributes)]
            (create-material-binding-tx self name material material-index textures vertex-attribute-overrides)))
        materials)
      (g/callback-ec detect-and-flag-migrated! self model-desc))))

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
    :category (localization/message "resource.category.components")
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

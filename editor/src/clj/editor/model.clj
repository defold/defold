(ns editor.model
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.pipeline :as pipeline]
            [editor.texture-unit :as texture-unit])
  (:import [com.dynamo.model.proto ModelProto$Model ModelProto$ModelDesc]
           [com.dynamo.rig.proto Rig$RigScene]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [name mesh material textures skeleton animations default-animation]
  (cond-> {:mesh (resource/resource->proj-path mesh)
           :material (resource/resource->proj-path material)
           :textures (mapv resource/resource->proj-path textures)
           :skeleton (resource/resource->proj-path skeleton)
           :animations (resource/resource->proj-path animations)
           :default-animation default-animation}
    (not (str/blank? name))
    (assoc :name name)))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- validate-default-animation [_node-id default-animation animation-ids]
  (when (and (not (str/blank? default-animation)) (seq animation-ids))
    (validation/prop-error :fatal _node-id :default-animation validation/prop-member-of? default-animation (set animation-ids)
                           (format "Animation '%s' does not exist" default-animation))))

(g/defnk produce-rig-scene-build-target [_node-id animation-set-build-target mesh-set-build-target skeleton-build-target texture-build-targets]
  (let [workspace (project/workspace (project/get-project _node-id))
        resource-type (workspace/get-resource-type workspace "rigscene")
        resource (resource/make-memory-resource workspace resource-type (str (gensym)))
        dep-build-targets (filterv some? (into [animation-set-build-target mesh-set-build-target skeleton-build-target] (remove nil? texture-build-targets)))
        rig-scene-pb (cond-> {}

                             (some? animation-set-build-target)
                             (assoc :animation-set (:resource animation-set-build-target))

                             (some? mesh-set-build-target)
                             (assoc :mesh-set (:resource mesh-set-build-target))

                             (some? skeleton-build-target)
                             (assoc :skeleton (:resource skeleton-build-target))

                             (seq texture-build-targets)
                             (assoc :textures (mapv :resource texture-build-targets)))]
    (pipeline/make-pb-map-build-target _node-id resource dep-build-targets Rig$RigScene rig-scene-pb)))

(g/defnk produce-build-targets [_node-id resource pb-msg rig-scene-build-target dep-build-targets default-animation animation-ids animations material mesh skeleton]
  (or (some->> [(prop-resource-error :fatal _node-id :mesh mesh "Mesh")
                (prop-resource-error :fatal _node-id :material material "Material")
                (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")
                (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations "Animations")
                (validate-default-animation _node-id default-animation animation-ids)]
               (filterv some?)
               not-empty
               g/error-aggregate)
      (let [rt-pb-msg (assoc (select-keys pb-msg [:material :default-animation])
                        :rig-scene (:resource rig-scene-build-target))
            dep-build-targets (into [rig-scene-build-target] (flatten dep-build-targets))]
        [(pipeline/make-pb-map-build-target _node-id resource dep-build-targets ModelProto$Model rt-pb-msg)])))

(g/defnk produce-scene [scene shader gpu-textures]
  (update scene :renderable (fn [r]
                              (cond-> r
                                shader (assoc-in [:user-data :shader] shader)
                                true (assoc-in [:user-data :textures] gpu-textures)
                                true (update :batch-key (fn [old-key] [old-key shader (texture-unit/batch-key-entry gpu-textures)]))))))

(g/defnode ModelNode
  (inherits texture-unit/TextureUnitBaseNode)

  (property name g/Str (dynamic visible (g/constantly false)))
  (property mesh resource/Resource
            (value (gu/passthrough mesh-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :mesh-resource]
                                            [:aabb :aabb]
                                            [:mesh-set-build-target :mesh-set-build-target]
                                            [:scene :scene])))
            (dynamic error (g/fnk [_node-id mesh]
                                  (prop-resource-error :fatal _node-id :mesh mesh "Mesh")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "dae"})))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets]
                                            [:shader :shader])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "material"})))
  (property skeleton resource/Resource
            (value (gu/passthrough skeleton-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :skeleton-resource]
                                            [:skeleton-build-target :skeleton-build-target])))
            (dynamic error (g/fnk [_node-id skeleton]
                                  (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "dae"})))
  (property animations resource/Resource
            (value (gu/passthrough animations-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :animations-resource]
                                            [:animation-set :animation-set]
                                            [:animation-set-build-target :animation-set-build-target])))
            (dynamic error (g/fnk [_node-id animations]
                                  (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations "Animations")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext ["dae" "animationset"]})))
  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id default-animation animation-ids]
                             (validate-default-animation _node-id default-animation animation-ids)))
            (dynamic edit-type (g/fnk [animation-ids]
                                 (properties/->choicebox (into [""] animation-ids)))))

  (input mesh-resource resource/Resource)
  (input mesh-set-build-target g/Any)
  (input material-resource resource/Resource)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input skeleton-resource resource/Resource)
  (input skeleton-build-target g/Any)
  (input animations-resource resource/Resource)
  (input animation-set g/Any)
  (input animation-set-build-target g/Any)
  (input dep-build-targets g/Any :array)
  (input scene g/Any)
  (input shader ShaderLifecycle)

  (output animation-ids g/Any :cached (g/fnk [animation-set] (mapv :id (:animations animation-set))))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (input aabb AABB)
  (output aabb AABB (gu/passthrough aabb))
  (output rig-scene-build-target g/Any :cached produce-rig-scene-build-target)
  (output gpu-textures g/Any :cached (g/fnk [default-tex-params gpu-texture-generators material-samplers]
                                       (texture-unit/gpu-textures-by-sampler-name default-tex-params gpu-texture-generators material-samplers)))
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties material-samplers textures]
                                             (texture-unit/properties _node-id _declared-properties material-samplers textures))))

(defn load-model [project self resource pb]
  (concat
    (g/connect project :default-tex-params self :default-tex-params)
    (g/set-property self :name (:name pb) :default-animation (:default-animation pb))
    (for [res [:mesh :material [:textures] :skeleton :animations]]
      (if (vector? res)
        (let [res (first res)]
          (g/set-property self res (mapv #(workspace/resolve-resource resource %) (get pb res))))
        (->> (get pb res)
          (workspace/resolve-resource resource)
          (g/set-property self res))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "model"
    :label "Model"
    :node-type ModelNode
    :ddf-type ModelProto$ModelDesc
    :load-fn load-model
    :icon model-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

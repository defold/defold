;; Copyright 2020-2022 The Defold Foundation
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
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.material :as material]
            [editor.model-scene :as model-scene]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.digest :as digest])
  (:import [com.dynamo.gamesys.proto ModelProto$Model ModelProto$ModelDesc]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-animation-set-build-target-single [_node-id resource animations-resource animation-set]
  (let [is-single-anim (and (not (empty? animation-set))
                            (not (animation-set/is-animation-set? animations-resource)))]
    (when is-single-anim
      (rig/make-animation-set-build-target (resource/workspace resource) _node-id animation-set))))

(g/defnk produce-animation-ids [_node-id resource animations-resource animation-set-info animation-set]
  (let [is-single-anim (or (empty? animation-set)
                           (not (animation-set/is-animation-set? animations-resource)))]
    (if is-single-anim
      (if animations-resource
        [(resource/base-name animations-resource)] ; single animation file
        [])
      (:animation-ids animation-set-info))))

(g/defnk produce-pb-msg [name mesh material textures skeleton animations default-animation]
  (cond-> {:mesh (resource/resource->proj-path mesh)
           :material (resource/resource->proj-path material)
           :textures (mapv resource/resource->proj-path textures)
           :skeleton (resource/resource->proj-path skeleton)
           :animations (resource/resource->proj-path animations)
           :default-animation default-animation}
    (not (str/blank? name))
    (assoc :name name)))

(defn- build-pb [resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb
                    (map (fn [[label res]]
                           [label (resource/proj-path (get dep-resources res))])
                         (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes ModelProto$Model pb)}))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- res-fields->resources [pb-msg deps-by-source fields]
  (->> (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb-msg (first field))))) [field])) fields)
    (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb-msg label) (get pb-msg label)))]))))

(defn- validate-default-animation [_node-id default-animation animation-ids]
  (when (not (str/blank? default-animation))
    (validation/prop-error :fatal _node-id :default-animation validation/prop-member-of? default-animation (set animation-ids)
                           (format "Animation '%s' does not exist" default-animation))))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets default-animation animation-ids animation-set-build-target animation-set-build-target-single mesh-set-build-target skeleton-build-target animations material mesh skeleton]
  (or (some->> [(prop-resource-error :fatal _node-id :mesh mesh "Mesh")
                (prop-resource-error :fatal _node-id :material material "Material")
                (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")
                (validation/prop-error :fatal _node-id :animations validation/prop-resource-not-exists? animations "Animations")
                (validate-default-animation _node-id default-animation animation-ids)]
               (filterv some?)
               not-empty
               g/error-aggregate)
      (let [workspace (resource/workspace resource)
            animation-set-build-target (if (nil? animation-set-build-target-single) animation-set-build-target animation-set-build-target-single)
            rig-scene-type (workspace/get-resource-type workspace "rigscene")
            rig-scene-pseudo-data (digest/string->sha1-hex (str/join (map #(-> % :resource :resource :data) [animation-set-build-target mesh-set-build-target skeleton-build-target])))
            rig-scene-resource (resource/make-memory-resource workspace rig-scene-type rig-scene-pseudo-data)
            rig-scene-dep-build-targets {:animation-set animation-set-build-target
                                         :mesh-set mesh-set-build-target
                                         :skeleton skeleton-build-target}
            rig-scene-pb-msg {:texture-set ""} ; Set in the ModelProto$Model message. Other field values taken from build targets.
            rig-scene-additional-resource-keys []
            rig-scene-build-targets (rig/make-rig-scene-build-targets _node-id rig-scene-resource rig-scene-pb-msg dep-build-targets rig-scene-additional-resource-keys rig-scene-dep-build-targets)
            pb-msg (select-keys pb-msg [:material :textures :default-animation])
            dep-build-targets (into rig-scene-build-targets (flatten dep-build-targets))
            deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
            dep-resources (into (res-fields->resources pb-msg deps-by-source [:rig-scene :material])
                                (filter second (res-fields->resources pb-msg deps-by-source [[:textures]])))]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-pb
            :user-data {:pb pb-msg
                        :dep-resources dep-resources}
            :deps dep-build-targets})])))

(g/defnk produce-gpu-textures [_node-id samplers gpu-texture-generators]
  (into {} (map (fn [unit-index sampler {tex-fn :f tex-args :args}]
                  (let [request-id [_node-id unit-index]
                        params     (material/sampler->tex-params sampler)
                        texture    (tex-fn tex-args request-id params unit-index)]
                    [(:name sampler) texture]))
                (range)
                samplers
                gpu-texture-generators)))

(g/defnk produce-scene [_node-id scene shader gpu-textures vertex-space]
  (if (some? scene)
    (update scene :renderable
            (fn [r]
              (cond-> r
                      shader (assoc-in [:user-data :shader] shader)
                      true (assoc-in [:user-data :textures] gpu-textures)
                      true (assoc-in [:user-data :vertex-space] vertex-space)
                      true (update :batch-key
                                   (fn [old-key]
                                     ;; We can only batch-render models that use
                                     ;; :vertex-space-world. In :vertex-space-local
                                     ;; we must supply individual transforms for
                                     ;; each model instance in the shader uniforms.
                                     (when (= :vertex-space-world vertex-space)
                                       [old-key shader gpu-textures]))))))
    {:aabb geom/empty-bounding-box
     :renderable {:passes [pass/selection]}}))

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (into v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(g/defnk produce-bones [skeleton-bones animations-bones]
  (or animations-bones skeleton-bones))

(g/defnode ModelNode
  (inherits resource-node/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))
  (property mesh resource/Resource
            (value (gu/passthrough mesh-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :mesh-resource]
                                            [:aabb :aabb]
                                            [:mesh-set-build-target :mesh-set-build-target]
                                            [:scene :scene])))
            (dynamic error (g/fnk [_node-id mesh]
                                  (prop-resource-error :fatal _node-id :mesh mesh "Mesh")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/model-file-types})))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :samplers]
                                            [:build-targets :dep-build-targets]
                                            [:shader :shader]
                                            [:vertex-space :vertex-space])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "material"})))
  (property textures resource/ResourceVec
            (value (gu/passthrough texture-resources))
            (set (fn [evaluation-context self old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context) self)
                         connections [[:resource :texture-resources]
                                      [:build-targets :dep-build-targets]
                                      [:gpu-texture-generator :gpu-texture-generators]]]
                     (concat
                      (for [r old-value]
                        (if r
                          (project/disconnect-resource-node evaluation-context project r self connections)
                          (g/disconnect project :nil-resource self :texture-resources)))
                      (for [r new-value]
                        (if r
                          (:tx-data (project/connect-resource-node evaluation-context project r self connections))
                          (g/connect project :nil-resource self :texture-resources)))))))
            (dynamic visible (g/constantly false)))
  (property skeleton resource/Resource
            (value (gu/passthrough skeleton-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :skeleton-resource]
                                            [:bones :skeleton-bones]
                                            [:skeleton-build-target :skeleton-build-target])))
            (dynamic error (g/fnk [_node-id skeleton]
                                  (validation/prop-error :fatal _node-id :skeleton validation/prop-resource-not-exists? skeleton "Skeleton")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext model-scene/model-file-types})))
  (property animations resource/Resource
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
                                              :ext model-scene/animation-file-types})))
  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id default-animation animation-ids]
                                  (validate-default-animation _node-id default-animation animation-ids)))
            (dynamic edit-type (g/fnk [animation-ids]
                                      (properties/->choicebox (into [""] animation-ids)))))

  (input mesh-resource resource/Resource)
  (input mesh-set-build-target g/Any)
  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input skeleton-resource resource/Resource)
  (input skeleton-build-target g/Any)
  (input animations-resource resource/Resource)
  (input animation-set-build-target g/Any)
  (input texture-resources resource/Resource :array)
  (input gpu-texture-generators g/Any :array)
  (input dep-build-targets g/Any :array)
  (input scene g/Any)
  (input shader ShaderLifecycle)
  (input vertex-space g/Keyword)

  (input skeleton-bones g/Any)
  (input animations-bones g/Any)

  (input animation-infos g/Any :array)
  (input animation-ids g/Any)
  (input aabb AABB)

  (output bones g/Any produce-bones)

  (output animation-resources g/Any (g/fnk [animations-resource] [animations-resource]))

  (output animation-info g/Any :cached animation-set/produce-animation-info)
  (output animation-set-info g/Any :cached animation-set/produce-animation-set-info)
  (output animation-set g/Any :cached animation-set/produce-animation-set)
  (output animation-ids g/Any :cached produce-animation-ids)

  ; if we're referencing a single animation file
  (output animation-set-build-target-single g/Any :cached produce-animation-set-build-target-single)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB (gu/passthrough aabb))
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties textures samplers]
                                                  (let [resource-type (get-in _declared-properties [:properties :material :type])
                                                        prop-entry {:node-id _node-id
                                                                    :type resource-type
                                                                    :edit-type {:type resource/Resource
                                                                                :ext (conj image/exts "cubemap")}}
                                                        keys (map :name samplers)
                                                        p (->> keys
                                                               (map-indexed (fn [i s]
                                                                              [(keyword (format "texture%d" i))
                                                                               (-> prop-entry
                                                                                   (assoc :value (get textures i)
                                                                                          :label s)
                                                                                   (assoc-in [:edit-type :set-fn]
                                                                                             (fn [_evaluation-context self old-value new-value]
                                                                                               (g/update-property self :textures vset i new-value))))])))]
                                                    (-> _declared-properties
                                                        (update :properties into p)
                                                        (update :display-order into (map first p)))))))

(defn load-model [project self resource pb]
  (concat
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

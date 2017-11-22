(ns editor.mesh
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.texture :as texture]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.mesh.proto MeshProto$Mesh MeshProto$MeshDesc]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [material textures]
  {:material (resource/resource->proj-path material)
   :textures (mapv resource/resource->proj-path textures)})

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
    {:resource resource
     :content (protobuf/map->bytes MeshProto$Mesh pb)}))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- res-fields->resources [pb-msg deps-by-source fields]
  (->> (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb-msg (first field))))) [field])) fields)
    (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb-msg label) (get pb-msg label)))]))))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets material]
  (or (some->> [(prop-resource-error :fatal _node-id :material material "Material")]
               (filterv some?)
               not-empty
               g/error-aggregate)
      (let [workspace (resource/workspace resource)
            pb-msg (select-keys pb-msg [:material :textures])
            dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
            dep-resources (into (res-fields->resources pb-msg deps-by-source [:material])
                                (filter second (res-fields->resources pb-msg deps-by-source [[:textures]])))]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-pb
          :user-data {:pb pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets}])))

(g/defnk produce-gpu-textures [_node-id samplers gpu-texture-generators]
  (into {} (map (fn [unit-index sampler {:keys [generate-fn user-data]}]
                  (let [request-id [_node-id unit-index]
                        params (material/sampler->tex-params sampler)
                        texture (generate-fn user-data request-id params unit-index)]
                    [(:name sampler) texture]))
                (range)
                samplers
                gpu-texture-generators)))

(g/defnk produce-scene [_node-id]
  {:node-id _node-id
   :aabb (geom/null-aabb)
   #_:renderable #_{:render-fn render-scene
                :batch-key _node-id
                :select-batch-key _node-id
                :user-data {}
                :passes [pass/opaque pass/selection pass/outline]}})

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (into v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(g/defnode MeshNode
  (inherits resource-node/ResourceNode)

  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :samplers]
                                            [:build-targets :dep-build-targets]
                                            [:shader :shader])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "material"})))
  (property textures resource/ResourceVec
            (value (gu/passthrough texture-resources))
            (set (fn [_evaluation-context self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :texture-resources]
                                      [:build-targets :dep-build-targets]
                                      [:gpu-texture-generator :gpu-texture-generators]]]
                     (concat
                       (for [r old-value]
                         (if r
                           (project/disconnect-resource-node project r self connections)
                           (g/disconnect project :nil-resource self :texture-resources)))
                       (for [r new-value]
                         (if r
                           (project/connect-resource-node project r self connections)
                           (g/connect project :nil-resource self :texture-resources)))))))
            (dynamic visible (g/constantly false)))

  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input texture-resources resource/Resource :array)
  (input gpu-texture-generators g/Any :array)
  (input dep-build-targets g/Any :array)

  (input shader ShaderLifecycle)

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

(defn load-mesh [project self resource pb]
  (concat
    (for [res [:material [:textures]]]
      (if (vector? res)
        (let [res (first res)]
          (g/set-property self res (mapv #(workspace/resolve-resource resource %) (get pb res))))
        (->> (get pb res)
          (workspace/resolve-resource resource)
          (g/set-property self res))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "mesh"
    :label "Mesh"
    :node-type MeshNode
    :ddf-type MeshProto$MeshDesc
    :load-fn load-mesh
    :icon model-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

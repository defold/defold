(ns editor.mesh
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.digest :as digest])
  (:import [com.dynamo.mesh.proto MeshProto$MeshDesc]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]))

(set! *warn-on-reflection* true)

(def ^:private mesh-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [position-stream material vertices textures]
  (cond-> {:material (resource/resource->proj-path material)
           :vertices (resource/resource->proj-path vertices)
           :textures (mapv resource/resource->proj-path textures)}
    (not (str/blank? position-stream))
    (assoc :position-stream position-stream)))

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
    {:resource resource :content (protobuf/map->bytes MeshProto$MeshDesc pb)}))

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
            pb-msg (select-keys pb-msg [:material :vertices :textures :position-stream])
            dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
            dep-resources (into (res-fields->resources pb-msg deps-by-source [:material :vertices])
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

(g/defnode MeshNode
  (inherits resource-node/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))

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

  (property vertices resource/Resource
            (value (gu/passthrough vertices-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :vertices-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "buffer"})))
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

  (property position-stream g/Str
            (dynamic edit-type (g/constantly {:type g/Str})))

  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input vertices-resource resource/Resource)
  (input texture-resources resource/Resource :array)
  (input gpu-texture-generators g/Any :array)
  (input dep-build-targets g/Any :array)
  (input scene g/Any)
  (input shader ShaderLifecycle)
  (input vertex-space g/Keyword)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output scene g/Any :cached produce-scene)
  (input aabb AABB)
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
    (g/set-property self :position-stream (:position-stream pb))
    (for [res [:material :vertices [:textures]]]
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
    :icon mesh-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

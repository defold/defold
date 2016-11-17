(ns editor.model
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.defold-project :as project]
            [editor.rig :as rig]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.math :as math]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.validation :as validation]
            [editor.image :as image]
            [editor.material :as material])
  (:import [com.dynamo.model.proto ModelProto$ModelDesc ModelProto$Model]
           [com.dynamo.rig.proto Rig$RigScene]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [name mesh material textures skeleton animations default-animation]
  {:name name
   :mesh (resource/resource->proj-path mesh)
   :material (resource/resource->proj-path material)
   :textures (mapv resource/resource->proj-path textures)
   :skeleton (resource/resource->proj-path skeleton)
   :animations (resource/resource->proj-path animations)
   :default-animation default-animation})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str ModelProto$ModelDesc pb-msg)})

(g/defnk produce-rig-scene-pb-msg [pb-msg]
  {:skeleton (:skeleton pb-msg)
   :animation-set (:animations pb-msg)
   :mesh-set (:mesh pb-msg)
   ;; set in the ModelProto$Model message
   :texture-set ""})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (resource/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes ModelProto$Model pb)}))

(defn- build-rig-scene [self basis resource dep-resources user-data]
  (let [pb (:pb user-data)
        pb (reduce #(assoc %1 (first %2) (second %2))
                   pb
                   (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))])
                        (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Rig$RigScene pb)}))

(defn- make-rig-scene-build-targets
  [node-id resource pb dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb (first field))))) [field])) [:mesh-set])
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb label) (get pb label)))]) resource-fields)]
    [{:node-id node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-rig-scene
      :user-data {:pb pb
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnk produce-build-targets [_node-id resource pb-msg rig-scene-pb-msg dep-build-targets]
  (let [;; TODO - this is not the right way to do things
        ;; should be fixed as part of https://jira.int.midasplayer.com/browse/DEFEDIT-576
        workspace (resource/workspace resource)
        rig-scene-type     (workspace/get-resource-type workspace "rigscene")
        rig-scene-resource (resource/make-memory-resource workspace rig-scene-type (str (gensym)))
        rig-scene-build-targets (make-rig-scene-build-targets _node-id rig-scene-resource rig-scene-pb-msg dep-build-targets)
        pb-msg (select-keys pb-msg [:material :textures :default-animation])
        dep-build-targets (into rig-scene-build-targets (flatten dep-build-targets))
        deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb-msg (first field))))) [field])) [:rig-scene :material [:textures]])
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb-msg label) (get pb-msg label)))]) resource-fields)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb pb-msg
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnk produce-gpu-textures [_node-id samplers images]
  (->> (map (fn [s [i img]]
              (let [request-id [_node-id i]
                    params (material/sampler->tex-params s)
                    unit i
                    t (texture/image-texture request-id img params unit)]
                [(:name s) t])) samplers (map-indexed vector images))
   (into {})))

(g/defnk produce-scene [scene shader gpu-textures]
  (update-in scene [:renderable :user-data] (fn [u]
                                              (cond-> u
                                                shader (assoc :shader shader)
                                                true (assoc :textures gpu-textures)))))

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (into v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(g/defnode ModelNode
  (inherits project/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))
  (property mesh resource/Resource
            (value (gu/passthrough mesh-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :mesh-resource]
                                            [:content :mesh-pb]
                                            [:aabb :aabb]
                                            [:build-targets :dep-build-targets]
                                            [:scene :scene])))
            (dynamic error (g/fnk [_node-id mesh]
                                  (prop-resource-error :info _node-id :mesh mesh "Mesh")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                          :ext "dae"})))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
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
            (set (fn [basis self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :texture-resources]
                                      [:build-targets :dep-build-targets]
                                      [:content :images]]]
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
  (property skeleton resource/Resource
            (value (gu/passthrough skeleton-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :skeleton-resource])))
            (dynamic error (g/fnk [_node-id skeleton]
                                  (when skeleton
                                    (prop-resource-error :fatal _node-id :skeleton skeleton "Skeleton"))))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "dae"})))
  (property animations resource/Resource
            (value (gu/passthrough animations-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :animations-resource])))
            (dynamic error (g/fnk [_node-id animations]
                                  (when animations
                                    (prop-resource-error :fatal _node-id :animations animations "Animations"))))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext ["dae" "animset"]})))
  (property default-animation g/Str)

  (input mesh-resource resource/Resource)
  (input mesh-pb g/Any)
  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input skeleton-resource resource/Resource)
  (input animations-resource resource/Resource)
  (input texture-resources resource/Resource :array)
  (input images BufferedImage :array)
  (input dep-build-targets g/Any :array)
  (input scene g/Any)
  (input shader ShaderLifecycle)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output rig-scene-pb-msg g/Any :cached produce-rig-scene-pb-msg)
  (output save-data g/Any :cached produce-save-data)
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
                                                            (map-indexed (fn [i s] [(keyword (format "texture%d" i))
                                                                                    (-> prop-entry
                                                                                      (assoc :value (get textures i)
                                                                                             :label s)
                                                                                      (assoc-in [:edit-type :set-fn]
                                                                                                (fn [basis self old-value new-value]
                                                                                                  (g/update-property self :textures vset i new-value))))])))]
                                                    (-> _declared-properties
                                                      (update :properties into p)
                                                      (update :display-order into (map first p)))))))

(defn load-model [project self resource]
  (let [pb (protobuf/read-text ModelProto$ModelDesc resource)]
    (concat
      (g/set-property self :name (:name pb) :default-animation (:default-animation pb))
      (for [res [:mesh :material [:textures] :skeleton :animations]]
        (if (vector? res)
          (let [res (first res)]
            (g/set-property self res (mapv #(workspace/resolve-resource resource %) (get pb res))))
          (->> (get pb res)
            (workspace/resolve-resource resource)
            (g/set-property self res)))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                 :ext "model"
                                 :label "Model"
                                 :node-type ModelNode
                                 :load-fn (fn [project self resource] (load-model project self resource))
                                 :icon model-icon
                                 :view-types [:scene :text]
                                 :tags #{:component}))

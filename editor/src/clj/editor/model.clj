(ns editor.model
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.math :as math]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.validation :as validation]
            [editor.image :as image])
  (:import [com.dynamo.model.proto Model$ModelDesc]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(set! *warn-on-reflection* true)

(def ^:private model-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [mesh material textures]
  {:mesh (resource/proj-path mesh)
   :material (resource/proj-path material)
   :textures (mapv resource/proj-path textures)})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Model$ModelDesc pb-msg)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (resource/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Model$ModelDesc pb)}))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb-msg (first field))))) [field])) [:mesh :material [:textures]])
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb-msg label) (get pb-msg label)))]) resource-fields)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb pb-msg
                  :pb-class Model$ModelDesc
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (into v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(g/defnode ModelNode
  (inherits project/ResourceNode)

  (property mesh resource/Resource
            (value (gu/passthrough mesh-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :mesh-resource]
                                            [:content :mesh-pb]
                                            [:aabb :aabb]
                                            [:build-targets :dep-build-targets])))
            (validate (validation/validate-resource mesh)))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :samplers]
                                            [:build-targets :dep-build-targets])))
            (validate (validation/validate-resource material)))
  (property textures resource/ResourceVec
            (value (gu/passthrough texture-resources))
            (set (fn [basis self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :texture-resources]
                                      [:build-targets :dep-build-targets]]]
                     (concat
                       (for [r old-value]
                         (if r
                           (project/disconnect-resource-node project r self connections)
                           (g/disconnect project :nil-resource self :texture-resources)))
                       (for [r new-value]
                         (if r
                           (project/connect-resource-node project r self connections)
                           (g/connect project :nil-resource self :texture-resources)))))))
            (dynamic visible (g/always false)))

  (input mesh-resource resource/Resource)
  (input mesh-pb g/Any)
  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input texture-resources resource/Resource :array)
  (input dep-build-targets g/Any :array)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/always {}))
  (input aabb AABB)
  (output aabb AABB (gu/passthrough aabb))
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties textures samplers]
                                                  (let [resource-type (get-in _declared-properties [:properties :material :type])
                                                        prop-entry {:node-id _node-id
                                                                    :type resource-type
                                                                    :edit-type {:type resource/Resource
                                                                                :ext image/exts}}
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
                                                      (update :properties into p))))))

(defn load-model [project self resource]
  (let [pb (protobuf/read-text Model$ModelDesc resource)]
    (for [res [:mesh :material [:textures]]]
      (if (vector? res)
        (let [res (first res)]
          (g/set-property self res (mapv #(workspace/resolve-resource resource %) (get pb res))))
        (->> (get pb res)
         (workspace/resolve-resource resource)
         (g/set-property self res))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                 :ext "model"
                                 :label "Model"
                                 :node-type ModelNode
                                 :load-fn (fn [project self resource] (load-model project self resource))
                                 :icon model-icon
                                 :view-types [:scene :text]
                                 :tags #{:component}))

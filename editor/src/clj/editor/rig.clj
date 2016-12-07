(ns editor.rig
  (:require
   [editor.defold-project :as project]
   [editor.pipeline :as pipeline]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.workspace :as workspace])
  (:import
   [com.dynamo.rig.proto Rig$RigScene Rig$MeshSet Rig$AnimationSet Rig$Skeleton]))

(defn- build-skeleton
  [self basis resource dep-resources {:keys [skeleton] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$Skeleton skeleton)})

(defn make-skeleton-build-target
  [workspace node-id skeleton]
  (let [skeleton-type     (workspace/get-resource-type workspace "skeleton")
        skeleton-resource (resource/make-memory-resource workspace skeleton-type (str (gensym)))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource skeleton-resource)
     :build-fn  build-skeleton
     :user-data {:skeleton skeleton}}))


(defn- build-animation-set
  [self basis resource dep-resources {:keys [animation-set] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$AnimationSet animation-set)})

(defn make-animation-set-build-target
  [workspace node-id animation-set]
  (let [animation-set-type     (workspace/get-resource-type workspace "animationset")
        animation-set-resource (resource/make-memory-resource workspace animation-set-type (str (gensym)))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource animation-set-resource)
     :build-fn  build-animation-set
     :user-data {:animation-set animation-set}}))


(defn- build-mesh-set
  [self basis resource dep-resources {:keys [mesh-set] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$MeshSet mesh-set)})

(defn make-mesh-set-build-target
  [workspace node-id mesh-set]
  (let [mesh-set-type     (workspace/get-resource-type workspace "meshset")
        mesh-set-resource (resource/make-memory-resource workspace mesh-set-type (str (gensym)))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource mesh-set-resource)
     :build-fn  build-mesh-set
     :user-data {:mesh-set mesh-set}}))

(defn make-rig-scene-build-targets
  [node-id resource pb dep-build-targets resource-keys]
  (let [workspace (project/workspace (project/get-project node-id))
        skeleton-target (make-skeleton-build-target workspace node-id (:skeleton pb))
        animation-set-target (make-animation-set-build-target workspace node-id (:animation-set pb))
        mesh-set-target (make-mesh-set-build-target workspace node-id (:mesh-set pb))
        dep-build-targets (into [skeleton-target animation-set-target mesh-set-target] (flatten dep-build-targets))]
    [(pipeline/make-protobuf-build-target node-id resource dep-build-targets
                                          Rig$RigScene
                                          (assoc pb
                                                 :skeleton (-> skeleton-target :resource :resource)
                                                 :animation-set (-> animation-set-target :resource :resource)
                                                 :mesh-set (-> mesh-set-target :resource :resource))
                                          (into [:skeleton :animation-set :mesh-set] resource-keys))]))

(defn register-resource-types
  [workspace]
  (concat
   (workspace/register-resource-type workspace :ext "rigscene")
   (workspace/register-resource-type workspace :ext "skeleton")
   (workspace/register-resource-type workspace :ext "meshset")
   (workspace/register-resource-type workspace :ext "animationset")))

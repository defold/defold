(ns editor.rig
  (:require
   [editor.defold-project :as project]
   [editor.pipeline :as pipeline]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.workspace :as workspace]
   [util.digest :as digest])
  (:import
   [com.dynamo.rig.proto Rig$RigScene Rig$MeshSet Rig$AnimationSet Rig$Skeleton]))

(defn- build-skeleton
  [resource dep-resources {:keys [skeleton] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$Skeleton skeleton)})

(defn make-skeleton-build-target
  [workspace node-id skeleton]
  (let [skeleton-type     (workspace/get-resource-type workspace "skeleton")
        ;; data of memory-resource below only used for
        ;; resource/resource-hash used to name the
        ;; corresponding "generated" build resource.
        skeleton-resource (resource/make-memory-resource workspace skeleton-type (protobuf/map->sha1-hex Rig$Skeleton skeleton))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource skeleton-resource)
     :build-fn  build-skeleton
     :user-data {:skeleton skeleton}}))


(defn- build-animation-set
  [resource dep-resources {:keys [animation-set] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$AnimationSet animation-set)})

(defn make-animation-set-build-target
  [workspace node-id animation-set]
  (let [animation-set-type     (workspace/get-resource-type workspace "animationset")
        ;; data of memory-resource below only used for
        ;; resource/resource-hash used to name the
        ;; corresponding "generated" build resource.
        animation-set-resource (resource/make-memory-resource workspace animation-set-type (protobuf/map->sha1-hex Rig$AnimationSet animation-set))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource animation-set-resource)
     :build-fn  build-animation-set
     :user-data {:animation-set animation-set}}))


(defn- build-mesh-set
  [resource dep-resources {:keys [mesh-set] :as user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$MeshSet mesh-set)})

(defn make-mesh-set-build-target
  [workspace node-id mesh-set]
  (let [mesh-set-type     (workspace/get-resource-type workspace "meshset")
        ;; data of memory-resource below only used for
        ;; resource/resource-hash used to name the
        ;; corresponding "generated" build resource.
        mesh-set-resource (resource/make-memory-resource workspace mesh-set-type (protobuf/map->sha1-hex Rig$MeshSet mesh-set))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource mesh-set-resource)
     :build-fn  build-mesh-set
     :user-data {:mesh-set mesh-set}}))

(defn make-rig-scene-build-targets
  ([node-id resource pb dep-build-targets resource-keys]
   (let [workspace (project/workspace (project/get-project node-id))
         skeleton-target (make-skeleton-build-target workspace node-id (:skeleton pb))
         animation-set-target (make-animation-set-build-target workspace node-id (:animation-set pb))
         mesh-set-target (make-mesh-set-build-target workspace node-id (:mesh-set pb))
         build-targets {:skeleton skeleton-target
                        :animation-set animation-set-target
                        :mesh-set mesh-set-target}]
     (make-rig-scene-build-targets node-id resource pb dep-build-targets resource-keys build-targets)))
  ([node-id resource pb dep-build-targets resource-keys build-targets]
   (let [dep-build-targets (into []
                                 (concat (remove nil? (vals build-targets))
                                         (flatten dep-build-targets)))]
     [(pipeline/make-protobuf-build-target resource dep-build-targets
                                           Rig$RigScene
                                           (reduce (fn [pb key]
                                                     (if-let [build-target (build-targets key)]
                                                       (assoc pb key (-> build-target :resource :resource))
                                                       (dissoc pb key)))
                                                   pb
                                                   [:skeleton :animation-set :mesh-set])
                                           (into [:skeleton :animation-set :mesh-set] resource-keys))])))

(defn register-resource-types
  [workspace]
  (concat
   (workspace/register-resource-type workspace :ext "rigscene")
   (workspace/register-resource-type workspace :ext "skeleton")
   (workspace/register-resource-type workspace :ext "meshset")))

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

(ns editor.rig
  (:require [clojure.string :as string]
            [editor.build-target :as bt]
            [editor.pipeline :as pipeline]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline ModelUtil$PackedMorphTargetTexture]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$RigScene Rig$Skeleton]))

(defn- build-skeleton
  [resource _dep-resources {:keys [skeleton] :as _user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$Skeleton skeleton)})

(defn make-skeleton-build-target
  [workspace node-id skeleton]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "skeleton")
     :build-fn build-skeleton
     :user-data {:skeleton skeleton}}))

(defn- build-animation-set
  [resource _dep-resources {:keys [animation-set] :as _user-data}]
  {:resource resource :content (protobuf/map->bytes Rig$AnimationSet animation-set)})

(defn make-animation-set-build-target
  [workspace node-id animation-set]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "animationset")
     :build-fn build-animation-set
     :user-data {:animation-set animation-set}}))

(defn- replace-morph-target-texture-tokens
  [mesh-set morph-target-textures dep-resources]
  (let [token->texture-resource-path
        (coll/pair-map-by :token
                          (fn [{:keys [resource]}]
                            (resource/proj-path (get dep-resources resource resource)))
                          morph-target-textures)]
    (update mesh-set :models coll/mapv->
            update :meshes coll/mapv->
            (fn [mesh]
              (if-let [morph-target-texture (coll/not-empty (:morph-target-texture mesh))]
                (cond-> mesh
                  (string/starts-with? morph-target-texture "__morph_target_texture_")
                  (assoc :morph-target-texture (token->texture-resource-path morph-target-texture)))
                mesh)))))

(defn- build-morph-target-texture
  [resource _dep-resources {:keys [packed-texture] :as _user-data}]
  (let [{:keys [width height layer-count data]} packed-texture
        texture (ModelUtil$PackedMorphTargetTexture. (int width) (int height) (int layer-count) data)]
    {:resource resource
     :write-content-fn tex-gen/write-texturec-content-fn
     :user-data {:texture-generator-result (.toGenerateResult texture)}}))

(defn- build-mesh-set
  [resource dep-resources {:keys [mesh-set morph-target-textures] :as _user-data}]
  (let [mesh-set (cond-> mesh-set
                   (coll/not-empty morph-target-textures)
                   (replace-morph-target-texture-tokens morph-target-textures dep-resources))]
    {:resource resource :content (protobuf/map->bytes Rig$MeshSet mesh-set)}))

(defn- make-morph-target-texture-build-target
  [workspace node-id {:keys [packed-texture]}]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "texture")
     :build-fn build-morph-target-texture
     :user-data {:packed-texture packed-texture}}))

(defn make-mesh-set-build-target
  [workspace node-id mesh-set morph-target-textures]
  (let [morph-target-texture-build-targets
        (mapv (partial make-morph-target-texture-build-target workspace node-id)
              morph-target-textures)
        morph-target-textures
        (mapv (fn [morph-target-texture build-target]
                (assoc morph-target-texture :resource (:resource build-target)))
              morph-target-textures
              morph-target-texture-build-targets)]
    (bt/with-content-hash
      (cond-> {:node-id node-id
               :resource (workspace/make-placeholder-build-resource workspace "meshset")
               :build-fn build-mesh-set
               :user-data {:mesh-set mesh-set
                           :morph-target-textures morph-target-textures}}
              (coll/not-empty morph-target-texture-build-targets)
              (assoc :deps morph-target-texture-build-targets)))))

(defn make-rig-scene-build-target
  ([workspace node-id pb dep-build-targets]
   (let [skeleton-target (make-skeleton-build-target workspace node-id (:skeleton pb))
         animation-set-target (make-animation-set-build-target workspace node-id (:animation-set pb))
         mesh-set-target (make-mesh-set-build-target workspace node-id (:mesh-set pb) nil)
         build-targets {:skeleton skeleton-target
                        :animation-set animation-set-target
                        :mesh-set mesh-set-target}]
     (make-rig-scene-build-target workspace node-id pb dep-build-targets build-targets)))
  ([workspace node-id pb dep-build-targets build-targets]
   (let [dep-build-targets (into []
                                 (concat (remove nil? (vals build-targets))
                                         (flatten dep-build-targets)))
         pb-map (reduce (fn [pb key]
                          (if-let [build-target (build-targets key)]
                            (assoc pb key (-> build-target :resource :resource))
                            (dissoc pb key)))
                        pb
                        [:skeleton :animation-set :mesh-set :texture-set])
         resource (workspace/make-placeholder-resource workspace "rigscene")]
     (pipeline/make-protobuf-build-target node-id resource Rig$RigScene pb-map dep-build-targets))))

(defn register-resource-types
  [workspace]
  (concat
    (workspace/register-resource-type workspace :ext "rigscene")
    (workspace/register-resource-type workspace :ext "skeleton")
    (workspace/register-resource-type workspace :ext "meshset")))

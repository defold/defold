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
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline ModelBuffers ModelUtil$PackedMorphTargetTexture]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$Mesh Rig$MeshSet Rig$Skeleton]))

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
    (reduce (fn [mesh-set model-key]
              (update mesh-set model-key coll/mapv->
                      update :meshes coll/mapv->
                      (fn [mesh]
                        (if-let [morph-target-texture (coll/not-empty (:morph-target-texture mesh))]
                          (cond-> mesh
                            (string/starts-with? morph-target-texture "__morph_target_texture_")
                            (assoc :morph-target-texture (token->texture-resource-path morph-target-texture)))
                          mesh))))
            mesh-set
            [:models :raw-models])))

(defn- referenced-morph-target-texture-tokens
  [mesh-set]
  (into #{}
        (comp (mapcat mesh-set)
              (mapcat :meshes)
              (keep :morph-target-texture)
              (remove string/blank?))
        [:models :raw-models]))

(defn- build-morph-target-texture
  [resource _dep-resources {:keys [packed-texture] :as _user-data}]
  (let [{:keys [width height layer-count data]} packed-texture
        texture (ModelUtil$PackedMorphTargetTexture. (int width) (int height) (int layer-count) data)]
    {:resource resource
     :write-content-fn tex-gen/write-texturec-content-fn
     :user-data {:texture-generator-result (.toGenerateResult texture)}}))

(defn- build-model-buffers
  [resource _dep-resources {:keys [geometries] :as _user-data}]
  {:resource resource
   :content (ModelBuffers/compile (coll/mapv-> geometries #(protobuf/map->pb Rig$Mesh %)))})

(defn compile-mesh-set
  "Builds metadata referencing shared geometry ranges and morph textures."
  [dep-resources {:keys [mesh-set geometries morph-target-textures] :as _user-data}]
  (let [mesh-set (cond-> mesh-set
                   (coll/not-empty morph-target-textures)
                   (replace-morph-target-texture-tokens morph-target-textures dep-resources))]
    (protobuf/pb->map-without-defaults
      (ModelBuffers/metadata
        (protobuf/map->pb Rig$MeshSet mesh-set)
        (coll/mapv-> geometries #(protobuf/map->pb Rig$Mesh %))))))

(defn- build-mesh-set
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Rig$MeshSet (compile-mesh-set dep-resources user-data))})

(defn- make-morph-target-texture-build-target
  [workspace node-id {:keys [packed-texture]}]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "texture")
     :build-fn build-morph-target-texture
     :user-data {:packed-texture packed-texture}}))

(defn make-mesh-set-build-target
  [workspace node-id mesh-set morph-target-textures]
  (let [geometries (coll/into-> (ModelBuffers/geometries (protobuf/map->pb Rig$MeshSet mesh-set)) []
                     (map protobuf/pb->map-without-defaults))
        model-buffers-target (bt/with-content-hash
                               {:node-id node-id
                                :resource (workspace/make-placeholder-build-resource workspace "modelbuffers")
                                :build-fn build-model-buffers
                                :user-data {:geometries geometries}})
        referenced-texture-tokens (referenced-morph-target-texture-tokens mesh-set)
        morph-target-textures (into []
                                    (filter #(contains? referenced-texture-tokens (:token %)))
                                    morph-target-textures)
        morph-target-texture-build-targets
        (mapv (partial make-morph-target-texture-build-target workspace node-id)
              morph-target-textures)
        morph-target-textures
        (mapv (fn [morph-target-texture build-target]
                (assoc morph-target-texture :resource (:resource build-target)))
              morph-target-textures
              morph-target-texture-build-targets)]
    (bt/with-content-hash
      {:node-id node-id
       :resource (workspace/make-placeholder-build-resource workspace "meshset")
       :build-fn build-mesh-set
       :user-data {:metadata-version 1
                   :mesh-set mesh-set
                   :geometries geometries
                   :model-buffers (:resource model-buffers-target)
                   :morph-target-textures morph-target-textures}
       :deps (into [model-buffers-target] morph-target-texture-build-targets)})))

(defn register-resource-types
  [workspace]
  (concat
    (workspace/register-resource-type workspace :ext "rigscene")
    (workspace/register-resource-type workspace :ext "skeleton")
    (workspace/register-resource-type workspace :ext "modelbuffers")
    (workspace/register-resource-type workspace :ext "meshset")))

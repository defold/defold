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

(ns editor.model-loader
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.gltf :as gltf]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline ModelImporterJni$DataResolver ModelUtil ModelUtil$CollectedMorphTargetTexture ModelUtil$ModelMetadata ModelUtil$PackedMorphTargetTexture]
           [com.dynamo.bob.pipeline GLTFValidator GLTFValidator$ValidateError GLTFValidator$ValidateResult]
           [com.dynamo.bob.pipeline Modelimporter$Material Modelimporter$Mesh Modelimporter$Model Modelimporter$PrimitiveType]
           [com.dynamo.rig.proto Rig$MeshSet Rig$Skeleton]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn- morph-target-texture-limits [project-settings]
  (mapv #(int (get project-settings %))
        [["model" "max_morph_target_texture_width"]
         ["model" "max_morph_target_texture_height"]]))

(defn- packed-morph-target-texture->map [^ModelUtil$PackedMorphTargetTexture texture]
  {:width (.-width texture)
   :height (.-height texture)
   :layer-count (.-layerCount texture)
   :data (.-data texture)})

(defn- collected-morph-target-texture->map [^ModelUtil$CollectedMorphTargetTexture texture]
  (let [packed-texture (.-texture texture)]
    {:token (.-resourcePath texture)
     :packed-texture (packed-morph-target-texture->map packed-texture)}))

(defn- model-mesh->collision-primitive [^Modelimporter$Mesh mesh]
  (let [^Modelimporter$Material material (.-material mesh)]
    (cond-> {:index-count (alength (.-indices mesh))
             :position-count (alength (.-positions mesh))
             :triangles (= Modelimporter$PrimitiveType/PRIMITIVE_TYPE_TRIANGLES
                           (.-primitiveType mesh))}
      material
      (assoc :material-index (.-index material)
             :material-name (.-name material)))))

(defn- model->collision-mesh [^Modelimporter$Model model]
  {:index (.-index model)
   :name (.-name model)
   :name-generated (.-nameIsGenerated model)
   :primitives (mapv model-mesh->collision-primitive (.-meshes model))})

(defn named-meshes [meshes]
  (into []
        (remove #(or (:name-generated %)
                     (string/blank? (:name %))))
        meshes))

(defn named-mesh-choicebox-options [meshes]
  (let [meshes (named-meshes meshes)
        name-counts (frequencies (mapv :name meshes))]
    (into [[-1 ""]]
          (map (fn [{:keys [index name]}]
                 [index (if (= 1 (get name-counts name))
                          name
                          (format "%s (raw%d)" name index))]))
          meshes)))

(defn resolve-named-mesh [meshes mesh-name mesh-index]
  (let [matching-meshes (into []
                              (filter #(= mesh-name (:name %)))
                              (named-meshes meshes))]
    (case (count matching-meshes)
      0 nil
      1 (nth matching-meshes 0)
      (coll/first-where #(= mesh-index (:index %)) matching-meshes))))

(defn read-external-buffer-uris [^InputStream input-stream]
  (try
    (let [^ModelUtil$ModelMetadata metadata (ModelUtil/getModelMetadata input-stream)]
      (vec (.externalBufferUris metadata)))
    (catch Exception _
      ;; Scene validation reports malformed glTF data when the content output is
      ;; evaluated. Dependency discovery must not replace that detailed error.
      [])))

(defn- load-model-scene
  [resource ^InputStream stream morph-tex-w morph-tex-h]
  (let [workspace (resource/workspace resource)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        path (resource/path resource)
        options nil
        ^ModelImporterJni$DataResolver data-resolver (gltf/make-data-resolver #(workspace/resolve-workspace-resource workspace %))
        scene (ModelUtil/loadScene stream ^String path options data-resolver)
        bones (ModelUtil/loadSkeleton scene)
        material-ids (ModelUtil/loadMaterialNames scene)
        animation-ids (ModelUtil/getAnimationNames scene) ; sorted on duration (largest first)
        morph-target-texture-collector (ModelUtil/createMorphTargetTextureCollector)]
    (when-not (coll/empty? bones)
      (ModelUtil/skeletonToDDF bones skeleton-builder))
    (ModelUtil/loadModelsForPreview scene mesh-set-builder morph-tex-w morph-tex-h morph-target-texture-collector)
    (let [mesh-set (protobuf/pb->map-with-defaults (.build mesh-set-builder))
          skeleton (protobuf/pb->map-with-defaults (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :buffers (.buffers scene)
       :collision-meshes (mapv model->collision-mesh (.models scene))
       :morph-target-textures (mapv collected-morph-target-texture->map (.getTextures morph-target-texture-collector))
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- format-gltf-validation-errors [^java.util.List validation-errors]
  (string/join "\n"
               (mapv (fn [^GLTFValidator$ValidateError error]
                       (format "  - %s (pointer=%s, code=%s)"
                               (.message error)
                               (.pointer error)
                               (.code error)))
                     validation-errors)))

(defn- model-load-error [node-id resource message]
  (g/->error node-id nil :fatal nil
             (localization/message "error.model-load-failed" {"file" (resource/proj-path resource) "error" message})
             {:type :invalid-content :resource resource}))

(defn load-scene [node-id resource project-settings]
  (try
    (let [ext (string/lower-case (resource/ext resource))
          ^GLTFValidator$ValidateResult validation-result
          (when (or (= ext "gltf") (= ext "glb"))
            ;; Zip resources cannot validate external buffers through filesystem paths.
            (if (resource/zip-resource? resource)
              (with-open [stream (io/input-stream resource)]
                (GLTFValidator/validateGltf stream ext false))
              (GLTFValidator/validateGltf (resource/abs-path resource) true)))]
      (if (and validation-result (not (.result validation-result)))
        (model-load-error node-id resource
                          (str "glTF validation failed:\n"
                               (format-gltf-validation-errors (.errors validation-result))))
        (let [[morph-tex-w morph-tex-h] (morph-target-texture-limits project-settings)]
          (with-open [stream (io/input-stream resource)]
            (load-model-scene resource stream morph-tex-w morph-tex-h)))))
    (catch Exception e
      (let [path (resource/proj-path resource)
            message (.getMessage e)]
        (log/error :message (format "The file '%s' failed to load:\n%s" path message) :exception e)
        (model-load-error node-id resource message)))))

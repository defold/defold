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
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [com.dynamo.bob.pipeline ColladaUtil]
           [com.dynamo.bob.pipeline ModelUtil]
           [com.dynamo.bob.pipeline GLTFValidator GLTFValidator$ValidateError GLTFValidator$ValidateResult]
           [com.dynamo.rig.proto Rig$MeshSet Rig$Skeleton]
           [java.io File InputStream]
           [java.util ArrayList]))

(set! *warn-on-reflection* true)

(def ^:private default-morph-target-texture-size 1024)

(defn- parse-morph-texture-dimension [v]
  (cond
    (nil? v) default-morph-target-texture-size
    (number? v) (int v)
    (string? v) (try (Integer/parseInt (string/trim v))
                     (catch NumberFormatException _
                       default-morph-target-texture-size))
    :else default-morph-target-texture-size))

(defn- morph-target-texture-limits-from-project-settings [project-settings]
  [(parse-morph-texture-dimension (get project-settings ["model" "max_morph_target_texture_width"]))
   (parse-morph-texture-dimension (get project-settings ["model" "max_morph_target_texture_height"]))])

(defn- morph-target-texture-limits-from-disk
  "Fallback when loading without the editor project settings graph (e.g. some tests)."
  [^File project-directory]
  (let [game-project-file (io/file project-directory "game.project")]
    (with-open [r (io/reader game-project-file)]
      (let [raw (settings-core/parse-settings r)
            w-str (settings-core/get-setting raw ["model" "max_morph_target_texture_width"])
            h-str (settings-core/get-setting raw ["model" "max_morph_target_texture_height"])
            max-morph-target-texture-width (int (Integer/parseInt w-str))
            max-morph-target-texture-height (int (Integer/parseInt h-str))]
        [max-morph-target-texture-width max-morph-target-texture-height]))))

(defn- load-collada-scene
  "Collada has no morph-target support in the importer; do not read game.project morph atlas limits here."
  [^InputStream stream]
  (let [mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        scene (ColladaUtil/loadScene stream)
        bones (ColladaUtil/loadSkeleton scene)
        material-ids (ColladaUtil/loadMaterialNames scene)
        animation-ids (ArrayList.)]
    (ColladaUtil/loadSkeleton scene skeleton-builder)
    (ColladaUtil/loadModels scene mesh-set-builder)
    (let [mesh-set (protobuf/pb->map-with-defaults (.build mesh-set-builder))
          skeleton (protobuf/pb->map-with-defaults (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- load-model-scene
  "glTF/glb only: mesh build runs morph atlas size checks against game.project limits."
  [resource ^InputStream stream morph-tex-w morph-tex-h]
  (let [workspace (resource/workspace resource)
        project-directory (workspace/project-directory workspace)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        path (resource/path resource)
        options nil
        data-resolver (ModelUtil/createFileDataResolver project-directory)
        scene (ModelUtil/loadScene stream ^String path options data-resolver)
        bones (ModelUtil/loadSkeleton scene)
        material-ids (ModelUtil/loadMaterialNames scene)
        animation-ids (ModelUtil/getAnimationNames scene)] ; sorted on duration (largest first)
    (when-not (empty? bones)
      (ModelUtil/skeletonToDDF bones skeleton-builder))
    (ModelUtil/loadModels scene mesh-set-builder morph-tex-w morph-tex-h)
    (let [mesh-set (protobuf/pb->map-with-defaults (.build mesh-set-builder))
          skeleton (protobuf/pb->map-with-defaults (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :buffers (.buffers scene)
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

(defn- handle-gltf-validation-result [resource ^GLTFValidator$ValidateResult gltf-validation-result]
  (when-not (.result gltf-validation-result)
    (let [gltf-validation-errors (.errors gltf-validation-result)
          formatted-gltf-validation-error (format-gltf-validation-errors gltf-validation-errors)]
      (throw (ex-info (str "glTF validation failed:\n" formatted-gltf-validation-error)
                      {:resource resource
                       :errors gltf-validation-errors})))))

(defn- load-scene-internal [resource project-settings]
  (let [workspace (resource/workspace resource)
        project-directory (workspace/project-directory workspace)
        [morph-tex-w morph-tex-h] (if (some? project-settings)
                                    (morph-target-texture-limits-from-project-settings project-settings)
                                    (morph-target-texture-limits-from-disk project-directory))
        ext (string/lower-case (resource/ext resource))
        is-zip-resource? (resource/zip-resource? resource)]
    ;; First, run glTF/glb files through the bob validator.
    ;; For zip resources we validate from a stream and avoid validating external
    ;; resources (buffers on disk). For regular filesystem resources we validate
    ;; by absolute path and allow external resource validation.
    (when (or (= ext "gltf") (= ext "glb"))
      (if is-zip-resource?
        (with-open [stream (io/input-stream resource)]
          (handle-gltf-validation-result resource (GLTFValidator/validateGltf stream ext false)))
        (handle-gltf-validation-result resource (GLTFValidator/validateGltf (resource/abs-path resource) true))))
    ;; Then, open a new stream for actually loading the scene.
    (with-open [stream (io/input-stream resource)]
      (if (= "dae" ext)
        (load-collada-scene stream)
        (load-model-scene resource stream morph-tex-w morph-tex-h)))))

(defn load-scene
  ([node-id resource]
   (load-scene node-id resource nil))
  ([node-id resource project-settings]
   (try
     (load-scene-internal resource project-settings)
     (catch Exception e
       (let [path (resource/proj-path resource)
             message (.getMessage e)]
         (log/error :message (format "The file '%s' failed to load:\n%s" path message) :exception e)
         (g/->error node-id nil :fatal nil
                    (localization/message "error.model-load-failed" {"file" path "error" message})
                    {:type :invalid-content :resource resource}))))))

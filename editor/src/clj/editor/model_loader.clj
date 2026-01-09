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
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [com.dynamo.bob.pipeline ColladaUtil]
           [com.dynamo.bob.pipeline ModelUtil]
           [com.dynamo.rig.proto Rig$MeshSet Rig$Skeleton]
           [java.util ArrayList]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn- load-collada-scene [^InputStream stream]
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

(defn- load-model-scene [resource ^InputStream stream]
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
    (ModelUtil/loadModels scene mesh-set-builder)
    (let [mesh-set (protobuf/pb->map-with-defaults (.build mesh-set-builder))
          skeleton (protobuf/pb->map-with-defaults (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :buffers (.buffers scene)
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- load-scene-internal [resource]
  (with-open [stream (io/input-stream resource)]
    (let [ext (string/lower-case (resource/ext resource))]
      (if (= "dae" ext)
        (load-collada-scene stream)
        (load-model-scene resource stream)))))

(defn load-scene [node-id resource]
  (try
    (load-scene-internal resource)
    (catch Exception e
      (let [msg (format "The file '%s' failed to load:\n%s" (resource/proj-path resource) (.getMessage e))]
        (log/error :message msg :exception e)
        (g/->error node-id nil :fatal nil msg {:type :invalid-content :resource resource})))))

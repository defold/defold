;; Copyright 2022 The Defold Foundation
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
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- load-model-scene [^InputStream stream ^String path]
  (let [mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        options nil
        scene (ModelUtil/loadScene stream path options)
        bones (ModelUtil/loadSkeleton scene)
        material-ids (ModelUtil/loadMaterialNames scene)
        animation-ids (ArrayList.)]
    (when-not (empty? bones)
      (ModelUtil/skeletonToDDF bones skeleton-builder))
    (ModelUtil/loadModels scene mesh-set-builder)
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- load-scene-internal [resource]
  (with-open [stream (io/input-stream resource)]
    (let [ext (string/lower-case (resource/ext resource))]
      (if (= "dae" ext)
        (load-collada-scene stream)
        (load-model-scene stream (resource/path resource))))))

(defn load-scene [node-id resource]
  (try
    (load-scene-internal resource)
    (catch Exception e
      (let [msg (format "The file '%s' contains invalid data, likely produced by a buggy exporter." (resource/proj-path resource))]
        (log/error :message msg :exception e)
        (g/->error node-id nil :fatal nil msg {:type :invalid-content :resource resource})))))

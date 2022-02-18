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
            [editor.protobuf :as protobuf]
            [editor.resource :as resource])
  (:import [com.dynamo.bob.pipeline ModelUtil]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$Skeleton]
           [java.util ArrayList]))

(set! *warn-on-reflection* false)

(defn create-scene [resource]
  (let [stream (io/input-stream resource)
        scene (ModelUtil/loadScene stream (:ext resource))]
    scene))

(defn load-bones [resource]
  (let [scene (create-scene resource)
        bones (ModelUtil/loadSkeleton scene)]
    bones))

(defn load-scene [resource]
  (let [mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        scene (create-scene resource)
        bones (ModelUtil/loadSkeleton scene)
        animation-ids (ArrayList.)]
    (when-not (empty? bones)
      (ModelUtil/skeletonToDDF bones skeleton-builder))
    (ModelUtil/loadMeshes scene mesh-set-builder)
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :animation-ids animation-ids})))


(set! *warn-on-reflection* true)

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
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.bob.pipeline ModelUtil]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$Skeleton]
           [java.util ArrayList]))

(set! *warn-on-reflection* false)

(defn create-scene [resource]
  (let [stream (io/input-stream resource)
        scene (ModelUtil/loadScene stream (:ext resource))]
    scene))

;; (defn load-skeleton [resource]
;;   (let [stream (io/input-stream resource)
;;         scene (ModelUtil/loadScene stream)
;;         skeleton-builder (Rig$Skeleton/newBuilder)
;;         bones (ModelUtil/loadSkeleton scene)
;;         _ (ModelUtil/loadSkeleton scene skeleton-builder)
;;         skeleton (protobuf/pb->map (.build skeleton-builder))]
;;     {:skeleton-pb skeleton
;;      :bones bones}))


(defn load-bones [resource]
  (let [scene (create-scene resource)
        bones (ModelUtil/loadSkeleton scene)]
    bones))

;; (defn load-animations [resource bones]
;;   (let [scene (create-scene resource)
;;         animation-ids (ArrayList.)
;;         animation-set-builder (Rig$AnimationSet/newBuilder)
;;         bones (ModelUtil/loadSkeleton scene)]
;;     (ModelUtil/loadAnimations scene bones animation-set-builder "" animation-ids)
;;     (let [animation-set (protobuf/pb->map (.build animation-set-builder))
;;           animation-set-with-string-ids (update animation-set :animations (partial mapv #(assoc %2 :id %1) animation-ids))]
;;       {:animation-set-pb animation-set-with-string-ids
;;        :animation-ids animation-ids
;;        :bones bones})))

;; (defn set-bone-indices [animation-set bones]
;;   (ModelUtil/setBoneIndices animation-set bones))

;; (defn load-meshes [^InputStream stream bones]
;;   (let [scene (ModelUtil/loadScene stream)
;;         mesh-set-builder (Rig$MeshSet/newBuilder)
;;         _ (ModelUtil/loadMeshes scene mesh-set-builder)
;;         mesh-set (protobuf/pb->map (.build mesh-set-builder))
;;         ]
;;     {:mesh-set-pb mesh-set}))

(defn load-scene [resource]
  (let [animation-set-builder (Rig$AnimationSet/newBuilder)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        scene (create-scene resource)
        bones (ModelUtil/loadSkeleton scene)
        animation-ids (ArrayList.)]
    (prn "MAWE load-scene " resource bones)
    (ModelUtil/loadSkeleton scene skeleton-builder)
    (ModelUtil/loadAnimations scene bones animation-set-builder "" animation-ids)
    (ModelUtil/loadMeshes scene mesh-set-builder)
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))
          animation-set (protobuf/pb->map (.build animation-set-builder))
          ; we take a strict protobuf map, with id of type uint64, and convert it to type string
          ; we'll partch the info back later in the process when we know what the actual end id should be
          animation-set-with-string-ids (update animation-set :animations (partial mapv #(assoc %2 :id %1) animation-ids))]
      {:animation-set animation-set-with-string-ids
       :mesh-set mesh-set
       :skeleton skeleton
       :bones bones})))


(set! *warn-on-reflection* true)

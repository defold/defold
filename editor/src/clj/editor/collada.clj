;; Copyright 2020 The Defold Foundation
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

(ns editor.collada
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline ColladaUtil]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$Skeleton]
           [java.io InputStream]
           [java.util ArrayList]))

(set! *warn-on-reflection* true)

(defn load-scene [^InputStream stream]
  (let [animation-set-builder (Rig$AnimationSet/newBuilder)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        collada-doc (ColladaUtil/loadDAE stream)
        bone-ids (ArrayList.)
        animation-ids (ArrayList.)]
    (ColladaUtil/loadMesh collada-doc mesh-set-builder false)
    (ColladaUtil/loadSkeleton collada-doc skeleton-builder bone-ids)
    (ColladaUtil/loadAnimations collada-doc animation-set-builder "" animation-ids)
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))
          animation-set (protobuf/pb->map (.build animation-set-builder))
          animation-set-with-string-ids (update animation-set :animations (partial mapv #(assoc %2 :id %1) animation-ids))]
      {:animation-set animation-set-with-string-ids
       :mesh-set mesh-set
       :skeleton skeleton})))

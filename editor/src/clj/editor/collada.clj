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
    (ColladaUtil/loadMesh collada-doc mesh-set-builder)
    (ColladaUtil/loadSkeleton collada-doc skeleton-builder bone-ids)
    (ColladaUtil/loadAnimations collada-doc animation-set-builder "" animation-ids)
    (let [mesh-set (protobuf/pb->map (.build mesh-set-builder))
          skeleton (protobuf/pb->map (.build skeleton-builder))
          animation-set (protobuf/pb->map (.build animation-set-builder))
          animation-set-with-string-ids (update animation-set :animations (partial mapv #(assoc %2 :id %1) animation-ids))]
      {:animation-set animation-set-with-string-ids
       :mesh-set mesh-set
       :skeleton skeleton})))

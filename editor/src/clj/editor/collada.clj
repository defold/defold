(ns editor.collada
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline ColladaUtil]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$MeshSet Rig$Skeleton]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn load-scene [^InputStream stream]
  (let [animation-set-builder (Rig$AnimationSet/newBuilder)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)]
    (ColladaUtil/load stream mesh-set-builder animation-set-builder skeleton-builder)
    {:animation-set (protobuf/pb->map (.build animation-set-builder))
     :mesh-set (protobuf/pb->map (.build mesh-set-builder))
     :skeleton (protobuf/pb->map (.build skeleton-builder))}))

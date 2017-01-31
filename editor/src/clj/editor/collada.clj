(ns editor.collada
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline ColladaUtil]
           [com.dynamo.rig.proto Rig$MeshSet]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn ->mesh-set [^InputStream stream]
  (let [mesh-set-builder (Rig$MeshSet/newBuilder)]
    (ColladaUtil/loadMesh stream mesh-set-builder)
    (protobuf/pb->map (.build mesh-set-builder))))

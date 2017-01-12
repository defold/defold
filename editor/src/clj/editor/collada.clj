(ns editor.collada
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline ColladaUtil]
           [com.dynamo.rig.proto Rig$MeshSet]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn- sequence-buffer-in-mesh [mesh data-stride data-key index-key]
  (let [partitioned-data (vec (partition data-stride (mesh data-key)))
        sequenced-data (into [] (mapcat #(partitioned-data %) (mesh index-key)))
        sequenced-indices (range (/ (count sequenced-data) data-stride))]
    (assoc mesh
      data-key sequenced-data
      index-key sequenced-indices)))

(defn- sequence-vertices-in-mesh [mesh]
  (-> mesh
      (sequence-buffer-in-mesh 3 :positions :indices)
      (sequence-buffer-in-mesh 3 :normals :normals-indices)
      (sequence-buffer-in-mesh 2 :texcoord0 :texcoord0-indices)))

(defn- sequence-vertices-in-mesh-entry [mesh-entry]
  (update mesh-entry :meshes #(mapv sequence-vertices-in-mesh %)))

(defn- sequence-vertices-in-mesh-set [mesh-set]
  (update mesh-set :mesh-entries #(mapv sequence-vertices-in-mesh-entry %)))

(defn ->mesh-set [^InputStream stream]
  (let [mesh-set-builder (Rig$MeshSet/newBuilder)]
    (ColladaUtil/loadMesh stream mesh-set-builder)
    (let [unprocessed-data (protobuf/pb->map (.build mesh-set-builder))]
      (sequence-vertices-in-mesh-set unprocessed-data))))

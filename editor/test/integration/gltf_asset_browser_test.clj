;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns integration.gltf-asset-browser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.asset-browser :as asset-browser]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [javafx.scene.control TreeItem]))

(def ^:private geometry-buffer-base64
  "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA")

(def ^:private image-base64
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Y9Z6L8AAAAASUVORK5CYII=")

(defn- scene-json [material-name buffer-json]
  (str "{"
       "\"asset\":{\"version\":\"2.0\"},"
       "\"scene\":0,"
       "\"scenes\":[{\"nodes\":[0]}],"
       "\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],"
       "\"meshes\":["
       "{\"name\":\"mymesh\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]},"
       "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"Shared\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"Shared\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"   \",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"bad/name\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
       "\"buffers\":[" buffer-json "],"
       "\"bufferViews\":["
       "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
       "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
       "\"accessors\":["
       "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
       "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
       "\"images\":[{\"name\":\"Albedo\",\"uri\":\"data:image/png;base64,"
       image-base64
       "\"}],"
       "\"textures\":[{\"name\":\"AlbedoTexture\",\"source\":0}],"
       "\"materials\":[{\"name\":\""
       material-name
       "\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}]}"))

(defn- gltf-content [material-name]
  (scene-json material-name
              (str "{\"uri\":\"data:application/octet-stream;base64,"
                   geometry-buffer-base64
                   "\",\"byteLength\":42}")))

(defn- glb-content
  ^bytes [material-name]
  (let [^bytes geometry-bytes (.decode (Base64/getDecoder) geometry-buffer-base64)
        ^bytes json-bytes (.getBytes ^String (scene-json material-name "{\"byteLength\":42}")
                                     StandardCharsets/UTF_8)
        padded-json-length (bit-and (+ (alength json-bytes) 3) (bit-not 3))
        padded-geometry-length (bit-and (+ (alength geometry-bytes) 3) (bit-not 3))
        glb-length (+ 12 8 padded-json-length 8 padded-geometry-length)
        ^ByteBuffer glb (doto (ByteBuffer/allocate glb-length)
                          (.order ByteOrder/LITTLE_ENDIAN))]
    (.putInt glb 0x46546c67)
    (.putInt glb 2)
    (.putInt glb glb-length)
    (.putInt glb padded-json-length)
    (.putInt glb 0x4e4f534a)
    (.put glb json-bytes)
    (while (< (.position glb) (+ 20 padded-json-length))
      (.put glb (byte 32)))
    (.putInt glb padded-geometry-length)
    (.putInt glb 0x004e4942)
    (.put glb geometry-bytes)
    (.array glb)))

(defn- tree-item-proj-paths [^TreeItem root]
  (loop [remaining [root]
         proj-paths #{}]
    (if-let [^TreeItem tree-item (peek remaining)]
      (recur (into (pop remaining) (.getChildren tree-item))
             (conj proj-paths (resource/proj-path (.getValue tree-item))))
      proj-paths)))

(def ^:private expected-mesh-resource-names
  #{"mymesh"
    "Mesh 1"
    "Shared [2]"
    "Shared [3]"
    "Mesh 4"
    "Mesh 5"})

(defn- expected-tree-proj-paths [source-proj-path]
  (into #{source-proj-path
          (str source-proj-path "/images")
          (str source-proj-path "/images/0.png")
          (str source-proj-path "/materials")
          (str source-proj-path "/materials/0.material")
          (str source-proj-path "/meshes")}
        (map #(str source-proj-path "/meshes/" %))
        expected-mesh-resource-names))

(deftest gltf-containers-are-expandable-in-assets-view
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        models-directory (io/file project-path "models")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! (io/file models-directory "robot.gltf") (gltf-content "GltfPaint"))
      (fs/create-file! (io/file models-directory "robot.glb") (glb-content "GlbPaint"))
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]
          (doseq [[source-proj-path material-label]
                  [["/models/robot.gltf" "GltfPaint [0].material"]
                   ["/models/robot.glb" "GlbPaint [0].material"]]]
            (testing source-proj-path
              (let [source-resource (workspace/find-resource workspace source-proj-path)
                    image-resource (workspace/find-resource workspace (str source-proj-path "/images/0.png"))
                    material-resource (workspace/find-resource workspace (str source-proj-path "/materials/0.material"))
                    meshes-resource (workspace/find-resource workspace (str source-proj-path "/meshes"))]
                (is (some? source-resource))
                (is (= "Albedo [0].png" (asset-browser/resource-tree-cell-text image-resource)))
                (is (= material-label (asset-browser/resource-tree-cell-text material-resource)))
                (is (some? meshes-resource))
                (when (and source-resource meshes-resource)
                  (let [mesh-resources (resource/children meshes-resource)
                        ^TreeItem source-tree-item (asset-browser/tree-item source-resource)
                        ^TreeItem meshes-tree-item (asset-browser/tree-item meshes-resource)]
                    (is (false? (.isLeaf source-tree-item)))
                    (is (= (expected-tree-proj-paths source-proj-path)
                           (tree-item-proj-paths source-tree-item)))
                    (is (false? (.isLeaf meshes-tree-item)))
                    (is (= expected-mesh-resource-names
                           (into #{} (map resource/resource-name) mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count (.getChildren meshes-tree-item))))
                    (doseq [^TreeItem mesh-tree-item (.getChildren meshes-tree-item)]
                      (is (.isLeaf mesh-tree-item)))
                    (doseq [mesh-resource mesh-resources]
                      (is (resource/gltf-resource? mesh-resource))
                      (is (= (resource/resource-name mesh-resource)
                             (asset-browser/resource-tree-cell-text mesh-resource)))
                      (is (= :file (resource/source-type mesh-resource)))
                      (is (resource/read-only? mesh-resource))
                      (is (false? (resource/openable? mesh-resource)))
                      (is (= "icons/32/Icons_27-AT-Mesh.png"
                             (workspace/resource-icon mesh-resource))))))))))))))

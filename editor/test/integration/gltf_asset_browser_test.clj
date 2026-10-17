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

(ns integration.gltf-asset-browser-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.fs :as fs]
            [editor.handler :as handler]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [com.dynamo.gamesys.proto ModelProto$Model ModelProto$ModelDesc]
           [java.io File]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [javafx.scene.control TreeItem]))

(set! *warn-on-reflection* true)

(def ^:private ^String geometry-buffer-base64
  "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA")

(def ^:private image-base64
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAC0lEQVR4nGP4DwQACfsD/fteaysAAAAASUVORK5CYII=")

(defn- scene-json [material-name buffer-json]
  (str "{"
       "\"asset\":{\"version\":\"2.0\"},"
       "\"scene\":0,"
       "\"scenes\":[{\"nodes\":[0]}],"
       "\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],"
       "\"meshes\":["
       "{\"name\":\"mymesh\",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]},"
       "{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"Shared\",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"Shared\",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"   \",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]},"
       "{\"name\":\"bad/name\",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2},\"indices\":1,\"material\":0}]}],"
       "\"buffers\":[" buffer-json "],"
       "\"bufferViews\":["
       "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
       "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
       "\"accessors\":["
       "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
       "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},{\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}],"
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
  #{"mymesh_0"
    "Shared_0"
    "Shared_1"
    "bad_name_0"})

(defn- expected-tree-proj-paths [source-proj-path material-resource-name]
  (into #{source-proj-path
          (str source-proj-path "/images")
          (str source-proj-path "/images/Albedo_0.png")
          (str source-proj-path "/materials")
          (str source-proj-path "/materials/" material-resource-name)
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
        (let [workspace (test-util/setup-workspace! project-path)]
          (doseq [[source-proj-path material-resource-name]
                  [["/models/robot.gltf" "GltfPaint_0.material"]
                   ["/models/robot.glb" "GlbPaint_0.material"]]]
            (testing source-proj-path
              (let [source-resource (workspace/find-resource workspace source-proj-path)
                    image-resource (workspace/find-resource workspace (str source-proj-path "/images/Albedo_0.png"))
                    material-resource (workspace/find-resource workspace
                                                               (str source-proj-path "/materials/" material-resource-name))
                    meshes-resource (workspace/find-resource workspace (str source-proj-path "/meshes"))]
                (is (some? source-resource))
                (is (= "Albedo_0.png" (resource/resource-name image-resource)))
                (is (= material-resource-name (resource/resource-name material-resource)))
                (is (some? meshes-resource))
                (when (and source-resource meshes-resource)
                  (let [mesh-resources (resource/children meshes-resource)
                        ^TreeItem source-tree-item (asset-browser/tree-item source-resource)
                        ^TreeItem meshes-tree-item (asset-browser/tree-item meshes-resource)]
                    (is (false? (.isLeaf source-tree-item)))
                    (is (= (expected-tree-proj-paths source-proj-path material-resource-name)
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
                      (is (= :file (resource/source-type mesh-resource)))
                      (is (resource/read-only? mesh-resource))
                      (is (true? (resource/openable? mesh-resource)))
                      (is (= "icons/32/Icons_27-AT-Mesh.png"
                             (workspace/resource-icon mesh-resource))))))))))))))

(defn- with-glb-project
  "Loads a GLB and a model referencing its embedded material."
  [f]
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        source-file (io/file project-path "robot.glb")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! source-file (glb-content "Paint/Chrome"))
      (fs/create-file! (io/file project-path "robot.model")
                       "mesh: \"/robot.glb\"\nmaterials { name: \"Paint/Chrome\" material: \"/robot.glb/materials/Paint_Chrome_0.material\" }\n")
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)]
          (f workspace (test-util/setup-project! workspace) source-file))))))

(defn- copy-selection! [selection]
  (let [copied-files (atom nil)]
    (with-redefs-fn {#'asset-browser/copy #(reset! copied-files %)}
      #(test-util/handler-run :edit.copy
                              [(handler/->context :asset-browser {:selection selection})]
                              {}))
    @copied-files))

(deftest copying-an-embedded-material-preserves-its-resource-name-and-content
  (with-glb-project
    (fn [workspace _project _source-file]
      (let [material (workspace/find-resource workspace "/robot.glb/materials/Paint_Chrome_0.material")
            ^File exported (first (copy-selection! [material]))]
        (with-open [_deleter (test-util/make-directory-deleter (.getParentFile exported))]
          (is (= "Paint_Chrome_0.material" (resource/resource-name material) (.getName exported)))
          (is (= (slurp material) (slurp exported))))))))

(deftest embedded-meshes-can-be-copied
  (with-glb-project
    (fn [workspace _project _source-file]
      (doseq [[paths copyable]
              [[[] false]
               [["/robot.glb"] true]
               [["/robot.glb/materials/Paint_Chrome_0.material"] true]
               [["/robot.glb/meshes"] true]
               [["/robot.glb/meshes/Shared_0"] true]
               [["/robot.glb" "/robot.glb/materials/Paint_Chrome_0.material"] true]
               [["/robot.glb/materials/Paint_Chrome_0.material" "/robot.glb/meshes/Shared_0"] true]]]
        (let [selection (mapv #(workspace/find-resource workspace %) paths)]
          (is (= copyable
                 (test-util/handler-enabled? :edit.copy
                                             [(handler/->context :asset-browser {:selection selection})]
                                             {}))))))))

;; Verifies exported glTF meshes preview and build using built-in shaders, including the default mesh index zero.
(deftest copying-meshes-exports-buildable-models
  (with-glb-project
    (fn [workspace project source-file]
      (let [meshes (workspace/find-resource workspace "/robot.glb/meshes")
            ^File exported-directory (first (copy-selection! [meshes]))
            project-directory (.getParentFile ^File source-file)]
        (with-open [_deleter (test-util/make-directory-deleter (.getParentFile exported-directory))]
          (is (= (into #{} (map #(str % ".model")) expected-mesh-resource-names)
                 (set (.list exported-directory))))
          (doseq [^File exported-file (.listFiles exported-directory)]
            (let [model-desc (protobuf/read-map-without-defaults ModelProto$ModelDesc exported-file)]
              (is (= "/robot.glb" (:mesh model-desc)))
              (is (= [{:name "Paint/Chrome"
                       :material "/robot.glb/materials/Paint_Chrome_0.material"
                       :textures [{:sampler "PbrMetallicRoughness_baseColorTexture"
                                   :texture "/robot.glb/images/Albedo_0.png"}]}]
                     (:materials model-desc)))
              (io/copy exported-file (io/file project-directory (.getName exported-file)))))
          (workspace/resource-sync! workspace)
          (doseq [[index name mesh-name] [[0 "mymesh_0" "mymesh"]
                                          [2 "Shared_0" "Shared"]
                                          [3 "Shared_1" "Shared"]
                                          [5 "bad_name_0" "bad/name"]]]
            (testing name
              (let [node (test-util/resource-node project (str "/" name ".model"))
                    save-value (g/node-value node :save-value)]
                (is (= index (test-util/prop node :mesh-index)))
                (is (= mesh-name (:mesh-name save-value)))
                (is (= [index] (into [] (keep :mesh-index) (:children (g/node-value node :scene)))))
                (with-open [_build (test-util/build! node)]
                  (is (= index (:mesh-index (protobuf/bytes->map-with-defaults ModelProto$Model
                                                                               (test-util/node-build-output node))))))))))))))

(deftest copied-mesh-zero-with-duplicate-names-survives-reload
  (let [original-scene-json scene-json]
    (with-redefs [scene-json (fn [material-name buffer-json]
                               (string/replace (original-scene-json material-name buffer-json)
                                               "\"name\":\"mymesh\"" "\"name\":\"Shared\""))]
      (with-glb-project
        (fn [workspace project source-file]
          (let [mesh (workspace/find-resource workspace "/robot.glb/meshes/Shared_0")
                project-directory (.getParentFile ^File source-file)]
            (doseq [[name content] [["explicit" (slurp mesh)]
                                    ["omitted" (string/replace (slurp mesh) "mesh_index: 0\n" "")]]]
              (testing name
                (fs/create-file! (io/file project-directory (str name ".model")) content)
                (workspace/resource-sync! workspace)
                (let [node (test-util/resource-node project (str "/" name ".model"))
                      save-value (g/node-value node :save-value)
                      reloaded-path (str "/" name "-reloaded.model")]
                  (is (= 0 (g/raw-property-value (g/now) node :mesh-index)))
                  (is (= 0 (test-util/prop node :mesh-index)))
                  (is (= [0] (into [] (keep :mesh-index) (:children (g/node-value node :scene)))))
                  (fs/create-file! (io/file project-directory (subs reloaded-path 1))
                                   (protobuf/map->str ModelProto$ModelDesc save-value))
                  (workspace/resource-sync! workspace)
                  (let [reloaded-node (test-util/resource-node project reloaded-path)]
                    (is (= 0 (test-util/prop reloaded-node :mesh-index)))
                    (with-open [_build (test-util/build! reloaded-node)]
                      (is (= 0 (:mesh-index (protobuf/bytes->map-with-defaults ModelProto$Model
                                                                               (test-util/node-build-output reloaded-node))))))))))))))))

(deftest an-unreferenced-mesh-has-a-read-only-preview
  (with-glb-project
    (fn [workspace project _source-file]
      (let [mesh (workspace/find-resource workspace "/robot.glb/meshes/Shared_0")
            node (test-util/resource-node project (resource/proj-path mesh))
            scene (g/node-value node :scene)]
        (is (resource/editor-openable-resource? mesh))
        (is (resource/read-only? mesh))
        (is (= [:scene] (mapv :id (workspace/resource-view-types mesh))))
        (is (not (g/error-value? scene)))
        (is (= node (:node-id scene)))
        (is (= [2] (into [] (keep :mesh-index) (:children scene))))))))

(deftest deleting-a-container-reports-missing-embedded-references
  (with-glb-project
    (fn [workspace project source-file]
      (let [model (test-util/resource-node project "/robot.model")]
        (fs/delete-file! source-file)
        (workspace/resource-sync! workspace)
        (is (nil? (workspace/find-resource workspace "/robot.glb/materials/Paint_Chrome_0.material")))
        (is (g/error-value? (g/node-value model :scene)))))))

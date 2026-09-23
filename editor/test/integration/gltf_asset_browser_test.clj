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
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.game-project :as game-project]
            [editor.handler :as handler]
            [editor.notifications :as notifications]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support :refer [with-clean-system]])
  (:import [java.io File]
           [java.net URI]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.scene Scene]
           [javafx.scene.control TreeItem]
           [javafx.scene.layout VBox]))

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
        (let [workspace (test-util/setup-workspace! project-path)]
          (doseq [[source-proj-path material-label]
                  [["/models/robot.gltf" "GltfPaint [0].material"]
                   ["/models/robot.glb" "GlbPaint [0].material"]]]
            (testing source-proj-path
              (let [source-resource (workspace/find-resource workspace source-proj-path)
                    image-resource (workspace/find-resource workspace (str source-proj-path "/images/0.png"))
                    material-resource (workspace/find-resource workspace (str source-proj-path "/materials/0.material"))
                    meshes-resource (workspace/find-resource workspace (str source-proj-path "/meshes"))]
                (is (some? source-resource))
                (is (= "Albedo [0].png" (resource/resource-name image-resource)))
                (is (= material-label (resource/resource-name material-resource)))
                (is (= "Albedo [0].png" (#'app-view/tab-title image-resource false)))
                (is (= material-label (#'app-view/tab-title material-resource false)))
                (is (some? meshes-resource))
                (when (and source-resource meshes-resource)
                  (let [mesh-resources (resource/children meshes-resource)
                        ^TreeItem source-tree-item (asset-browser/tree-item source-resource)
                        ^TreeItem meshes-tree-item (asset-browser/tree-item meshes-resource)]
                    (is (false? (.isLeaf source-tree-item)))
                    (is (= (expected-tree-proj-paths source-proj-path)
                           (tree-item-proj-paths source-tree-item)))
                    (is (false? (.isLeaf meshes-tree-item)))
                    (is (= (into #{}
                                 (map #(str (resource/resource-name source-resource) " : " %))
                                 expected-mesh-resource-names)
                           (into #{} (map resource/resource-name) mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count (.getChildren meshes-tree-item))))
                    (doseq [^TreeItem mesh-tree-item (.getChildren meshes-tree-item)]
                      (is (.isLeaf mesh-tree-item)))
                    (doseq [mesh-resource mesh-resources]
                      (is (= (resource/resource-name mesh-resource)
                             (#'app-view/tab-title mesh-resource false)))
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
                       "mesh: \"/robot.glb\"\nmaterials { name: \"Paint/Chrome\" material: \"/robot.glb/materials/0.material\" }\n")
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)]
          (f workspace (test-util/setup-project! workspace) source-file))))))

(deftest copying-an-embedded-material-preserves-its-name-and-content
  (with-glb-project
    (fn [workspace _project _source-file]
      (let [material (workspace/find-resource workspace "/robot.glb/materials/0.material")
            ^File exported (first (#'asset-browser/fileify-resources! [material]))]
        (with-open [_deleter (test-util/make-directory-deleter (.getParentFile exported))]
          (is (= "Paint_Chrome [0].material" (resource/resource-name material) (.getName exported)))
          (is (= (slurp material) (slurp exported))))))))

(deftest metadata-meshes-cannot-be-copied-as-empty-files
  (with-glb-project
    (fn [workspace _project _source-file]
      (doseq [[paths copyable]
              [[[] false]
               [["/robot.glb"] true]
               [["/robot.glb/materials/0.material"] true]
               [["/robot.glb/meshes"] false]
               [["/robot.glb/meshes/Mesh 1"] false]
               [["/robot.glb" "/robot.glb/materials/0.material"] true]
               [["/robot.glb/materials/0.material" "/robot.glb/meshes/Mesh 1"] false]]]
        (let [selection (mapv #(workspace/find-resource workspace %) paths)]
          (is (= copyable
                 (test-util/handler-enabled? :edit.copy
                                             [(handler/->context :asset-browser {:selection selection})]
                                             {}))))))))

(deftest an-unreferenced-mesh-has-a-read-only-preview
  (with-glb-project
    (fn [workspace project _source-file]
      (let [mesh (workspace/find-resource workspace "/robot.glb/meshes/Mesh 1")
            node (test-util/resource-node project (resource/proj-path mesh))
            scene (g/node-value node :scene)]
        (is (resource/editor-openable-resource? mesh))
        (is (resource/read-only? mesh))
        (is (= [:scene] (mapv :id (workspace/resource-view-types mesh))))
        (is (not (g/error-value? scene)))
        (is (= node (:node-id scene)))
        (is (= [1] (into [] (keep :mesh-index) (:children scene))))))))

(deftest deleting-a-container-reports-missing-embedded-references
  (with-glb-project
    (fn [workspace project source-file]
      (let [model (test-util/resource-node project "/robot.model")]
        (fs/delete-file! source-file)
        (workspace/resource-sync! workspace)
        (is (nil? (workspace/find-resource workspace "/robot.glb/materials/0.material")))
        (is (g/error-value? (g/node-value model :scene)))))))

(deftest pbr-library-notification-follows-imports-and-dependencies
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)]
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              project (test-util/setup-project! workspace)
              notifications (workspace/notifications workspace)
              notification #(get-in (g/node-value notifications :notifications)
                                    [:id->notification ::project/pbr-library])
              game-project (test-util/resource-node project "/game.project")
              dependencies (project/project-dependencies project)]
          (is (nil? (notification)) "Built-in glTF materials do not trigger the offer")
          (fs/create-file! (io/file project-path "first.gltf") (gltf-content "Paint"))
          (fs/create-file! (io/file project-path "second.glb") (glb-content "Paint"))
          (workspace/resource-sync! workspace)
          (is (= :info (:type (notification))))
          (is (= "Add Library" (test-util/localization (get-in (notification) [:actions 0 :message]))))
          (let [command-call (atom nil)]
            (with-redefs [ui/main-scene (constantly (Scene. (VBox.)))
                          ui/execute-command (fn [_ command user-data]
                                               (reset! command-call [command user-data]))]
              ((get-in (notification) [:actions 0 :on-action])))
            (is (= [:private/add-dependency {:dep-url "https://github.com/defold/asset-pbr/archive/refs/heads/master.zip"}]
                   @command-call)))
          (is (= 1 (count (:ids (g/node-value notifications :notifications)))))
          (is (= dependencies (project/project-dependencies project)))

          (notifications/close! notifications ::project/pbr-library)
          (workspace/resource-sync! workspace)
          (is (nil? (notification)))
          (fs/create-file! (io/file project-path "note.txt") "Unrelated file")
          (workspace/resource-sync! workspace)
          (is (nil? (notification)) "Unrelated resource changes preserve dismissal")

          (fs/create-file! (io/file project-path "third.gltf") (gltf-content "Paint"))
          (workspace/resource-sync! workspace)
          (is (some? (notification)) "A new import offers the library again")
          (fs/create-file! (io/file project-path "fourth.gltf") (gltf-content "Paint"))
          (workspace/resource-sync! workspace)
          (is (= 1 (count (:ids (g/node-value notifications :notifications)))))

          (game-project/set-setting! game-project ["project" "dependencies"] ["https://github.com/defold/asset-pbr/archive/0123456789abcdef0123456789abcdef01234567.zip"])
          (is (nil? (notification)) "Pending dependency changes defer to the Fetch Libraries notification")
          (fs/create-file! (io/file project-path "fifth.gltf") (gltf-content "Paint"))
          (workspace/resource-sync! workspace)
          (is (nil? (notification)) "Imports do not offer another library while dependencies await fetching")

          (game-project/set-setting! game-project ["project" "dependencies"] [])
          (is (nil? (notification)) "Removing the dependency alone does not show an import offer")
          (fs/create-file! (io/file project-path "sixth.gltf") (gltf-content "Paint"))
          (workspace/resource-sync! workspace)
          (is (some? (notification))))))))

(deftest pbr-library-notification-checks-shader-resources
  (doseq [origin [:file :zip]]
    (testing origin
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
            library-uri (URI/create "https://github.com/defold/asset-pbr/archive/0123456789abcdef0123456789abcdef01234567.zip")
            shader-paths ["defold-pbr/shaders/pbr.vp" "defold-pbr/shaders/pbr.fp"]]
        (with-open [_deleter (test-util/make-directory-deleter project-path)]
          (test-util/with-project-default-library-directory
            (if (= :file origin)
              (doseq [path shader-paths]
                (fs/create-parent-directories! (io/file project-path path))
                (fs/create-file! (io/file project-path path) "void main() {}"))
              (let [archive (test-support/library-file (io/file project-path) library-uri "")]
                (fs/create-parent-directories! archive)
                (with-open [out (ZipOutputStream. (io/output-stream archive))]
                  (doseq [[path content] [["game.project" "[library]\ninclude_dirs = defold-pbr\n"]
                                         [(shader-paths 0) "void main() {}"]
                                         [(shader-paths 1) "void main() {}"]]]
                    (.putNextEntry out (ZipEntry. path))
                    (.write out (.getBytes ^String content StandardCharsets/UTF_8))
                    (.closeEntry out)))
                (fs/create-file! (io/file project-path "game.project")
                                 (str "[project]\ndependencies = " library-uri "\n"))))
            (with-clean-system
              (let [workspace (test-util/setup-workspace! project-path)]
                (when (= :zip origin)
                  (test-util/set-cached-project-dependencies! workspace [library-uri])
                  (workspace/resource-sync! workspace))
                (let [project (test-util/setup-project! workspace)
                      notifications (workspace/notifications workspace)
                      notification #(get-in (g/node-value notifications :notifications)
                                            [:id->notification ::project/pbr-library])]
                  (is (= (set (project/project-dependencies project))
                         (set (workspace/dependencies workspace)))
                      "Dependencies are fetched; the check must use their shader resources")
                  (fs/create-file! (io/file project-path "robot.gltf") (gltf-content "Paint"))
                  (workspace/resource-sync! workspace)
                  (is (nil? (notification)) "Either local or library shaders satisfy the requirement")
                  (when (= :file origin)
                    (fs/delete-file! (io/file project-path (shader-paths 1)))
                    (fs/create-file! (io/file project-path "second.gltf") (gltf-content "Paint"))
                    (workspace/resource-sync! workspace)
                    (is (some? (notification)) "Both shaders are required")
                    (fs/create-file! (io/file project-path (shader-paths 1)) "void main() {}")
                    (workspace/resource-sync! workspace)
                    (is (nil? (notification)) "Making the shaders available clears the offer")))))))))))

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
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.gltf-ui :as gltf-ui]
            [editor.handler :as handler]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [java.io File]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
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
                (is (= "Albedo [0].png" (resource/display-name image-resource)))
                (is (= material-label (resource/display-name material-resource)))
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
                    (is (= expected-mesh-resource-names
                           (into #{} (map resource/resource-name) mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count mesh-resources)))
                    (is (= (count expected-mesh-resource-names)
                           (count (.getChildren meshes-tree-item))))
                    (doseq [^TreeItem mesh-tree-item (.getChildren meshes-tree-item)]
                      (is (.isLeaf mesh-tree-item)))
                    (doseq [mesh-resource mesh-resources]
                      (is (= (resource/resource-name mesh-resource)
                             (resource/display-name mesh-resource)))
                      (is (= (str (resource/resource-name source-resource) " : " (resource/resource-name mesh-resource))
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
        (let [workspace (test-util/setup-workspace! world project-path)]
          (f workspace (test-util/setup-project! workspace) source-file))))))

(deftest copying-an-embedded-material-preserves-its-name-and-content
  (with-glb-project
    (fn [workspace _project _source-file]
      (let [material (workspace/find-resource workspace "/robot.glb/materials/0.material")
            ^File exported (first (#'asset-browser/fileify-resources! [material]))]
        (with-open [_deleter (test-util/make-directory-deleter (.getParentFile exported))]
          (is (= "Paint_Chrome [0].material" (.getName exported)))
          (is (= (slurp material) (slurp exported))))))))

(deftest metadata-meshes-cannot-be-copied-as-empty-files
  (with-glb-project
    (fn [workspace _project _source-file]
      (doseq [[path copyable] [["/robot.glb" true]
                               ["/robot.glb/materials/0.material" true]
                               ["/robot.glb/meshes" false]
                               ["/robot.glb/meshes/Mesh 1" false]]]
        (is (= copyable (asset-browser/copyable-resource? (workspace/find-resource workspace path))))))))

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

(deftest adding-gltf-offers-the-pbr-library-once
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! (io/file project-path "existing.glb") (glb-content "Paint"))
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              prompts (atom [])
              dependencies (project/project-dependencies project)]
          ;; Substitute only the modal dialog boundary; exercise normal resource sync.
          (with-redefs [dialogs/make-confirmation-dialog
                        (fn [_ props]
                          (swap! prompts conj props)
                          false)]
            (test-util/with-ui-run-later-rebound
              (gltf-ui/register-resource-listener! workspace project test-util/localization))
            (is (coll/empty? @prompts))
            (test-util/with-ui-run-later-rebound
              (fs/create-file! (io/file project-path "first.gltf") (gltf-content "Paint"))
              (fs/create-file! (io/file project-path "second.glb") (glb-content "Paint"))
              (workspace/resource-sync! workspace))
            (is (= 1 (count @prompts)))
            (is (= dependencies (project/project-dependencies project)))
            (let [message (test-util/localization (:content (first @prompts)))]
              (is (string/includes? message gltf-ui/pbr-library-url))
              (is (string/includes? message "\n")))
            (test-util/with-ui-run-later-rebound
              (fs/create-file! (io/file project-path "note.txt") "Unrelated file")
              (workspace/resource-sync! workspace))
            (is (= 1 (count @prompts)))
            (test-util/set-setting! (test-util/resource-node project "/game.project")
                                    ["project" "dependencies"] [gltf-ui/pbr-library-url])
            (gltf-ui/offer-pbr-library! project test-util/localization
                                        [(workspace/find-resource workspace "/first.gltf")])
            (is (= 1 (count @prompts)))))))))

(deftest accepting-the-pbr-library-prompt-adds-and-fetches-the-dependency
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)
                library-server (http-server/start! test-util/lib-server-handler)
                editor-server (http-server/start! (web-server/make-dynamic-handler []))]
      (fs/create-file! (io/file project-path "robot.gltf") (gltf-content "Paint"))
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              app-view (test-util/setup-app-view! project)
              library-url (test-util/lib-server-uri library-server "lib_resource_project")
              scene (Scene. (VBox.))
              completion (promise)]
          (ui/context! (.getRoot scene) :global
                       {:app-view app-view
                        :workspace workspace
                        :project project
                        :changes-view nil
                        :build-errors-view nil
                        :prefs (test-util/make-test-prefs)
                        :localization test-util/localization
                        :web-server editor-server}
                       (reify handler/SelectionProvider
                         (selection [_this _evaluation-context] [])
                         (succeeding-selection [_this _evaluation-context] [])
                         (alt-selection [_this _evaluation-context] [])))
          ;; Supply UI boundaries and a local HTTP fixture; run the real add/fetch command.
          (with-redefs [dialogs/make-confirmation-dialog (fn [_ _] true)
                        gltf-ui/pbr-library-url library-url
                        app-view/make-render-task-progress (constantly progress/null-render-progress!)
                        ui/main-scene (constantly scene)]
            (test-util/run-event-loop!
              (fn [exit!]
                (let [fetch (gltf-ui/offer-pbr-library! project test-util/localization
                                                        [(workspace/find-resource workspace "/robot.gltf")])]
                  (future
                    (deliver completion
                             (try
                               (deref fetch 30000 ::timeout)
                               (catch Throwable error error)))
                    (exit!))))))
          (is (vector? @completion) (str @completion))
          (when (vector? @completion)
            (is (true? (second @completion))))
          (is (= [library-url] (mapv str (project/project-dependencies project))))
          (is (resource/exists? (workspace/find-resource workspace "/lib_resource_project/simple.gui"))))))))

(deftest unsupported-gltf-images-produce-visible-warnings
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        source-file (io/file project-path "unsupported.gltf")
        content (gltf-content "Paint")
        unsupported-content (string/replace content "data:image/png" "data:image/avif")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! source-file unsupported-content)
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              notifications (workspace/notifications workspace)
              warnings (fn []
                         (into []
                               (comp (filter #(= :warning (:type %)))
                                     (map #(test-util/localization (:message %))))
                               (coll/vals (:id->notification (g/node-value notifications :notifications)))))]
          (test-util/with-ui-run-later-rebound
            (gltf-ui/register-resource-listener! workspace project test-util/localization))
          (is (coll/any? #(string/includes? % "image/avif") (warnings)))
          (is (some? (workspace/find-resource workspace "/unsupported.gltf/materials/0.material")))
          (is (nil? (workspace/find-resource workspace "/unsupported.gltf/images/0.png")))

          (test-util/with-ui-run-later-rebound
            (fs/create-file! source-file content)
            (workspace/resource-sync! workspace))
          (is (coll/empty? (warnings)))
          (is (some? (workspace/find-resource workspace "/unsupported.gltf/images/0.png")))

          (test-util/with-ui-run-later-rebound
            (fs/create-file! source-file unsupported-content)
            (workspace/resource-sync! workspace))
          (is (coll/any? #(string/includes? % "image/avif") (warnings)))
          (test-util/with-ui-run-later-rebound
            (fs/delete-file! source-file)
            (workspace/resource-sync! workspace))
          (is (coll/empty? (warnings))))))))

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

(ns integration.gltf-resource-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.camera :as camera]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.gl.pass :as pass]
            [editor.gltf :as gltf]
            [editor.model-scene :as model-scene]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-watch :as resource-watch]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support :refer [with-clean-system]]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline TextureGenerator]
           [com.dynamo.bob.util TextureUtil]
           [com.dynamo.gamesys.proto MeshProto$MeshDesc]
           [com.dynamo.lua.proto Lua$LuaModule]
           [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream IOException]
           [java.net URI]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [java.util.zip ZipEntry ZipOutputStream]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d]))

(def ^:private geometry-buffer-base64
  "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA")

(def ^:private ^bytes geometry-buffer-bytes
  (.decode (Base64/getDecoder) geometry-buffer-base64))

(defn- gltf-content
  ([material-name]
   (gltf-content material-name (str "data:application/octet-stream;base64," geometry-buffer-base64)))
  ([material-name buffer-uri]
   (str "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],"
        "\"meshes\":[{\"name\":\"Body\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
        "\"buffers\":[{\"uri\":\""
        buffer-uri
        "\",\"byteLength\":42}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"samplers\":[{\"wrapS\":10497,\"wrapT\":10497,\"minFilter\":9729,\"magFilter\":9729}],"
        "\"images\":[{\"name\":\"Albedo\",\"uri\":\"albedo.png\",\"mimeType\":\"image/png\"}],"
        "\"textures\":[{\"name\":\"AlbedoTexture\",\"sampler\":0,\"source\":0}],"
        "\"materials\":[{\"name\":\""
        material-name
        "\",\"pbrMetallicRoughness\":{"
        "\"baseColorTexture\":{\"index\":0},"
        "\"metallicRoughnessTexture\":{\"index\":0}},"
        "\"normalTexture\":{\"index\":0},"
        "\"occlusionTexture\":{\"index\":0},"
        "\"emissiveTexture\":{\"index\":0}}]}")))

(defn- png-bytes
  ^bytes [color]
  (let [image (BufferedImage. 1 1 BufferedImage/TYPE_INT_ARGB)
        output (ByteArrayOutputStream.)]
    (.setRGB image 0 0 (unchecked-int color))
    (when-not (ImageIO/write image "png" output)
      (throw (IOException. "No PNG writer is available")))
    (.toByteArray output)))

(defn- embedded-gltf-content [material-name]
  (string/replace (gltf-content material-name)
                  "albedo.png"
                  (str "data:image/png;base64," (.encodeToString (Base64/getEncoder) (png-bytes 0xff336699)))))

(defn- proj-paths [resources]
  (into #{} (map resource/proj-path) resources))

(defn- write-library-zip! [zip-file entries]
  (let [bytes (ByteArrayOutputStream.)]
    (with-open [output (ZipOutputStream. bytes)]
      (run!
        (fn [[entry-path content]]
          (let [^bytes content (if (string? content)
                                 (.getBytes ^String content StandardCharsets/UTF_8)
                                 content)]
            (.putNextEntry output (ZipEntry. entry-path))
            (.write output content 0 (alength content))
            (.closeEntry output)))
        entries))
    (fs/create-parent-directories! zip-file)
    (test-support/write-until-new-mtime zip-file (.toByteArray bytes))))

(defn- with-gltf-project
  "Loads the same fixture from disk or a cached ZIP library."
  [origin content f]
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)]
      (test-util/with-project-default-library-directory
        (let [library-uri (URI/create "file:/gltf-resource-test")]
          (if (= :zip origin)
            (write-library-zip!
              (test-support/library-file (io/file project-path) library-uri "")
              [["game.project" "[library]\ninclude_dirs = models\n"]
               ["models/robot.gltf" content]])
            (fs/create-file! (io/file project-path "models/robot.gltf") content))
          (with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)]
              (when (= :zip origin)
                (test-util/set-cached-project-dependencies! workspace [library-uri])
                (workspace/resource-sync! workspace))
              (f project-path workspace (test-util/setup-project! workspace)))))))))

(defn- preview-texture-paths
  "Returns the image paths connected to the source scene's preview bindings."
  [source-node]
  (into []
        (comp (filter #(g/node-instance? model-scene/GltfPreviewMaterialBinding %))
              (mapcat #(g/node-value % :nodes))
              (map #(resource/proj-path (g/node-value % :texture))))
        (g/node-value source-node :nodes)))

(deftest discovers-assets-from-files-and-zip-entries
  (doseq [origin [:file :zip]]
    (testing origin
      (with-gltf-project origin (string/replace (gltf-content "Paint") "albedo.png" "../albedo.png")
        (fn [_project-path workspace _project]
          (let [source (workspace/find-resource workspace "/models/robot.gltf")
                material (workspace/find-resource workspace "/models/robot.gltf/materials/Paint_0.material")
                mesh (workspace/find-resource workspace "/models/robot.gltf/meshes/Body_0")]
            (is (= #{"/models/robot.gltf/materials" "/models/robot.gltf/meshes"}
                   (proj-paths (resource/children source))))
            (is (= :file (resource/source-type source)))
            (is (resource/openable? source))
            (is (= "robot.gltf" (resource/resource-name source)))
            (is (resource/openable? mesh))
            (is (= "Body_0" (resource/resource-name mesh)))
            (is (= "icons/32/Icons_27-AT-Mesh.png" (workspace/resource-icon mesh)))
            (is (resource/read-only? material))
            (is (= "Paint_0.material" (resource/resource-name material)))
            (is (not (resource/save-tracked? material)))
            (is (string/includes? (slurp material) "name: \"Paint\""))
            (is (thrown? Exception (io/output-stream material)))
            (is (string/includes? (slurp mesh) "mesh_index: 0"))))))))

(deftest unnamed-meshes-render-without-exposing-resources
  (doseq [origin [:file :zip]]
    (with-gltf-project origin (string/replace (gltf-content "Paint") "\"name\":\"Body\"," "")
      (fn [_project-path workspace project]
        (let [source-path "/models/robot.gltf"
              source-node (test-util/resource-node project source-path)]
          (is (nil? (workspace/find-resource workspace (str source-path "/meshes"))))
          (is (not (g/error-value? (g/node-value source-node :scene))))
          (is (= 1 (count (get-in (g/node-value source-node :content) [:mesh-set :models])))))))))

(deftest touching-containers-preserves-embedded-resources
  (doseq [origin [:file :zip]]
    (testing origin
      (with-gltf-project origin (gltf-content "Paint")
        (fn [project-path workspace project]
          (let [container-file (if (= :file origin)
                                 (io/file project-path "models/robot.gltf")
                                 (test-support/library-file (io/file project-path) (URI/create "file:/gltf-resource-test") ""))
                mesh-path "/models/robot.gltf/meshes/Body_0"
                mesh-node (test-util/resource-node project mesh-path)
                old-snapshot (g/node-value workspace :resource-snapshot)]
            (test-support/touch-until-new-mtime container-file)
            (workspace/resource-sync! workspace)
            (let [changes (resource-watch/diff old-snapshot (g/node-value workspace :resource-snapshot))]
              (is (contains? (proj-paths (:changed changes)) "/models/robot.gltf"))
              (is (coll/not-any? #(string/starts-with? (resource/proj-path %) "/models/robot.gltf/")
                                 (:changed changes)))
              (is (= mesh-node (test-util/resource-node project mesh-path))))))))))

(deftest mesh-previews-update-without-replacing-embedded-resources
  (with-gltf-project :file (gltf-content "Paint")
    (fn [project-path workspace project]
      (let [source-file (io/file project-path "models/robot.gltf")
            mesh-path "/models/robot.gltf/meshes/Body_0"
            mesh-node (test-util/resource-node project mesh-path)
            old-scene (g/node-value mesh-node :scene)]
        (is (not (g/error-value? old-scene)))
        (let [geometry-bytes (byte-array geometry-buffer-bytes)]
          (doto (ByteBuffer/wrap geometry-bytes)
            (.order ByteOrder/LITTLE_ENDIAN)
            (.putFloat 12 2.0))
          (test-support/write-until-new-mtime
            source-file
            (json/write-str
              (-> (json/read-str (gltf-content "Paint") :key-fn keyword)
                  (assoc-in [:buffers 0 :uri] (str "data:application/octet-stream;base64,"
                                                   (.encodeToString (Base64/getEncoder) geometry-bytes)))
                  (assoc-in [:accessors 0 :max] [2 1 0])))))
        (workspace/resource-sync! workspace)
        (is (= mesh-node (test-util/resource-node project mesh-path)))
        (let [scene (g/node-value mesh-node :scene)]
          (is (not (g/error-value? scene)))
          (is (not= (:aabb old-scene) (:aabb scene))))))))

(deftest embedded-resource-names-match-paths
  (doseq [origin [:file :zip]]
    (testing origin
      (with-gltf-project origin (-> (embedded-gltf-content "Paint/Chrome")
                                    (string/replace "\"Albedo\"" "\"Albedo/Chrome\"")
                                    (string/replace "albedo.png" "../albedo.png"))
        (fn [_project-path workspace _project]
          (doseq [[path expected-name original-name]
                  [["/models/robot.gltf/materials/Paint_Chrome_0.material" "Paint_Chrome_0.material" "Paint/Chrome"]
                   ["/models/robot.gltf/images/Albedo_Chrome_0.png" "Albedo_Chrome_0.png" "Albedo/Chrome"]]]
            (let [resource (workspace/find-resource workspace path)]
              (is (= expected-name (resource/resource-name resource)))
              (is (= original-name (:name (gltf/asset-info resource)))))))))))

(deftest embedded-assets-appear-in-resource-dialogs
  (with-gltf-project :file (embedded-gltf-content "Paint")
    (fn [_project-path workspace _project]
      (let [choices (atom #{})]
        (with-redefs [dialogs/make-select-list-dialog
                      (fn [items _localization _options]
                        (reset! choices (proj-paths items))
                        nil)]
          (resource-dialog/make workspace nil {:ext "material"})
          (is (contains? @choices "/models/robot.gltf/materials/Paint_0.material"))
          (resource-dialog/make workspace nil {:ext "png"})
          (is (contains? @choices "/models/robot.gltf/images/Albedo_0.png"))
          (resource-dialog/make workspace nil {:ext "model"})
          (is (not (contains? @choices "/models/robot.gltf/meshes/Body_0"))))))))

(deftest external-images-refresh-without-reloading-the-container
  (doseq [origin [:file :zip]]
    (testing origin
      (with-gltf-project origin (string/replace (gltf-content "Paint") "albedo.png" "../albedo.png")
        (fn [project-path workspace project]
          (let [source-node (test-util/resource-node project "/models/robot.gltf")
                source (workspace/find-resource workspace "/models/robot.gltf")
                resolve-resource (partial workspace/resolve-workspace-resource workspace)
                bindings (gltf/material-binding-descriptors source nil resolve-resource)
                image-file (io/file project-path "albedo.png")]
            (is (= 5 (count (:textures (first bindings)))))
            (is (= (vec (repeat 5 "/albedo.png")) (preview-texture-paths source-node)))
            (doseq [color [0xff336699 0xffcc8844]]
              (test-support/write-until-new-mtime image-file (png-bytes color))
              (workspace/resource-sync! workspace)
              (is (= source-node (test-util/resource-node project "/models/robot.gltf")))
              (let [image-node (test-util/resource-node project "/albedo.png")
                    generator (g/node-value image-node :content-generator)]
                (is (not (g/error-value? generator)))
                (when-not (g/error-value? generator)
                  (let [^BufferedImage image (texture-util/call-generator generator)]
                    (is (= (unchecked-int color) (.getRGB image 0 0)))))))
            (fs/delete-file! image-file)
            (workspace/resource-sync! workspace)
            (is (= source-node (test-util/resource-node project "/models/robot.gltf")))
            (is (= bindings (gltf/material-binding-descriptors (workspace/find-resource workspace "/models/robot.gltf") nil resolve-resource)))
            (is (= (vec (repeat 5 "/albedo.png")) (preview-texture-paths source-node)))
            (is (nil? (workspace/find-resource workspace "/albedo.png")))))))))

(deftest images-in-the-same-zip-refresh-with-the-library
  (with-gltf-project :zip (gltf-content "Paint")
    (fn [project-path workspace project]
      (let [archive (test-support/library-file (io/file project-path) (URI/create "file:/gltf-resource-test") "")
            image-path "/models/albedo.png"]
        (doseq [color [0xff336699 0xffcc8844]]
          (let [bytes (png-bytes color)]
            (write-library-zip! archive
                                [["game.project" "[library]\ninclude_dirs = models\n"]
                                 ["models/robot.gltf" (gltf-content "Paint" "geometry.bin")]
                                 ["models/geometry.bin" geometry-buffer-bytes]
                                 ["models/albedo.png" bytes]])
            (workspace/resource-sync! workspace)
            (let [image (workspace/find-resource workspace image-path)
                  node (test-util/resource-node project image-path)
                  generator (g/node-value node :content-generator)]
              (is (resource/zip-resource? image))
              (is (resource/zip-resource? (workspace/find-resource workspace "/models/albedo.png")))
              (is (= (vec bytes) (vec (resource/resource->bytes image))))
              (is (not (g/error-value? generator)))
              (when-not (g/error-value? generator)
                (is (= (unchecked-int color) (.getRGB ^BufferedImage (texture-util/call-generator generator) 0 0)))))))
        (write-library-zip! archive
                            [["game.project" "[library]\ninclude_dirs = models\n"]
                             ["models/robot.gltf" (gltf-content "Paint")]])
        (workspace/resource-sync! workspace)
        (is (nil? (workspace/find-resource workspace image-path)))))))

(deftest external-image-discovery-does-not-read-siblings
  (doseq [origin [:file :zip]
          uri ["albedo" "albedo.ktx2" "../textures/albedo%20map.png"]]
    (with-gltf-project origin (string/replace (gltf-content "Paint") "albedo.png" uri)
      (fn [_project-path workspace _project]
        (let [source (workspace/find-resource workspace "/models/robot.gltf")
              resolve-resource (partial workspace/resolve-workspace-resource workspace)
              image-path (gltf/uri->proj-path "/models/robot.gltf" uri)
              bindings (gltf/material-binding-descriptors source nil resolve-resource)]
          (is (nil? (workspace/find-resource workspace "/models/robot.gltf/images")))
          (is (= [image-path] (gltf/external-image-paths source)))
          (is (= (vec (repeat 5 image-path))
                 (mapv (comp resource/proj-path :texture) (:textures (first bindings)))))
          (is (= [image-path]
                 (mapv (comp resource/proj-path :image)
                       (:textures (gltf/metadata-descriptors source resolve-resource))))))))))

(deftest missing-geometry-does-not-hide-container-assets
  (with-gltf-project :file (gltf-content "Paint" "geometry.bin")
    (fn [project-path workspace project]
      (let [source-node (test-util/resource-node project "/models/robot.gltf")
            bindings (gltf/material-binding-descriptors (workspace/find-resource workspace "/models/robot.gltf") nil (partial workspace/resolve-workspace-resource workspace))]
        (is (= ["Paint"] (mapv :name bindings)))
        (is (g/error-value? (g/node-value source-node :content)))
        (fs/create-file! (io/file project-path "models/geometry.bin") geometry-buffer-bytes)
        (workspace/resource-sync! workspace)
        (is (= source-node (test-util/resource-node project "/models/robot.gltf")))
        (is (not (g/error-value? (g/node-value source-node :content))))))))

(deftest source-edits-refresh-materials-and-recover-from-invalid-json
  (with-gltf-project :file (gltf-content "Paint")
    (fn [project-path workspace _project]
      (let [source-file (io/file project-path "models/robot.gltf")
            paint-path "/models/robot.gltf/materials/Paint_0.material"
            chrome-path "/models/robot.gltf/materials/Chrome_0.material"
            restored-path "/models/robot.gltf/materials/Restored_0.material"]
        (test-support/write-until-new-mtime source-file (gltf-content "Chrome"))
        (workspace/resource-sync! workspace)
        (is (nil? (workspace/find-resource workspace paint-path)))
        (is (string/includes? (slurp (workspace/find-resource workspace chrome-path)) "name: \"Chrome\""))
        (test-support/write-until-new-mtime source-file "{")
        (log/without-logging (workspace/resource-sync! workspace))
        (is (nil? (workspace/find-resource workspace chrome-path)))
        (test-support/write-until-new-mtime source-file (gltf-content "Restored"))
        (workspace/resource-sync! workspace)
        (is (string/includes? (slurp (workspace/find-resource workspace restored-path)) "name: \"Restored\""))))))

(deftest moving-a-container-moves-its-embedded-references
  (doseq [[renamed-resource-path target-source-path] [["/models/robot.gltf" "/models/renamed.gltf"]
                                                      ["/models" "/renamed/robot.gltf"]]]
    (testing renamed-resource-path
      (with-gltf-project :file (embedded-gltf-content "Paint")
        (fn [_project-path workspace project]
          (let [changes (atom nil)
                source-path "/models/robot.gltf"
                source-node (test-util/resource-node project source-path)
                old-material (workspace/find-resource workspace (str source-path "/materials/Paint_0.material"))]
            (g/node-value source-node :node-outline)
            (workspace/prepend-resource-listener! workspace 1
                                                  (reify resource/ResourceListener
                                                    (handle-changes [_this diff _render-progress!]
                                                      (reset! changes diff))))
            (asset-browser/rename [(workspace/find-resource workspace renamed-resource-path)]
                                  "renamed" test-util/localization)
            (let [moves (mapv #(mapv resource/proj-path %) (:moved @changes))]
              (is (= (count moves) (count (set moves))))
              (doseq [suffix ["" "/materials/Paint_0.material" "/images/Albedo_0.png" "/meshes/Body_0"]]
                (is (contains? (set moves) [(str source-path suffix) (str target-source-path suffix)]))))
            (is (= source-node (test-util/resource-node project target-source-path)))
            (is (not (resource/exists? old-material)))
            (is (resource/exists? (workspace/find-resource workspace (str target-source-path "/materials/Paint_0.material"))))
            (let [links (into []
                              (comp (mapcat :children) (keep :link))
                              (:children (g/node-value source-node :node-outline)))]
              (is (= #{(str target-source-path "/materials/Paint_0.material")
                       (str target-source-path "/images/Albedo_0.png")}
                     (proj-paths links))))))))))

(deftest declared-image-selection-does-not-depend-on-file-existence
  (let [source (-> (gltf-content "")
                   (string/replace "\"asset\":{\"version\":\"2.0\"},"
                                   "\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_texture_basisu\"],")
                   (string/replace "\"mimeType\":\"image/png\"}],"
                                   "\"mimeType\":\"image/png\"},{\"name\":\"Preferred\",\"uri\":\"preferred.png\",\"mimeType\":\"image/png\"}],")
                   (string/replace "\"source\":0}"
                                   "\"source\":0,\"extensions\":{\"KHR_texture_basisu\":{\"source\":1}}}"))]
    (with-gltf-project :file source
      (fn [project-path workspace _project]
        ;; PNG stand-ins isolate image selection from GPU decoding support.
        (doseq [available [false true false]]
          (let [image-file (io/file project-path "models/preferred.png")]
            (if available
              (fs/create-file! image-file (png-bytes 0xff224466))
              (fs/delete-file! image-file))
            (workspace/resource-sync! workspace)
            (let [source (workspace/find-resource workspace "/models/robot.gltf")
                  {:keys [materials textures]} (gltf/metadata-descriptors source (partial workspace/resolve-workspace-resource workspace))]
              (is (= [1] (mapv :image-index textures)))
              (is (:basisu (first textures)))
              (is (= "Material 0" (:name (first materials))))
              (is (= ["gltf_material_0"] (mapv :name (gltf/material-binding-descriptors source nil (partial workspace/resolve-workspace-resource workspace))))))))))))

(deftest changing-an-image-uri-updates-preview-bindings
  (with-gltf-project :file (gltf-content "Paint")
    (fn [project-path workspace project]
      (let [source-file (io/file project-path "models/robot.gltf")
            source-path "/models/robot.gltf"]
        (is (= (vec (repeat 5 "/models/albedo.png"))
               (preview-texture-paths (test-util/resource-node project source-path))))
        (test-support/write-until-new-mtime source-file
                                            (string/replace (gltf-content "Paint") "albedo.png" "other.png"))
        (workspace/resource-sync! workspace)
        (is (= (vec (repeat 5 "/models/other.png"))
               (preview-texture-paths (test-util/resource-node project source-path))))))))

(deftest external-images-keep-preview-bindings
  (with-gltf-project :file (string/replace (gltf-content "Paint")
                                           "\"uri\":\"albedo.png\",\"mimeType\":\"image/png\""
                                           "\"uri\":\"albedo\"")
    (fn [project-path workspace project]
      (let [image-file (io/file project-path "models/albedo")
            source-path "/models/robot.gltf"
            image-path "/models/albedo"]
        (is (= (vec (repeat 5 image-path)) (preview-texture-paths (test-util/resource-node project source-path))))
        (doseq [available [true false true]]
          (if available
            (test-support/write-until-new-mtime image-file (png-bytes 0xff336699))
            (fs/delete-file! image-file))
          (log/without-logging (workspace/resource-sync! workspace))
          (let [source-node (test-util/resource-node project source-path)]
            (is (= (vec (repeat 5 image-path))
                   (preview-texture-paths source-node)))
            (is (= (workspace/find-resource workspace source-path)
                   (g/node-value source-node :resource)))))))))

(deftest container-outline-links-follow-renames
  (with-gltf-project :file (embedded-gltf-content "Paint")
    (fn [_project-path workspace project]
      (let [source-path "/models/robot.gltf"
            renamed-path "/models/renamed.gltf"
            source-node (test-util/resource-node project source-path)]
        (g/node-value source-node :node-outline)
        (test-util/move-file! workspace source-path renamed-path)
        (is (= source-node (test-util/resource-node project renamed-path)))
        (let [links (into []
                          (comp (mapcat :children) (keep :link))
                          (:children (g/node-value source-node :node-outline)))]
          (is (= #{(str renamed-path "/materials/Paint_0.material")
                   (str renamed-path "/images/Albedo_0.png")}
                 (proj-paths links)))
          (doseq [link links]
            (is (resource/exists? link))))))))

(deftest ktx2-mesh-and-script-bindings
  (let [fixture (io/file "../com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/ktx2/uastc.ktx2")
        bytes (with-open [stream (io/input-stream fixture)] (.readAllBytes stream))
        content (-> (gltf-content "Paint")
                    (string/replace "albedo.png" (str "data:image/ktx2;base64," (.encodeToString (Base64/getEncoder) bytes)))
                    (string/replace "image/png" "image/ktx2"))
        bindings [["standalone" "/models/albedo.ktx2"] ["embedded" "/models/robot.gltf/images/Albedo_0.ktx2"]]]
    (with-gltf-project :file content
      (fn [project-path workspace project]
        (fs/create-file! (io/file project-path "models/albedo.ktx2") bytes)
        (fs/create-file! (io/file project-path "vertices.buffer")
                         "[{\"name\":\"position\",\"type\":\"float32\",\"count\":3,\"data\":[0,0,0,1,0,0,0,1,0]}]")
        (doseq [[name texture-path] bindings]
          (fs/create-file! (io/file project-path (str name ".mesh"))
                           (str "material: \"/builtins/materials/model.material\" vertices: \"/vertices.buffer\" "
                                "position_stream: \"position\" textures: \"" texture-path "\""))
          (fs/create-file! (io/file project-path (str name ".script"))
                           (str "go.property('texture', resource.texture('" texture-path "'))")))
        (workspace/resource-sync! workspace)
        (doseq [[name texture-path] bindings
                [ext property proto-class textures-key] [["mesh" :texture0 MeshProto$MeshDesc :textures]
                                                        ["script" :__texture Lua$LuaModule :property-resources]]]
          (let [node (test-util/resource-node project (str "/" name "." ext))
                property-data (get-in (g/node-value node :_properties) [:properties property])]
            (is (= texture-path (resource/proj-path (:value property-data))))
            (is (contains? (set (get-in property-data [:edit-type :ext])) "ktx2"))
            (with-open [_build (test-util/build! node)]
              (let [built (protobuf/bytes->map-with-defaults proto-class (test-util/node-build-output node))
                    texture-build-path (resource/proj-path (test-util/build-resource project texture-path))]
                (is (= [texture-build-path] (get built textures-key)))))))))))

(deftest ktx2-images-build-preview-and-refresh
  (let [fixture (io/file "../com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/ktx2/uastc.ktx2")
        bytes (with-open [stream (io/input-stream fixture)] (.readAllBytes stream))
        content (-> (gltf-content "Paint")
                    (string/replace "albedo.png" (str "data:image/ktx2;base64," (.encodeToString (Base64/getEncoder) bytes)))
                    (string/replace "image/png" "image/ktx2"))]
    (with-gltf-project :file content
      (fn [project-path workspace project]
        (let [image-file (io/file project-path "models/albedo.ktx2")
              virtual-path "/models/robot.gltf/images/Albedo_0.ktx2"
              profiles-file (io/file project-path "ktx2.texture_profiles")]
          (fs/create-file! image-file bytes)
          (fs/create-file! profiles-file
                           (str "path_settings { path: \"/models/**\" profile: \"KTX2\" }\n"
                                "profiles { name: \"KTX2\" platforms { os: OS_ID_GENERIC mipmaps: true "
                                "premultiply_alpha: false formats { format: TEXTURE_FORMAT_RGBA } } }"))
          (doseq [[name texture-path] [["standalone" "/models/albedo.ktx2"] ["embedded" virtual-path]]]
            (fs/create-file! (io/file project-path (str name ".model"))
                             (str "mesh: \"/models/robot.gltf\" materials { name: \"Paint\" "
                                  "material: \"/builtins/materials/model.material\" "
                                  "textures { sampler: \"tex0\" texture: \"" texture-path "\" } }")))
          (workspace/resource-sync! workspace)
          (doseq [[name texture-path] [["standalone" "/models/albedo.ktx2"] ["embedded" virtual-path]]]
            (let [node (test-util/resource-node project (str "/" name ".model"))
                  binding (-> (g/node-value node :material-binding-infos) first :texture-binding-infos first)]
              (is (= texture-path (resource/proj-path (:texture binding))))
              (is (texture-util/texture-lifecycle-generator? (:gpu-texture-generator binding)))
              (is (not (g/error-value? (g/node-value node :build-targets))))))
          (test-util/set-setting! (test-util/resource-node project "/game.project")
                                  ["graphics" "texture_profiles"]
                                  (workspace/find-resource workspace "/ktx2.texture_profiles"))
          (let [standalone (test-util/resource-node project "/models/albedo.ktx2")
                virtual (test-util/resource-node project virtual-path)
                profiles (test-util/resource-node project "/ktx2.texture_profiles")
                app-view (test-util/setup-app-view! project)]
            (doseq [node [standalone virtual]]
              (is (= {:width 8 :height 4} (g/node-value node :size)))
              (is (= "KTX2" (:name (g/node-value node :texture-profile))))
              (is (not (g/error-value? (g/node-value node :gpu-texture))))
              (is (false? (g/node-value node :dirty)))
              (let [[_ view] (test-util/open-scene-view! project app-view (resource/proj-path (g/node-value node :resource)) 320 160)
                    outline (g/node-value node :node-outline)
                    mips (:children outline)
                    preview-scene (g/node-value node :scene)
                    selected-mip (:node-id (second mips))
                    render-data (g/node-value view :scene-render-data)]
                (is (= ["Mip 0 (8 x 4)" "Mip 1 (4 x 2)"] (mapv (comp test-util/localization :label) mips)))
                (is (coll/every? :read-only (conj mips outline)))
                (is (= ["mip-0" "mip-1"] (mapv :node-outline-key (:children preview-scene))))
                (is (= 2 (count (g/node-value node :mip-texture-request-datas))))
                (is (= "8 x 4 (KTX2 profile)" (:info-text preview-scene)))
                (doseq [[property expected] [[:dimensions [8 4]] [:format "UASTC"] [:supercompression "—"]
                                            [:mip-count 2] [:channels 3] [:color-space "Linear"]
                                            [:premultiplied-alpha false] [:orientation "rd"]]]
                  (let [property-data (get-in (g/node-value node :_properties) [:properties property])]
                    (is (= expected (:value property-data)))
                    (is (true? (:read-only? property-data)))))
                (is (= (set (map :node-id mips))
                       (into #{} (map :picking-node-id) (get-in render-data [:renderables pass/selection]))))
                (app-view/select! app-view [selected-mip])
                (is (= [selected-mip] (mapv :node-id (g/node-value view :selected-renderables))))
                (let [selected-properties (properties/coalesce (g/node-value app-view :selected-node-properties))]
                  (is (= [[4 2]] (get-in selected-properties [:properties :dimensions :values])))
                  (is (true? (get-in selected-properties [:properties :dimensions :read-only?]))))
                (is (instance? BufferedImage (g/node-value view :frame)))
                (app-view/select! app-view [node])
                (let [screen-pos (camera/camera-project (g/node-value view :camera)
                                                        (g/node-value view :viewport)
                                                        (Point3d. 26.0 1.0 0.0))]
                  (test-util/mouse-click! view (.x screen-pos) (.y screen-pos))
                  (is (= [selected-mip] (g/node-value app-view :selected-node-ids)))))
              (with-open [_build (test-util/build! node)]
                (let [output (ByteArrayOutputStream.)
                      profile (tex-gen/match-texture-profile-pb (g/node-value project :texture-profiles)
                                                               (resource/proj-path (g/node-value node :resource)))
                      bob (TextureGenerator/generate bytes profile false)]
                  (TextureUtil/writeGenerateResultToOutputStream bob output)
                  (is (= (vec (.toByteArray output))
                         (vec (resource/resource->bytes (test-util/node-build-resource node))))))))
            (is (= #{virtual-path}
                   (into #{}
                         (map (comp resource/proj-path :texture))
                         (:textures (first (gltf/material-binding-descriptors
                                             (workspace/find-resource workspace "/models/robot.gltf") nil #(workspace/find-resource workspace %)))))))
            (let [target-before (first (g/node-value virtual :build-targets))
                  gpu-before (g/node-value virtual :gpu-texture-generator)]
              ;; Edit in memory only. Build and preview must track unsaved profile changes.
              (g/transact (g/update-property profiles :pb assoc-in [:profiles 0 :platforms 0 :regenerate-mipmaps] true))
              (is (true? (get-in (g/node-value virtual :texture-profile) [:platforms 0 :regenerate-mipmaps])))
              (is (not= (:content-hash target-before) (:content-hash (first (g/node-value virtual :build-targets)))))
              (is (not (identical? gpu-before (g/node-value virtual :gpu-texture-generator))))
              (g/transact (g/update-property profiles :pb assoc-in [:profiles 0 :platforms 0 :recompress] true))
              (is (true? (get-in (g/node-value profiles :save-value) [:profiles 0 :platforms 0 :recompress])))
              (g/transact (g/update-property profiles :pb assoc-in [:profiles 0 :platforms 0 :mipmaps] false))
              (is (= 2 (count (:children (g/node-value virtual :scene)))))
              (g/transact (g/update-property profiles :pb
                                             #(-> %
                                                  (assoc-in [:profiles 0 :name] "Changed")
                                                  (assoc-in [:path-settings 0 :profile] "Changed"))))
              (is (= "8 x 4 (Changed profile)" (:info-text (g/node-value virtual :scene)))))
            (let [before (:content-hash (first (g/node-value virtual :build-targets)))
                  changed (with-open [stream (io/input-stream (io/file (.getParentFile fixture) "etc1s.ktx2"))]
                            (.readAllBytes stream))]
              (test-support/write-until-new-mtime image-file changed)
              (test-support/write-until-new-mtime (io/file project-path "models/robot.gltf")
                                                  (string/replace content
                                                                  (.encodeToString (Base64/getEncoder) bytes)
                                                                  (.encodeToString (Base64/getEncoder) changed)))
              (workspace/resource-sync! workspace)
              (let [virtual (test-util/resource-node project virtual-path)]
                (is (not= before (:content-hash (first (g/node-value virtual :build-targets)))))
                (is (= "ETC1S" (g/node-value virtual :format)))
                (is (= "BasisLZ" (g/node-value virtual :supercompression)))))
            (let [changed (with-open [stream (io/input-stream (io/file (.getParentFile fixture) "bc7.ktx2"))]
                            (.readAllBytes stream))]
              (test-support/write-until-new-mtime image-file changed)
              (test-support/write-until-new-mtime (io/file project-path "models/robot.gltf")
                                                  (string/replace content
                                                                  (.encodeToString (Base64/getEncoder) bytes)
                                                                  (.encodeToString (Base64/getEncoder) changed)))
              (workspace/resource-sync! workspace)
              (doseq [path ["/models/albedo.ktx2" virtual-path]]
                (let [node (test-util/resource-node project path)]
                  (is (= "BC7" (g/node-value node :format)))
                  (is (= 1 (count (:children (g/node-value node :node-outline)))))
                  (is (= 1 (count (:children (g/node-value node :scene))))))))
            (fs/delete-file! image-file)
            (workspace/resource-sync! workspace)
            (is (g/error-value? (g/node-value (test-util/resource-node project "/standalone.model") :build-targets)))
            (test-support/write-until-new-mtime image-file bytes)
            (test-support/write-until-new-mtime (io/file project-path "models/robot.gltf") content)
            (workspace/resource-sync! workspace)
            (let [virtual (test-util/resource-node project virtual-path)]
              (is (= {:width 8 :height 4} (g/node-value virtual :size)))
              (is (= "UASTC" (g/node-value virtual :format)))
              (is (= 2 (count (:children (g/node-value virtual :node-outline))))))))))))

(deftest buffer-backed-ktx2-reloads-metadata-and-mips
  (doseq [origin [:file :zip]]
    (testing origin
      (let [fixtures (mapv (fn [[name format mip-count]]
                             {:bytes (with-open [stream (io/input-stream
                                                          (io/file "../com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/ktx2"
                                                                   (str name ".ktx2")))]
                                       (.readAllBytes stream))
                              :format format
                              :mip-count mip-count})
                           [["uastc" "UASTC" 2] ["bc7" "BC7" 1] ["etc1s" "ETC1S" 2]])
            length (transduce (map #(alength ^bytes (:bytes %))) max 0 fixtures)
            content (json/write-str
                      {:asset {:version "2.0"}
                       :buffers [{:uri "../images.bin" :byteLength (+ 4 length)}]
                       :bufferViews [{:buffer 0 :byteOffset 4 :byteLength length}]
                       :images [{:bufferView 0 :mimeType "image/ktx2"}]}
                      :escape-slash false)]
        (with-gltf-project origin content
          (fn [project-path workspace project]
            (let [image-path "/models/robot.gltf/images/0.ktx2"
                  buffer-file (io/file project-path "images.bin")
                  build-hashes (atom [])]
              (is (g/error-value? (g/node-value (test-util/resource-node project image-path) :content-generator)))
              (doseq [{:keys [bytes format mip-count]} fixtures]
                (let [buffer (byte-array (+ 4 length))]
                  (System/arraycopy bytes 0 buffer 4 (alength ^bytes bytes))
                  (test-support/write-until-new-mtime buffer-file buffer))
                (workspace/resource-sync! workspace)
                (let [node (test-util/resource-node project image-path)]
                  (is (= format (g/node-value node :format)))
                  (is (= mip-count (count (:children (g/node-value node :node-outline)))))
                  (is (= mip-count (count (g/node-value node :mip-texture-request-datas))))
                  (with-open [_build (test-util/build! node)]
                    (swap! build-hashes conj (vec (test-util/node-build-output node))))))
              (is (= 3 (count (set @build-hashes))))
              (fs/delete-file! buffer-file)
              (workspace/resource-sync! workspace)
              (is (g/error-value? (g/node-value (test-util/resource-node project image-path) :content-generator)))
              (let [buffer (byte-array (+ 4 length))
                    bytes (:bytes (first fixtures))]
                (System/arraycopy bytes 0 buffer 4 (alength ^bytes bytes))
                (test-support/write-until-new-mtime buffer-file buffer))
              (workspace/resource-sync! workspace)
              (let [node (test-util/resource-node project image-path)]
                (is (= "UASTC" (g/node-value node :format)))
                (is (= 2 (count (:children (g/node-value node :node-outline)))))))))))))

(deftest buffer-backed-images-reload-when-buffers-change
  (doseq [origin [:file :zip]]
    (testing origin
      (let [image-bytes (png-bytes 0xff336699)
            content (json/write-str
                      {:asset {:version "2.0"}
                       :buffers [{:uri "../images.bin" :byteLength (+ 4 (alength image-bytes))}]
                       :bufferViews [{:buffer 0 :byteOffset 4 :byteLength (alength image-bytes)}]
                       :images [{:bufferView 0 :mimeType "image/png"}]}
                      :escape-slash false)]
        (with-gltf-project origin content
          (fn [project-path workspace project]
            (let [source-path "/models/robot.gltf"
                  image-path (str source-path "/images/0.png")
                  buffer-file (io/file project-path "images.bin")
                  build-outputs (atom [])]
              (let [image-node (test-util/resource-node project image-path)]
                (is (g/error-value? (g/node-value image-node :content-generator)))
                (is (g/error-value? (g/node-value image-node :size))))
              (doseq [color [0xff336699 0xffcc8844]]
                (test-support/write-until-new-mtime buffer-file
                                                    (byte-array (into [0 0 0 0] (png-bytes color))))
                (workspace/resource-sync! workspace)
                (let [image-node (test-util/resource-node project image-path)
                      generator (g/node-value image-node :content-generator)]
                  (is (= {:width 1 :height 1} (g/node-value image-node :size)))
                  (is (not (g/error-value? generator)))
                  (when-not (g/error-value? generator)
                    (is (= (unchecked-int color)
                           (.getRGB ^BufferedImage (texture-util/call-generator generator) 0 0))))
                  (with-open [_build (test-util/build! image-node)]
                    (swap! build-outputs conj (vec (test-util/node-build-output image-node))))))
              (is (= 2 (count (set @build-outputs))))
              (fs/delete-file! buffer-file)
              (workspace/resource-sync! workspace)
              (let [image-node (test-util/resource-node project image-path)]
                (is (g/error-value? (g/node-value image-node :content-generator)))
                (is (g/error-value? (g/node-value image-node :size))))
              (test-support/write-until-new-mtime buffer-file (byte-array (into [0 0 0 0] image-bytes)))
              (workspace/resource-sync! workspace)
              (let [image-node (test-util/resource-node project image-path)]
                (is (= {:width 1 :height 1} (g/node-value image-node :size)))
                (is (not (g/error-value? (g/node-value image-node :content-generator))))))))))))

(deftest glb-image-content-depends-on-its-container
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_deleter (test-util/make-directory-deleter project-path)]
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              project (test-util/setup-project! workspace)
              source-file (io/file project-path "image.glb")]
          (doseq [color [0xff336699 0xffcc8844]]
            (let [image-bytes (png-bytes color)
                  json-bytes (.getBytes (json/write-str
                                          {:asset {:version "2.0"}
                                           :buffers [{:byteLength (alength image-bytes)}]
                                           :bufferViews [{:buffer 0 :byteLength (alength image-bytes)}]
                                           :images [{:bufferView 0 :mimeType "image/png"}]}
                                          :escape-slash false)
                                        StandardCharsets/UTF_8)
                  json-length (bit-and (+ (alength json-bytes) 3) -4)
                  binary-length (bit-and (+ (alength image-bytes) 3) -4)
                  length (+ 28 json-length binary-length)
                  glb (doto (ByteBuffer/allocate length)
                        (.order ByteOrder/LITTLE_ENDIAN)
                        (.putInt 0x46546c67)
                        (.putInt 2)
                        (.putInt length)
                        (.putInt json-length)
                        (.putInt 0x4e4f534a)
                        (.put json-bytes))]
              (while (< (.position glb) (+ 20 json-length))
                (.put glb (byte 32)))
              (doto glb
                (.putInt binary-length)
                (.putInt 0x004e4942)
                (.put image-bytes))
              (test-support/write-until-new-mtime source-file (.array glb)))
            (workspace/resource-sync! workspace)
            (let [node (test-util/resource-node project "/image.glb/images/0.png")
                  generator (g/node-value node :content-generator)]
              (is (not (g/error-value? generator)))
              (when-not (g/error-value? generator)
                (is (= (unchecked-int color)
                       (.getRGB ^BufferedImage (texture-util/call-generator generator) 0 0)))))))))))

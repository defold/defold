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
            [editor.asset-browser :as asset-browser]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.gltf :as gltf]
            [editor.model-scene :as model-scene]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-watch :as resource-watch]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support :refer [with-clean-system]]
            [util.coll :as coll])
  (:import [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream IOException]
           [java.net URI]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [java.util.zip ZipEntry ZipOutputStream]
           [javax.imageio ImageIO]))

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
                material (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")
                mesh (workspace/find-resource workspace "/models/robot.gltf/meshes/Body")]
            (is (= #{"/models/robot.gltf/materials" "/models/robot.gltf/meshes"}
                   (proj-paths (resource/children source))))
            (is (= :file (resource/source-type source)))
            (is (resource/openable? source))
            (is (= "robot.gltf" (resource/resource-name source)))
            (is (resource/openable? mesh))
            (is (= "robot.gltf : Body" (resource/resource-name mesh)))
            (is (= "icons/32/Icons_27-AT-Mesh.png" (workspace/resource-icon mesh)))
            (is (resource/read-only? material))
            (is (= "Paint [0].material" (resource/resource-name material)))
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
                mesh-path "/models/robot.gltf/meshes/Body"
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
            mesh-path "/models/robot.gltf/meshes/Body"
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

(deftest embedded-resource-names-are-safe-filenames
  (doseq [origin [:file :zip]]
    (testing origin
      (with-gltf-project origin (-> (embedded-gltf-content "Paint/Chrome")
                                   (string/replace "\"Albedo\"" "\"Albedo/Chrome\"")
                                   (string/replace "albedo.png" "../albedo.png"))
        (fn [_project-path workspace _project]
          (doseq [[path expected-name original-name]
                  [["/models/robot.gltf/materials/0.material" "Paint_Chrome [0].material" "Paint/Chrome"]
                   ["/models/robot.gltf/images/0.png" "Albedo_Chrome [0].png" "Albedo/Chrome"]]]
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
          (is (contains? @choices "/models/robot.gltf/materials/0.material"))
          (resource-dialog/make workspace nil {:ext "png"})
          (is (contains? @choices "/models/robot.gltf/images/0.png"))
          (resource-dialog/make workspace nil {:ext "model"})
          (is (not (contains? @choices "/models/robot.gltf/meshes/Body"))))))))

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
            material-path "/models/robot.gltf/materials/0.material"]
        (test-support/write-until-new-mtime source-file (gltf-content "Chrome"))
        (workspace/resource-sync! workspace)
        (is (string/includes? (slurp (workspace/find-resource workspace material-path)) "name: \"Chrome\""))
        (test-support/write-until-new-mtime source-file "{")
        (log/without-logging (workspace/resource-sync! workspace))
        (is (nil? (workspace/find-resource workspace material-path)))
        (test-support/write-until-new-mtime source-file (gltf-content "Restored"))
        (workspace/resource-sync! workspace)
        (is (string/includes? (slurp (workspace/find-resource workspace material-path)) "name: \"Restored\""))))))

(deftest moving-a-container-moves-its-embedded-references
  (doseq [[renamed-resource-path target-source-path] [["/models/robot.gltf" "/models/renamed.gltf"]
                                                     ["/models" "/renamed/robot.gltf"]]]
    (testing renamed-resource-path
      (with-gltf-project :file (embedded-gltf-content "Paint")
        (fn [_project-path workspace project]
          (let [changes (atom nil)
                source-path "/models/robot.gltf"
                source-node (test-util/resource-node project source-path)
                old-material (workspace/find-resource workspace (str source-path "/materials/0.material"))]
            (g/node-value source-node :node-outline)
            (workspace/prepend-resource-listener! workspace 1
                                                  (reify resource/ResourceListener
                                                    (handle-changes [_this diff _render-progress!]
                                                      (reset! changes diff))))
            (asset-browser/rename [(workspace/find-resource workspace renamed-resource-path)]
                                  "renamed" test-util/localization)
            (let [moves (mapv #(mapv resource/proj-path %) (:moved @changes))]
              (is (= (count moves) (count (set moves))))
              (doseq [suffix ["" "/materials/0.material" "/images/0.png" "/meshes/Body"]]
                (is (contains? (set moves) [(str source-path suffix) (str target-source-path suffix)]))))
            (is (= source-node (test-util/resource-node project target-source-path)))
            (is (not (resource/exists? old-material)))
            (is (resource/exists? (workspace/find-resource workspace (str target-source-path "/materials/0.material"))))
            (let [links (into []
                              (comp (mapcat :children) (keep :link))
                              (:children (g/node-value source-node :node-outline)))]
              (is (= #{(str target-source-path "/materials/0.material")
                       (str target-source-path "/images/0.png")}
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
          (is (= #{(str renamed-path "/materials/0.material")
                   (str renamed-path "/images/0.png")}
                 (proj-paths links)))
          (doseq [link links]
            (is (resource/exists? link))))))))

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
                (let [bytes (png-bytes color)]
                  (test-support/write-until-new-mtime buffer-file
                                                    (byte-array (into [0 0 0 0] bytes))))
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

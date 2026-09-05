;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns integration.gltf-resource-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.gltf :as gltf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support :refer [with-clean-system]])
  (:import [com.dynamo.bob.pipeline ModelImporterJni$DataResolver ModelUtil Modelimporter$Buffer Modelimporter$Scene]
           [java.awt.image BufferedImage]
           [java.io ByteArrayOutputStream IOException]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.util Arrays Base64]
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
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
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

(defn- proj-paths [resources]
  (into #{} (map resource/proj-path) resources))

(defn- moved-proj-path-pairs [moved-resources]
  (into #{}
        (map (fn [[source-resource target-resource]]
               [(resource/proj-path source-resource)
                (resource/proj-path target-resource)]))
        moved-resources))

(defn- write-library-zip! [zip-file entries]
  (fs/create-parent-directories! zip-file)
  (with-open [output (ZipOutputStream. (io/output-stream zip-file))]
    (run!
      (fn [[entry-path content]]
        (let [^bytes content (if (string? content)
                               (.getBytes ^String content StandardCharsets/UTF_8)
                               content)]
          (.putNextEntry output (ZipEntry. entry-path))
          (.write output content 0 (alength content))
          (.closeEntry output)))
      entries)))

(deftest workspace-exposes-and-refreshes-gltf-resources
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        models-directory (io/file project-path "models")
        gltf-file (io/file models-directory "robot.gltf")
        image-file (io/file models-directory "albedo.png")
        mesh-proj-path "/models/robot.gltf/meshes/Mesh 0"
        renamed-mesh-proj-path "/models/renamed.gltf/meshes/Mesh 0"
        initial-png (png-bytes 0xff336699)]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! gltf-file (gltf-content "Paint"))
      (fs/create-file! image-file initial-png)

      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              source-resource (workspace/find-resource workspace "/models/robot.gltf")
              meshes-resource (workspace/find-resource workspace "/models/robot.gltf/meshes")
              mesh-resource (workspace/find-resource workspace mesh-proj-path)
              material-resource (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")
              image-resource (workspace/find-resource workspace "/models/robot.gltf/images/0.png")]
          (is (= #{"/models/robot.gltf/images"
                   "/models/robot.gltf/materials"
                   "/models/robot.gltf/meshes"}
                 (proj-paths (resource/children source-resource))))
          (is (= #{mesh-proj-path}
                 (proj-paths (resource/children meshes-resource))))
          (is (= 1 (count (resource/children meshes-resource))))
          (is (= [{:index 0
                   :name "Mesh 0"
                   :name-generated true
                   :primitive-count 1
                   :vertex-count 3}]
                 (:meshes (gltf/metadata-descriptors source-resource))))
          (is (= (:meshes (gltf/metadata-descriptors source-resource))
                 (-> source-resource
                     (g/write-graph (core/write-handlers))
                     (g/read-graph (core/read-handlers))
                     (gltf/metadata-descriptors)
                     :meshes)))
          (is (resource/gltf-resource? meshes-resource))
          (is (resource/gltf-resource? mesh-resource))
          (is (resource/gltf-resource? material-resource))
          (is (resource/gltf-resource? image-resource))
          (is (= :file (resource/source-type source-resource)))
          (is (= :folder (resource/source-type meshes-resource)))
          (is (= :file (resource/source-type mesh-resource)))
          (is (= :file (resource/source-type material-resource)))
          (is (= :file (resource/source-type image-resource)))
          (is (= "" (resource/type-ext mesh-resource)))
          (is (= "material" (resource/type-ext material-resource)))
          (is (= "png" (resource/type-ext image-resource)))
          (is (resource/read-only? mesh-resource))
          (is (resource/read-only? material-resource))
          (is (resource/read-only? image-resource))
          (is (resource/loaded? mesh-resource))
          (is (resource/loaded? material-resource))
          (is (resource/loaded? image-resource))
          (is (false? (resource/openable? mesh-resource)))
          (is (= "icons/32/Icons_27-AT-Mesh.png"
                 (workspace/resource-icon mesh-resource)))
          (is (zero? (count (resource/resource->bytes mesh-resource))))
          (is (not (resource/save-tracked? mesh-resource)))
          (is (not (resource/save-tracked? material-resource)))
          (is (= {:kind :mesh
                  :index 0
                  :name "model_0"
                  :path "meshes/Mesh 0"
                  :name-generated true
                  :primitive-count 1
                  :vertex-count 3}
                 (resource/gltf-resource-asset-info mesh-resource)))
          (is (= {:kind :material
                  :index 0
                  :name "Paint"
                  :material-name "Paint"
                  :path "materials/0.material"
                  :sampler-bindings
                  [{:sampler "PbrMetallicRoughness_baseColorTexture"
                    :material-index 0
                    :texture-index 0
                    :image-index 0
                    :image-path "images/0.png"}
                   {:sampler "PbrMetallicRoughness_metallicRoughnessTexture"
                    :material-index 0
                    :texture-index 0
                    :image-index 0
                    :image-path "images/0.png"}
                   {:sampler "PbrMaterial_normalTexture"
                    :material-index 0
                    :texture-index 0
                    :image-index 0
                    :image-path "images/0.png"}
                   {:sampler "PbrMaterial_occlusionTexture"
                    :material-index 0
                    :texture-index 0
                    :image-index 0
                    :image-path "images/0.png"}
                   {:sampler "PbrMaterial_emissiveTexture"
                    :material-index 0
                    :texture-index 0
                    :image-index 0
                    :image-path "images/0.png"}]}
                 (resource/gltf-resource-asset-info material-resource)))
          (is (= {:kind :image
                  :index 0
                  :name "Albedo"
                  :path "images/0.png"
                  :uri "albedo.png"
                  :mime-type "image/png"
                  :source-kind "external-uri"
                  :textures [{:index 0
                              :name "AlbedoTexture"
                              :sampler-index 0
                              :min-filter 9729
                              :mag-filter 9729
                              :wrap-s 10497
                              :wrap-t 10497
                              :basisu false}]}
                 (resource/gltf-resource-asset-info image-resource)))
          (is (= material-resource
                 (g/read-graph
                   (g/write-graph material-resource (core/write-handlers))
                   (core/read-handlers))))
          (is (= mesh-resource
                 (g/read-graph
                   (g/write-graph mesh-resource (core/write-handlers))
                   (core/read-handlers))))
          (is (thrown? IOException (io/output-stream material-resource)))
          (is (.contains (String. ^bytes (resource/resource->bytes material-resource)
                                  StandardCharsets/UTF_8)
                         "name: \"Paint\""))
          (is (Arrays/equals initial-png (resource/resource->bytes image-resource)))

          (let [selected-proj-paths (atom nil)]
            (with-redefs [dialogs/make-select-list-dialog
                          (fn [items _localization _options]
                            (reset! selected-proj-paths (proj-paths items))
                            nil)]
              (resource-dialog/make workspace nil {:ext "material"})
              (is (contains? @selected-proj-paths "/models/robot.gltf/materials/0.material"))
              (resource-dialog/make workspace nil {:ext "png"})
              (is (contains? @selected-proj-paths "/models/robot.gltf/images/0.png"))))

          (fs/create-file! (io/file project-path "unrelated.txt") "unrelated")
          (let [sync-diff (workspace/resource-sync! workspace)]
            (is (contains? (proj-paths (:added sync-diff)) "/unrelated.txt"))
            (is (not (contains? (proj-paths (:changed sync-diff))
                                "/models/robot.gltf/materials/0.material")))
            (is (= material-resource
                   (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")))
            (is (= image-resource
                   (workspace/find-resource workspace "/models/robot.gltf/images/0.png")))
            (is (= mesh-resource
                   (workspace/find-resource workspace mesh-proj-path))))

          (let [updated-png (png-bytes 0xffcc8844)]
            (test-support/write-until-new-mtime image-file updated-png)
            (let [sync-diff (workspace/resource-sync! workspace)
                  changed-proj-paths (proj-paths (:changed sync-diff))
                  updated-image-resource (workspace/find-resource workspace "/models/robot.gltf/images/0.png")]
              (is (contains? changed-proj-paths "/models/albedo.png"))
              (is (contains? changed-proj-paths "/models/robot.gltf/images/0.png"))
              (is (not= image-resource updated-image-resource))
              (is (Arrays/equals updated-png (resource/resource->bytes updated-image-resource)))))

          (test-support/write-until-new-mtime gltf-file (gltf-content "Chrome"))
          (let [sync-diff (workspace/resource-sync! workspace)
                changed-proj-paths (proj-paths (:changed sync-diff))
                updated-material-resource (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")]
            (is (contains? changed-proj-paths "/models/robot.gltf"))
            (is (contains? changed-proj-paths "/models/robot.gltf/materials/0.material"))
            (is (not= material-resource updated-material-resource))
            (is (.contains (String. ^bytes (resource/resource->bytes updated-material-resource)
                                    StandardCharsets/UTF_8)
                           "name: \"Chrome\"")))

          (test-support/write-until-new-mtime gltf-file "{")
          (let [sync-diff (log/without-logging (workspace/resource-sync! workspace))
                removed-proj-paths (proj-paths (:removed sync-diff))]
            (is (contains? removed-proj-paths "/models/robot.gltf/materials/0.material"))
            (is (contains? removed-proj-paths "/models/robot.gltf/images/0.png"))
            (is (contains? removed-proj-paths mesh-proj-path))
            (is (nil? (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")))
            (is (nil? (workspace/find-resource workspace "/models/robot.gltf/images/0.png")))
            (is (nil? (workspace/find-resource workspace mesh-proj-path))))

          (test-support/write-until-new-mtime gltf-file (gltf-content "Restored"))
          (let [sync-diff (workspace/resource-sync! workspace)
                added-proj-paths (proj-paths (:added sync-diff))]
            (is (contains? added-proj-paths "/models/robot.gltf/materials/0.material"))
            (is (contains? added-proj-paths "/models/robot.gltf/images/0.png"))
            (is (contains? added-proj-paths mesh-proj-path)))

          (let [old-material-resource (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")
                old-mesh-resource (workspace/find-resource workspace mesh-proj-path)
                observed-changes (atom nil)
                observing-resource-listener
                (reify resource/ResourceListener
                  (handle-changes [_this changes _render-progress!]
                    (reset! observed-changes changes)))]
            (workspace/prepend-resource-listener! workspace 1 observing-resource-listener)
            (test-util/move-file! workspace "/models/robot.gltf" "/models/renamed.gltf")
            (let [moved-path-pairs (moved-proj-path-pairs (:moved @observed-changes))]
              (is (contains? moved-path-pairs
                             ["/models/robot.gltf" "/models/renamed.gltf"]))
              (is (contains? moved-path-pairs
                             ["/models/robot.gltf/materials/0.material"
                              "/models/renamed.gltf/materials/0.material"]))
              (is (contains? moved-path-pairs
                             ["/models/robot.gltf/images/0.png"
                              "/models/renamed.gltf/images/0.png"]))
              (is (contains? moved-path-pairs
                             [mesh-proj-path renamed-mesh-proj-path]))
              (is (not (resource/exists? old-material-resource)))
              (is (not (resource/exists? old-mesh-resource)))
              (is (resource/exists?
                    (workspace/find-resource workspace "/models/renamed.gltf/materials/0.material")))
              (is (resource/exists?
                    (workspace/find-resource workspace "/models/renamed.gltf/images/0.png")))
              (let [renamed-mesh-resource (workspace/find-resource workspace renamed-mesh-proj-path)]
                (is (resource/exists? renamed-mesh-resource))
                (is (false? (resource/openable? renamed-mesh-resource)))
                (is (= "icons/32/Icons_27-AT-Mesh.png"
                       (workspace/resource-icon renamed-mesh-resource)))))))))))

(deftest adding-missing-gltf-image-reloads-preview-bindings
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        models-directory (io/file project-path "models")
        gltf-file (io/file models-directory "robot.gltf")
        image-file (io/file models-directory "albedo.png")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! gltf-file (gltf-content "Paint"))

      (with-clean-system
        (let [workspace (log/without-logging
                          (test-util/setup-workspace! world project-path))
              project (test-util/setup-project! workspace)
              source-node-id (test-util/resource-node project "/models/robot.gltf")
              material-binding-node-id (first (g/node-value source-node-id :nodes))]
          (is (nil? (workspace/find-resource workspace "/models/robot.gltf/images/0.png")))
          (is (zero? (count (g/node-value material-binding-node-id :nodes))))

          (fs/create-file! image-file (png-bytes 0xff336699))
          (let [sync-diff (workspace/resource-sync! workspace)
                changed-proj-paths (proj-paths (:changed sync-diff))
                added-proj-paths (proj-paths (:added sync-diff))
                reloaded-source-node-id (test-util/resource-node project "/models/robot.gltf")
                reloaded-material-binding-node-id (first (g/node-value reloaded-source-node-id :nodes))]
            (is (contains? changed-proj-paths "/models/robot.gltf"))
            (is (contains? added-proj-paths "/models/robot.gltf/images/0.png"))
            (is (not= source-node-id reloaded-source-node-id))
            (is (= 5 (count (g/node-value reloaded-material-binding-node-id :nodes))))))))))

(deftest gltf-descriptors-use-shared-image-selection-and-material-names
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        gltf-file (io/file project-path "models/robot.gltf")
        preferred-file (io/file project-path "models/preferred.png")
        png (png-bytes 0xff224466)
        source (-> (gltf-content "")
                   (string/replace "\"asset\":{\"version\":\"2.0\"},"
                                   "\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_texture_basisu\"],")
                   (string/replace "\"mimeType\":\"image/png\"}],"
                                   "\"mimeType\":\"image/png\"},{\"name\":\"Preferred\",\"uri\":\"preferred.png\",\"mimeType\":\"image/png\"}],")
                   (string/replace "\"source\":0}"
                                   "\"source\":0,\"extensions\":{\"KHR_texture_basisu\":{\"source\":1}}}"))]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! gltf-file source)
      (fs/create-file! (io/file project-path "models/albedo.png") png)
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]
          ;; PNG stand-ins exercise image selection independently of GPU texture decoding.
          (doseq [preferred-available [false true false]]
            (if preferred-available
              (fs/create-file! preferred-file png)
              (fs/delete-file! preferred-file))
            (workspace/resource-sync! workspace)
            (let [source-resource (workspace/find-resource workspace "/models/robot.gltf")
                  {:keys [materials textures]} (gltf/metadata-descriptors source-resource)
                  bindings (gltf/material-binding-descriptors source-resource nil)
                  selected-image-index (if preferred-available 1 0)
                  selected-image-path (format "/models/robot.gltf/images/%d.png" selected-image-index)]
              (is (= 1 (count textures)))
              (is (= selected-image-index (:image-index (first textures))))
              (is (= selected-image-path (resource/proj-path (:image (first textures)))))
              (is (= preferred-available (:basisu (first textures))))
              (is (= "Material 0" (:name (first materials))))
              (is (= ["gltf_material_0"] (mapv :name bindings)))
              (is (= #{selected-image-path}
                     (into #{}
                           (map (comp resource/proj-path :texture))
                           (:textures (first bindings))))))))))))

(deftest dependency-gltf-remains-a-selectable-file-container
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        library-uri (URI/create "file:/gltf-resource-test")
        initial-png (png-bytes 0xff224466)]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (test-util/with-project-default-library-directory
        (write-library-zip!
          (test-support/library-file (io/file project-path) library-uri "")
          [["game.project" "[library]\ninclude_dirs = models\n"]
           ["models/robot.gltf" (gltf-content "LibraryPaint" "geometry.bin")]
           ["models/geometry.bin" geometry-buffer-bytes]
           ["models/albedo.png" initial-png]])
        (with-clean-system
          (let [workspace (test-util/setup-workspace! world project-path)]
            (test-util/set-cached-project-dependencies! workspace [library-uri])
            (workspace/resource-sync! workspace)
            (let [project (test-util/setup-project! workspace)
                  source-resource (workspace/find-resource workspace "/models/robot.gltf")
                  meshes-resource (workspace/find-resource workspace "/models/robot.gltf/meshes")
                  mesh-resource (workspace/find-resource workspace "/models/robot.gltf/meshes/Mesh 0")
                  material-resource (workspace/find-resource workspace "/models/robot.gltf/materials/0.material")
                  image-resource (workspace/find-resource workspace "/models/robot.gltf/images/0.png")
                  source-node-id (project/get-resource-node project source-resource)
                  external-buffer-resources (g/node-value source-node-id :external-buffer-resources)
                  ^ModelImporterJni$DataResolver data-resolver
                  (gltf/make-data-resolver #(workspace/resolve-workspace-resource workspace %) nil)
                  ^Modelimporter$Scene scene
                  (with-open [stream (io/input-stream source-resource)]
                    (ModelUtil/loadScene stream (resource/path source-resource) nil data-resolver))
                  ^Modelimporter$Buffer buffer (first (.buffers scene))]
              (is (resource/zip-resource? source-resource))
              (is (= [{:index 0
                       :name "Mesh 0"
                       :name-generated true
                       :primitive-count 1
                       :vertex-count 3}]
                     (-> source-resource
                         (g/write-graph (core/write-handlers))
                         (g/read-graph (core/read-handlers))
                         (gltf/metadata-descriptors)
                         :meshes)))
              (is (= :file (resource/source-type source-resource)))
              (is (resource/openable? source-resource))
              (is (= #{"/models/robot.gltf/images"
                       "/models/robot.gltf/materials"
                       "/models/robot.gltf/meshes"}
                     (proj-paths (resource/children source-resource))))
              (is (= #{"/models/robot.gltf/meshes/Mesh 0"}
                     (proj-paths (resource/children meshes-resource))))
              (is (resource/gltf-resource? mesh-resource))
              (is (resource/gltf-resource? material-resource))
              (is (resource/gltf-resource? image-resource))
              (is (= :file (resource/source-type mesh-resource)))
              (is (resource/read-only? mesh-resource))
              (is (false? (resource/openable? mesh-resource)))
              (is (= "icons/32/Icons_27-AT-Mesh.png"
                     (workspace/resource-icon mesh-resource)))
              (is (.contains (String. ^bytes (resource/resource->bytes material-resource)
                                      StandardCharsets/UTF_8)
                             "name: \"LibraryPaint\""))
              (is (Arrays/equals initial-png (resource/resource->bytes image-resource)))
              (is (= ["/models/geometry.bin"]
                     (mapv resource/proj-path external-buffer-resources)))
              (is (= "geometry.bin" (.uri buffer)))
              (is (= 42 (count (.buffer buffer)))))))))))

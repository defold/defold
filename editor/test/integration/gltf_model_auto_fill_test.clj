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

(ns integration.gltf-model-auto-fill-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(defn- gltf-content [scene-node-indices-json nodes-json meshes-json]
  (let [geometry-buffer-base64 "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"
        image-base64 "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Y9Z6L8AAAAASUVORK5CYII="]
    (str "{"
         "\"asset\":{\"version\":\"2.0\"},"
         "\"scene\":0,"
         "\"scenes\":[{\"nodes\":" scene-node-indices-json "}],"
         "\"nodes\":" nodes-json ","
         "\"meshes\":" meshes-json ","
         "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
         geometry-buffer-base64
         "\",\"byteLength\":66}],"
         "\"bufferViews\":["
         "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
         "{\"buffer\":0,\"byteOffset\":60,\"byteLength\":6}],"
         "\"accessors\":["
         "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
         "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
         "{\"bufferView\":2,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
         "\"samplers\":["
         "{\"wrapS\":10497,\"wrapT\":10497,\"minFilter\":9729,\"magFilter\":9729},"
         "{\"wrapS\":33071,\"wrapT\":33648,\"minFilter\":9728,\"magFilter\":9728}],"
         "\"images\":["
         "{\"name\":\"PaintAlbedo\",\"uri\":\"data:image/png;base64,"
         image-base64
         "\",\"mimeType\":\"image/png\"},"
         "{\"name\":\"ChromeMetallicRoughness\",\"uri\":\"data:image/png;base64,"
         image-base64
         "\",\"mimeType\":\"image/png\"}],"
         "\"textures\":["
         "{\"name\":\"PaintAlbedoTexture\",\"sampler\":0,\"source\":0},"
         "{\"name\":\"ChromeMetallicRoughnessTexture\",\"sampler\":1,\"source\":1}],"
         "\"materials\":["
         "{\"name\":\"Paint\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}},"
         "{\"name\":\"Chrome\",\"pbrMetallicRoughness\":{\"metallicRoughnessTexture\":{\"index\":1}}}]}")))

(defn- coalesced-property [node-id prop-kw]
  (get-in (properties/coalesce [(g/node-value node-id :_properties)])
          [:properties prop-kw]))

(defn- edit-property! [node-id prop-kw value]
  (properties/set-values! (coalesced-property node-id prop-kw) [value]))

(defn- material-bindings [model-node-id]
  (into (sorted-map)
        (map (fn [{:keys [name material textures]}]
               [name
                {:material (resource/proj-path material)
                 :textures (into (sorted-map)
                                 (map (fn [{:keys [sampler texture]}]
                                        [sampler (resource/proj-path texture)]))
                                 textures)}]))
        (g/node-value model-node-id :materials)))

(defn- model-state [model-node-id]
  {:mesh (some-> (test-util/prop model-node-id :mesh) resource/proj-path)
   :materials (material-bindings model-node-id)})

(deftest gltf-mesh-user-edit-auto-fill
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        model-file (io/file project-path "robot.model")
        gltf-file (io/file project-path "models/robot.gltf")
        multi-mesh-gltf-file (io/file project-path "models/two_meshes.gltf")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file!
        gltf-file
        (gltf-content
          "[0]"
          "[{\"mesh\":0,\"name\":\"Robot\"}]"
          (str "[{\"primitives\":["
               "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":0},"
               "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":1}]}]")))
      (fs/create-file!
        multi-mesh-gltf-file
        (gltf-content
          "[0,1]"
          (str "[{\"mesh\":0,\"name\":\"PaintNode\"},"
               "{\"mesh\":1,\"name\":\"ChromeNode\"}]")
          (str "[{\"name\":\"PaintMesh\",\"primitives\":["
               "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":0}]},"
               "{\"name\":\"ChromeMesh\",\"primitives\":["
               "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":1}]}]")))
      (fs/create-file!
        model-file
        (str "mesh: \"/builtins/assets/gltf/cube.gltf\"\n"
             "materials {\n"
             "  name: \"default\"\n"
             "  material: \"/builtins/materials/model.material\"\n"
             "  textures {\n"
             "    sampler: \"tex0\"\n"
             "    texture: \"/builtins/graphics/particle_blob.png\"\n"
             "  }\n"
             "}\n"))

      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              model-node-id (test-util/resource-node project "/robot.model")
              gltf-resource (workspace/find-resource workspace "/models/robot.gltf")
              multi-mesh-gltf-resource (workspace/find-resource workspace "/models/two_meshes.gltf")
              initial-state (model-state model-node-id)
              generated-state
              {:mesh "/models/robot.gltf"
               :materials
               {"Chrome"
                {:material "/models/robot.gltf/materials/1.material"
                 :textures
                 {"PbrMetallicRoughness_metallicRoughnessTexture"
                  "/models/robot.gltf/images/1.png"}}

                "Paint"
                {:material "/models/robot.gltf/materials/0.material"
                 :textures
                 {"PbrMetallicRoughness_baseColorTexture"
                  "/models/robot.gltf/images/0.png"}}}}]
          (is (= {:mesh "/builtins/assets/gltf/cube.gltf"
                  :materials
                  {"default"
                   {:material "/builtins/materials/model.material"
                    :textures {"tex0" "/builtins/graphics/particle_blob.png"}}}}
                 initial-state))

          (testing "non-user graph edits neither prompt nor auto-fill"
            (let [dialog-call-count (atom 0)]
              (with-redefs [dialogs/make-confirmation-dialog
                            (fn [_localization _props]
                              (swap! dialog-call-count inc)
                              true)]
                (g/set-property! model-node-id :mesh gltf-resource))

              (is (zero? @dialog-call-count))
              (is (= (assoc initial-state :mesh "/models/robot.gltf")
                     (model-state model-node-id)))

              (g/undo! :undo/global)
              (is (= initial-state (model-state model-node-id)))))

          (testing "declining changes the mesh but preserves existing bindings"
            (let [dialog-call-count (atom 0)]
              (with-redefs [dialogs/make-confirmation-dialog
                            (fn [_localization _props]
                              (swap! dialog-call-count inc)
                              false)]
                (edit-property! model-node-id :mesh gltf-resource))

              (is (= 1 @dialog-call-count))
              (is (= (assoc initial-state :mesh "/models/robot.gltf")
                     (model-state model-node-id)))

              (g/undo! :undo/global)
              (is (= initial-state (model-state model-node-id)))))

          (testing "multi-mesh sources wait for a mesh selection and only bind its assets"
            (let [dialog-call-count (atom 0)
                  selected-mesh-state
                  {:mesh "/models/two_meshes.gltf"
                   :materials
                   {"Chrome"
                    {:material "/models/two_meshes.gltf/materials/1.material"
                     :textures
                     {"PbrMetallicRoughness_metallicRoughnessTexture"
                      "/models/two_meshes.gltf/images/1.png"}}}}]
              (with-redefs [dialogs/make-confirmation-dialog
                            (fn [_localization _props]
                              (swap! dialog-call-count inc)
                              true)]
                (edit-property! model-node-id :mesh multi-mesh-gltf-resource)

                (is (zero? @dialog-call-count))
                (is (= (assoc initial-state :mesh "/models/two_meshes.gltf")
                       (model-state model-node-id)))
                (is (= [[-1 ""] [0 "PaintMesh"] [1 "ChromeMesh"]]
                       (get-in (g/node-value model-node-id :_properties)
                               [:properties :mesh-index :edit-type :options])))

                (edit-property! model-node-id :mesh-index 1))

              (is (= 1 @dialog-call-count))
              (is (= "ChromeMesh" (test-util/prop model-node-id :mesh-name)))
              (is (= 1 (test-util/prop model-node-id :mesh-index)))
              (is (= selected-mesh-state (model-state model-node-id)))

              (g/undo! :undo/global)
              (is (= -1 (test-util/prop model-node-id :mesh-index)))
              (is (= (assoc initial-state :mesh "/models/two_meshes.gltf")
                     (model-state model-node-id)))

              (g/undo! :undo/global)
              (is (= initial-state (model-state model-node-id)))))

          (testing "accepting atomically replaces all material and texture bindings"
            (let [dialog-call-count (atom 0)
                  initial-undo-stack-count (g/undo-stack-count :undo/global)
                  tx-result
                  (with-redefs [dialogs/make-confirmation-dialog
                                (fn [_localization _props]
                                  (swap! dialog-call-count inc)
                                  true)]
                    (edit-property! model-node-id :mesh gltf-resource))]

              (is (= 1 @dialog-call-count))
              (is (= 4 (count (g/tx-nodes-added tx-result))))
              (is (= generated-state (model-state model-node-id)))
              (is (= (inc initial-undo-stack-count)
                     (g/undo-stack-count :undo/global)))

              (g/undo! :undo/global)
              (is (= initial-state (model-state model-node-id)))

              (g/redo! :undo/global)
              (is (= generated-state (model-state model-node-id))))))))))

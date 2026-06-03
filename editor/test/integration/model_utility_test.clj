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

(ns integration.model-utility-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.model-loader :as model-loader]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.bob.util TextureUtil]
           [com.dynamo.rig.proto Rig$MeshSet]
           [java.nio.file Files]))

(defn- load-scene [workspace project file-path]
  (let [resource (workspace/file-resource workspace file-path)
        node-id (project/get-resource-node project resource)]
    (model-loader/load-scene node-id resource (project/settings project))))

(deftest mesh-normals
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/quad.gltf")
          content mesh-set
          normals (->> (get-in content [:models 0 :meshes 0 :normals])
                       (partition 3))]
      (is (seq normals))
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  normals)))))

(deftest mesh-texcoords
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/quad.gltf")
          content mesh-set
          positions (->> (get-in content [:models 0 :meshes 0 :positions])
                         (partition 3)
                         (mapv vec))
          texcoords (->> (get-in content [:models 0 :meshes 0 :texcoord0])
                         (partition 2)
                         (mapv vec))]
      (is (seq texcoords))
      (is (= [[-0.5 0.0 0.5]
              [0.5 0.0 0.5]
              [-0.5 0.0 -0.5]
              [0.5 0.0 -0.5]]
             positions))
      (is (= [[0.0 0.0]
              [1.0 0.0]
              [0.0 1.0]
              [1.0 1.0]]
             texcoords)))))

(deftest bones
  (test-util/with-loaded-project
    (let [{:keys [animation-set bones]} (load-scene workspace project "/mesh/treasure_chest.gltf")]
      (is (= 3 (count bones)))
      (is (set/subset? (:bone-list animation-set) (set (map :id bones)))))))

(deftest external-buffers
  (test-util/with-loaded-project
    (let [{:keys [buffers]} (load-scene workspace project "/mesh/triangle/gltf/Triangle.gltf")]
      (is (= 1 (count buffers)))
      (is (= "simpleTriangle.bin" (.uri (first buffers))))
      (is (= 44 (count (.buffer (first buffers))))))))

(deftest morph-target-build-data
  (test-util/with-loaded-project
    (let [resource (workspace/file-resource workspace "/mesh/morph_weights_anim.gltf")
          node-id (project/get-resource-node project resource)
          {:keys [mesh-set morph-target-textures]} (model-loader/load-scene node-id
                                                                            resource
                                                                            (project/settings project))
          mesh (-> mesh-set :models first :meshes first)]
      (is (= 2 (:morph-target-count mesh)))
      (is (= [0.0 0.0] (:morph-base-weights mesh)))
      (is (= 1 (count morph-target-textures)))
      (is (string/starts-with? (:morph-target-texture mesh) "__morph_target_texture_"))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (let [build-results (test-util/build-node! node-id)
            mesh-set-build-resource (->> (:artifacts build-results)
                                         (map :resource)
                                         (filter #(= "meshsetc" (resource/ext %)))
                                         first)]
        (when (is (some? mesh-set-build-resource))
          (let [built-mesh-set (protobuf/bytes->map-with-defaults
                                Rig$MeshSet
                                (Files/readAllBytes (.toPath (io/as-file mesh-set-build-resource))))
                built-mesh (-> built-mesh-set :models first :meshes first)
                morph-target-texture (:morph-target-texture built-mesh)
                texture-file (workspace/build-path workspace morph-target-texture)
                texture-image (-> (Files/readAllBytes (.toPath texture-file))
                                  TextureUtil/textureResourceBytesToTextureImage
                                  protobuf/pb->map-with-defaults)
                first-alternative (first (:alternatives texture-image))]
            (is (string/ends-with? morph-target-texture ".texturec"))
            (is (false? (string/starts-with? morph-target-texture "__morph_target_texture_")))
            (is (.exists texture-file))
            (is (= :type-2d-array (:type texture-image)))
            (is (= 6 (:count texture-image)))
            (is (= 1 (:depth first-alternative)))
            (is (= 1 (:original-depth first-alternative)))))))))

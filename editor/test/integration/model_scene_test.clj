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

(ns integration.model-scene-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gl.vertex2 :as vtx]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [java.nio ByteBuffer ByteOrder]
           [javax.vecmath Point3d]))

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/builtins/assets/gltf/cube.gltf")
          scene (g/node-value node-id :scene)
          aabb (:aabb scene)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)
          dist (.distance max min)]
      (is (< 1 dist 2)))))

(deftest gltf-valid-scene
  (test-util/with-loaded-project
    ;; Valid .gltf should load successfully via model_loader and glTF validation.
    (let [node-id (test-util/resource-node project "/mesh/triangle/gltf/Triangle.gltf")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (not (g/error? scene))))
    ;; Valid .glb should load successfully via model_loader and glTF validation.
    (let [node-id (test-util/resource-node project "/mesh/triangle/glb/valid.glb")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (not (g/error? scene))))))

(deftest gltf-invalid-scene
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/accessor_element_out_of_max_bound.gltf")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (g/error? scene))
      (let [errors (g/flatten-errors scene)
            msg    (some-> errors g/error-message test-util/localization)]
        (is (re-find #"glTF validation failed" msg))
        (is (re-find #"ACCESSOR_MAX_MISMATCH" msg))
        (is (re-find #"ACCESSOR_ELEMENT_OUT_OF_MAX_BOUND" msg))))))

(deftest external-buffer-change-invalidates-content
  (test-util/with-scratch-project "test/resources/test_project"
    (let [model-scene (test-util/resource-node project "/mesh/triangle/gltf/Triangle.gltf")
          collision-object (test-util/resource-node project "/collision_object/mesh_shape.collisionobject")
          collision-shape (:node-id (test-util/outline collision-object [0]))
          model-scene-resource (workspace/find-resource workspace "/mesh/triangle/gltf/Triangle.gltf")
          buffer-resource (workspace/find-resource workspace "/mesh/triangle/gltf/simpleTriangle.bin")
          buffer-node (project/get-resource-node project buffer-resource)
          first-x (fn []
                    (-> (g/node-value model-scene :content)
                        (get-in [:mesh-set :models 0 :meshes 0 :positions 0])
                        double))
          collision-preview-position-buffer
          (fn []
            (get-in (g/node-value collision-shape :selected-collision-mesh-renderable)
                    [:primitives 0 :position-buffer]))]
      (g/set-property! collision-shape :mesh-scene model-scene-resource)
      (g/set-properties! collision-shape :mesh-name "Triangle" :mesh-index 0)

      (is (= ["simpleTriangle.bin"] (g/node-value model-scene :source-value)))
      (is (= [[buffer-node :sha256]]
             (g/sources-of model-scene :external-buffer-sha256s)))
      (is (coll/empty? (get-in (g/node-value model-scene :content)
                               [:mesh-set :raw-models 0 :meshes])))
      (is (= 0.0 (first-x)))

      (let [initial-mesh-set-content-hash (:content-hash (g/node-value model-scene :mesh-set-build-target))
            initial-collision-content-hash (-> (g/node-value collision-object :build-targets)
                                               first
                                               :content-hash)
            initial-collision-preview-position-buffer (collision-preview-position-buffer)
            bytes (resource/resource->bytes buffer-resource)]
        (-> (ByteBuffer/wrap bytes)
            (.order ByteOrder/LITTLE_ENDIAN)
            (.putFloat 8 0.5))
        (test-support/write-until-new-mtime (io/as-file buffer-resource) bytes)

        (workspace/resource-sync! workspace)

        (is (= 0.5 (first-x)))
        (is (not= initial-mesh-set-content-hash
                  (:content-hash (g/node-value model-scene :mesh-set-build-target))))
        (is (not= initial-collision-content-hash
                  (-> (g/node-value collision-object :build-targets)
                      first
                      :content-hash)))
        (is (not (identical? initial-collision-preview-position-buffer
                             (collision-preview-position-buffer))))))))

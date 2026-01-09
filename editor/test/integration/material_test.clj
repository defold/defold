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

(ns integration.material-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")
          samplers (g/node-value node-id :samplers)]
      (is (some? (g/node-value node-id :shader)))
      (is (= 1 (count samplers))))))

(deftest material-pbr-parameters
  ;; Test that all exposed PBR parameters are found, and that they are true
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test_pbr_materials.material")
          pbr-parameters (get-in (g/node-value node-id :build-targets) [0 :user-data :material-desc-with-build-resources :pbr-parameters])]
      (is (= {:has-iridescence true
              :has-metallic-roughness true
              :has-volume true
              :has-clearcoat true
              :has-sheen true
              :has-specular true
              :has-transmission true
              :has-specular-glossiness true
              :has-ior true
              :has-parameters true
              :has-emissive-strength true}
             pbr-parameters)))))

(deftest material-combined-shaders
  ;; Test that materials that have the same .vp and .fp pair will reference the same .sp file
  (test-util/with-loaded-project
    (let [node-id-material-1 (test-util/resource-node project "/materials/test_combined_shader_1.material")
          node-id-material-2 (test-util/resource-node project "/materials/test_combined_shader_2.material")
          node-id-material-3 (test-util/resource-node project "/materials/test_combined_shader_3.material")
          build-targets-material-1 (g/node-value node-id-material-1 :build-targets)
          build-targets-material-2 (g/node-value node-id-material-2 :build-targets)
          build-targets-material-3 (g/node-value node-id-material-3 :build-targets)
          sp-dep-material-1 (get-in build-targets-material-1 [0 :deps 0])
          sp-dep-material-2 (get-in build-targets-material-2 [0 :deps 0])
          sp-dep-material-3 (get-in build-targets-material-3 [0 :deps 0])]
      (is (= (g/node-value node-id-material-1 :vertex-program)
             (g/node-value node-id-material-2 :vertex-program)
             (g/node-value node-id-material-3 :vertex-program)))
      (is (= (g/node-value node-id-material-1 :fragment-program)
             (g/node-value node-id-material-2 :fragment-program)
             (g/node-value node-id-material-3 :fragment-program)))
      ;; Check that the material content is different between the three materials
      (is (not (= (get-in build-targets-material-1 [0 :content-hash])
                  (get-in build-targets-material-2 [0 :content-hash])
                  (get-in build-targets-material-3 [0 :content-hash]))))
      (is (and some? (:resource sp-dep-material-1)
               ;; Same resource path
               (= (resource/proj-path (:resource sp-dep-material-1))
                  (resource/proj-path (:resource sp-dep-material-2))
                  (resource/proj-path (:resource sp-dep-material-3)))
               ;; Same content hash of the dependency
               (= (:content-hash sp-dep-material-1)
                  (:content-hash sp-dep-material-2)
                  (:content-hash sp-dep-material-3)))))))

(deftest material-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")]
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.vp")]]
        (test-util/with-prop [node-id :vertex-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets)))))
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.fp")]]
        (test-util/with-prop [node-id :fragment-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets))))))))

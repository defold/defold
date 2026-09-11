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
            [editor.form :as form]
            [editor.material]
            [editor.pipeline.shader-gen :as shader-gen]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.fn :as fn]))

(deftest shader-transpilation-is-memoized
  (let [transpile-shader-source-cached (var-get (ns-resolve 'editor.material 'transpile-shader-source-cached))
        transpile-count (atom 0)]
    (fn/clear-memoized! transpile-shader-source-cached)
    (try
      (with-redefs [shader-gen/transpile-shader-source
                    (fn [_shader-proj-path shader-source & args]
                      (swap! transpile-count inc)
                      (if (= "invalid source" shader-source)
                        (throw (Exception. "Invalid shader source."))
                        args))]
        (is (= (transpile-shader-source-cached "/test.vp" "source" 0 "mediump" "highp")
               (transpile-shader-source-cached "/test.vp" "source" 0 "mediump" "highp")))
        (is (= 1 @transpile-count))

        (transpile-shader-source-cached "/test.vp" "different source" 0 "mediump" "highp")
        (is (= 2 @transpile-count))

        (is (thrown? Exception
                     (transpile-shader-source-cached "/test.vp" "invalid source" 0 "mediump" "highp")))
        (is (thrown? Exception
                     (transpile-shader-source-cached "/test.vp" "invalid source" 0 "mediump" "highp")))
        (is (= 4 @transpile-count)))
      (finally
        (fn/clear-memoized! transpile-shader-source-cached)))))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test_samplers.material")
          samplers (g/node-value node-id :samplers)
          sampler (first samplers)]
      (is (some? (g/node-value node-id :shader)))
      (is (= 1 (count samplers)))
      (is (= :wrap-mode-repeat (:wrap-w sampler)))
      (is (not (contains? (first (:samplers (g/node-value node-id :save-value))) :wrap-w)))
      (prop! node-id :samplers [(assoc sampler :wrap-w :wrap-mode-clamp-to-edge)])
      (is (= :wrap-mode-clamp-to-edge
             (get-in (g/node-value node-id :save-value) [:samplers 0 :wrap-w]))))))

(deftest missing-material-constant-value
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test_missing_constant_value.material")]
      (is (= protobuf/vector4-zero (get-in (g/node-value node-id :fragment-constants) [0 :value])))
      (is (= [protobuf/vector4-zero] (get-in (g/node-value node-id :save-value) [:fragment-constants 0 :value]))))))

(deftest matrix4-material-constant-value
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test_matrix4_constant.material")
          fragment-constants (g/node-value node-id :fragment-constants)
          saved-constants (:fragment-constants (g/node-value node-id :save-value))]
      (is (= (mapv float (range 1.0 17.0))
             (:value (first fragment-constants))))
      (is (= (mapv float (repeat 16 0.0))
             (:value (second fragment-constants))))
      (is (= [[1.0 2.0 3.0 4.0]
              [5.0 6.0 7.0 8.0]
              [9.0 10.0 11.0 12.0]
              [13.0 14.0 15.0 16.0]]
             (mapv #(mapv double %) (:value (first saved-constants)))))
      (is (= 4 (count (:value (second saved-constants)))))
      (is (= (into (mapv float [1.0 2.0 3.0 4.0])
                   (repeat 12 protobuf/float-zero))
             (:value (nth fragment-constants 2))))
      (is (= 4 (count (:value (nth saved-constants 2)))))
      (is (not (g/error? (g/node-value node-id :shader)))))))

(deftest matrix4-material-constant-type-switch
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test_matrix4_constant.material")
          set-constant-type! (fn [constant-type]
                               (let [constants (assoc-in (g/node-value node-id :fragment-constants)
                                                         [0 :type] constant-type)]
                                 (g/transact {:undoable false}
                                   (form/set-value (:form-ops (g/node-value node-id :form-data))
                                                   [:fragment-constants]
                                                   constants))))]
      (set-constant-type! :constant-type-user)
      (is (= 4 (count (:value (first (g/node-value node-id :fragment-constants))))))

      (set-constant-type! :constant-type-user-matrix4)
      (is (= 16 (count (:value (first (g/node-value node-id :fragment-constants)))))))))

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
          node-id-material-with-uniforms (test-util/resource-node project "/materials/test.material")
          shader-material-1 (g/node-value node-id-material-1 :shader)
          shader-material-2 (g/node-value node-id-material-2 :shader)
          shader-material-3 (g/node-value node-id-material-3 :shader)
          shader-material-with-uniforms (g/node-value node-id-material-with-uniforms :shader)
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
      (is (= (:request-data shader-material-1)
             (:request-data shader-material-2)
             (:request-data shader-material-3)
             (:request-data shader-material-with-uniforms)))
      ;; Each material needs its own mutable OpenGL uniform state.
      (is (distinct? (:request-id shader-material-1)
                     (:request-id shader-material-2)
                     (:request-id shader-material-3)
                     (:request-id shader-material-with-uniforms)))
      (is (not= (:uniforms shader-material-1)
                (:uniforms shader-material-with-uniforms)))
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

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
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.form :as form]
            [editor.material]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.murmur :as murmur])
  (:import [com.dynamo.bob.pipeline MaterialBuilder]
           [com.dynamo.graphics.proto Graphics$ShaderDesc Graphics$ShaderDesc$Shader Graphics$ShaderDesc$ShaderReflection Graphics$ShaderDesc$ShaderType]
           [com.dynamo.render.proto Material$MaterialDesc]
           [com.google.protobuf ByteString]))

;; The editor build function must emit the same native state and hash as Bob,
;; independently of resource-path fusion and material metadata.
(deftest shared-material-render-state
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test.material")
          build-resource (workspace/make-build-resource (g/node-value node-id :resource))
          program-resource (workspace/make-build-resource (g/node-value node-id :vertex-program))
          reflection (Graphics$ShaderDesc$ShaderReflection/getDefaultInstance)
          desc {:name "material"
                :vertex-program ""
                :fragment-program ""
                :vertex-space :vertex-space-local
                :program program-resource}
          user-data {:material-desc-with-build-resources desc
                     :shader-reflection (.toByteString reflection)}
          result (#'editor.material/build-material
                   build-resource
                   {program-resource program-resource}
                   user-data)
          compiled (Material$MaterialDesc/parseFrom ^bytes (:content result))
          expected (MaterialBuilder/buildRenderState
                     (protobuf/map->pb Material$MaterialDesc
                                       (assoc desc :name "renamed" :tags ["shadow"] :program "/other.spc"))
                     reflection)]
      (is (.hasRenderHash compiled))
      (is (string? (:content-hash
                     (bt/with-content-hash
                       {:resource build-resource
                        :build-fn @#'editor.material/build-material
                        :user-data user-data}))))
      (is (.hasRenderState compiled))
      (is (= (.getRenderHash expected) (.getRenderHash compiled)))
      (is (= (.getRenderState expected) (.getRenderState compiled))))))

;; Editor and Bob must generate aliases only for active array fallback samplers,
;; preserving the same shared material hash for native and fallback variants.
(deftest shared-material-sampler-aliases
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/materials/test.material")
          build-resource (workspace/make-build-resource (g/node-value node-id :resource))
          program-resource (workspace/make-build-resource (g/node-value node-id :vertex-program))
          samplers (mapv (fn [name]
                           {:name name
                            :wrap-u :wrap-mode-repeat
                            :wrap-v :wrap-mode-repeat
                            :filter-min :filter-mode-min-linear
                            :filter-mag :filter-mode-mag-linear
                            :name-indirections [(murmur/hash64 "stale_alias")]})
                         ["ordinary" "array" "unused"])
          hashes
          (mapv (fn [fallback]
                  (let [shader-desc (protobuf/map->pb Graphics$ShaderDesc
                                                      {:shaders [{:source ByteString/EMPTY
                                                                  :shader-type :shader-type-fragment
                                                                  :language :language-glsl-sm120
                                                                  :variant-texture-array fallback}]
                                                       :reflection {:textures [{:name "ordinary"
                                                                                :name-hash (murmur/hash64 "ordinary")
                                                                                :type {:shader-type :shader-type-sampler2d}}
                                                                               {:name "array"
                                                                                :name-hash (murmur/hash64 "array")
                                                                                :type {:shader-type :shader-type-sampler2d-array}}]}})
                        reflection (.getReflection shader-desc)
                        editor-samplers (#'editor.material/build-target-samplers samplers 2 shader-desc)
                        desc {:name "material"
                              :vertex-program ""
                              :fragment-program ""
                              :vertex-space :vertex-space-local
                              :max-page-count 2
                              :program program-resource
                              :samplers editor-samplers}
                        bob-builder (.toBuilder
                                      (protobuf/map->pb Material$MaterialDesc
                                                        (assoc desc
                                                          :program "/other.spc"
                                                          :samplers (mapv (fn [sampler]
                                                                            (assoc sampler :name-hash (murmur/hash64 (:name sampler))))
                                                                          samplers))))
                        result (#'editor.material/build-material
                                 build-resource
                                 {program-resource program-resource}
                                 {:material-desc-with-build-resources desc
                                  :shader-reflection (.toByteString reflection)})
                        compiled (Material$MaterialDesc/parseFrom ^bytes (:content result))]
                    (MaterialBuilder/applyVariantTextureArray bob-builder shader-desc)
                    (let [expected (MaterialBuilder/buildRenderState (.build bob-builder) reflection)]
                      (is (= [[] (if fallback (mapv murmur/hash64 ["array_0" "array_1"]) []) []]
                             (mapv #(vec (:name-indirections %)) editor-samplers)))
                      (is (= (.getSamplersList expected) (.getSamplersList compiled)))
                      (is (= 2 (.getSamplersCount (.getRenderState compiled))))
                      (is (= (.getRenderState expected) (.getRenderState compiled)))
                      (is (= (.getRenderHash expected) (.getRenderHash compiled)))
                      (.getRenderHash compiled))))
                [false true])]
      (is (apply = hashes)))))

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
              :has-fixed-layout true
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
      (is (= (dissoc (:request-data shader-material-1) :proj-path)
             (dissoc (:request-data shader-material-2) :proj-path)
             (dissoc (:request-data shader-material-3) :proj-path)
             (dissoc (:request-data shader-material-with-uniforms) :proj-path)))
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

;; Verifies local builds retain existing shader formats alongside DXIL and its
;; root signature on every editor host for remote Windows Build and Run.
(deftest material-shaders-for-remote-engines
  (test-util/with-loaded-project
    (let [settings (test-util/resource-node project "/game.project")
          material (test-util/resource-node project "/materials/test.material")]
      (doseq [exclude-gles-sm100 [false true]]
        (test-util/set-setting! settings ["shader" "exclude_gles_sm100"] exclude-gles-sm100)
        (let [targets (g/node-value material :build-targets)]
          (is (not (g/error? targets)))
          (when-not (g/error? targets)
            (let [^Graphics$ShaderDesc shader
                  (get-in targets [0 :deps 0 :user-data :shader-desc])

                  expected-languages
                  (cond-> #{:language-glsl-sm330 :language-gles-sm300 :language-gles-sm100
                            :language-glsl-sm430 :language-spirv :language-msl-22 :language-hlsl-60}
                    exclude-gles-sm100 (disj :language-gles-sm100))]
              (is (some? shader))
              (when shader
                (is (not (.isEmpty (.getHlslRootSignature shader))))
                (doseq [shader-type [Graphics$ShaderDesc$ShaderType/SHADER_TYPE_VERTEX
                                     Graphics$ShaderDesc$ShaderType/SHADER_TYPE_FRAGMENT]]
                  (is (= expected-languages
                         (coll/into-> (.getShadersList shader)
                           #{}
                           (filter #(= shader-type (.getShaderType ^Graphics$ShaderDesc$Shader %)))
                           (map #(protobuf/pb-enum->val (.getLanguage ^Graphics$ShaderDesc$Shader %)))))))))))))))

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

;; Vector shader builds require an explicit compatibility choice and track
;; changes to that setting.
(deftest vector-shader-requires-explicit-gles2-exclusion
  (test-util/with-loaded-project
    (let [settings (test-util/resource-node project "/game.project")
          material (test-util/resource-node project "/builtins/fonts/font-vector.material")]
      (doseq [exclude [false true false]]
        (test-util/set-setting! settings ["shader" "exclude_gles_sm100"] exclude)
        (let [error (test-util/build-error! material)]
          (if exclude
            (is (nil? error))
            (do
              (is (g/error-fatal? error))
              (is (string/includes? (test-util/localization (g/error-message error)) "shader.exclude_gles_sm100"))
              (is (string/includes? (test-util/localization (g/error-message error)) "OpenGL ES 2.0 / WebGL 1.0")))))
        (is (= exclude (test-util/get-setting settings ["shader" "exclude_gles_sm100"])))))))

;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.code.shader-compilation
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graphics.types :as graphics.types]
            [editor.pipeline.shader-gen :as shader-gen]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.array :as array]
            [util.eduction :as e])
  (:import [com.dynamo.bob.pipeline ShaderProgramBuilderEditor]
           [com.dynamo.bob.pipeline.shader ShaderCompilePipeline$ShaderModuleDesc]
           [com.dynamo.graphics.proto Graphics$ShaderDesc Graphics$ShaderDesc$Language]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def built-pb-class Graphics$ShaderDesc)

(defn- pb-shader-languages [shader-languages]
  (array/from-type Graphics$ShaderDesc$Language (mapv shader-gen/shader-language->pb-shader-language shader-languages)))

(def ^:private default-shader-languages
  ;; TODO: WGSL support (:language-wgsl)
  ;; TODO(question): Does the order matter?
  [:language-glsl-sm330 :language-gles-sm300 :language-gles-sm100 :language-glsl-sm430 :language-spirv])

(defonce ^:private ^"[Lcom.dynamo.graphics.proto.Graphics$ShaderDesc$Language;" pb-default-shader-languages
  (pb-shader-languages default-shader-languages))

(defonce ^:private ^"[Lcom.dynamo.graphics.proto.Graphics$ShaderDesc$Language;" pb-shader-languages-without-gles-sm100
  (pb-shader-languages (e/remove #(= :language-gles-sm100 %) default-shader-languages)))

(defn- make-shader-module-desc
  ^ShaderCompilePipeline$ShaderModuleDesc [^String resource-proj-path ^String shader-source]
  {:pre [(string? resource-proj-path)
         (pos? (count resource-proj-path))
         (string? shader-source)
         (pos? (count shader-source))]}
  (let [pb-shader-type (graphics.types/filename-pb-shader-type resource-proj-path)
        shader-module-desc (ShaderCompilePipeline$ShaderModuleDesc.)]
    (set! (. shader-module-desc source) shader-source)
    (set! (. shader-module-desc resourcePath) resource-proj-path)
    (set! (. shader-module-desc type) pb-shader-type)
    shader-module-desc))

(defn- error-string->error-value [^String error-string]
  (g/error-fatal (string/trim error-string)))

(defn- build-shader [build-resource _dep-resources user-data]
  {:pre [(workspace/build-resource? build-resource)]}
  (let [{:keys [^Graphics$ShaderDesc shader-desc compile-warning-messages]} user-data
        compile-error-values (mapv error-string->error-value compile-warning-messages)]
    (g/precluding-errors compile-error-values
      {:resource build-resource
       :content (protobuf/pb->bytes shader-desc)})))

(defn make-shader-build-target [node-id shader-source-infos max-page-count exclude-gles-sm100]
  {:pre [(g/node-id? node-id)
         (vector? shader-source-infos)
         (pos? (count shader-source-infos))
         (integer? max-page-count)]}
  (let [workspace (resource/workspace (:resource (first shader-source-infos)))

        build-resource (workspace/make-placeholder-build-resource workspace "sp")

        shader-infos
        (mapv (fn [{:keys [resource shader-source]}]
                {:pre [(workspace/source-resource? resource)
                       (string? shader-source)
                       (pos? (count shader-source))]}
                {:proj-path (resource/proj-path resource)
                 :shader-source shader-source})
              shader-source-infos)

        shader-module-descs (e/map (fn [{:keys [proj-path shader-source]}]
                                     (make-shader-module-desc proj-path shader-source))
                                   shader-infos)
        build-resource-path (resource/path build-resource)
        shader-module-descs-array (into-array ShaderCompilePipeline$ShaderModuleDesc shader-module-descs)
        pb-shader-languages (if exclude-gles-sm100
                              pb-shader-languages-without-gles-sm100
                              pb-default-shader-languages)

        shader-desc-build-result (ShaderProgramBuilderEditor/makeShaderDescWithVariants build-resource-path shader-module-descs-array pb-shader-languages (int max-page-count))
        shader-desc (.-shaderDesc shader-desc-build-result)]
    (bt/with-content-hash
      {:node-id node-id
       :resource build-resource
       :shader-reflection (when shader-desc (.getReflection shader-desc))
       :build-fn build-shader
       :user-data {:shader-desc shader-desc
                   :compile-warning-messages (.buildWarnings shader-desc-build-result)}})))

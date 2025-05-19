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

(ns editor.pipeline.shader-gen
  (:require [clojure.string :as string]
            [util.coll :as coll :refer [pair]])
  (:import [com.dynamo.bob CompileExceptionError]
           [com.dynamo.bob.pipeline ShaderProgramBuilder ShaderProgramBuilderEditor ShaderUtil$Common$GLSLCompileResult Shaderc$ShaderResource]
           [com.dynamo.bob.pipeline.shader SPIRVReflector]
           [com.dynamo.graphics.proto Graphics$ShaderDesc$Language Graphics$ShaderDesc$ShaderDataType Graphics$ShaderDesc$ShaderType]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn filename->shader-type [^String filename]
  {:pre [(string? filename)
         (pos? (count filename))]}
  (let [type-ext (string/lower-case (FilenameUtils/getExtension filename))]
    (case type-ext
      "vp" :shader-type-vertex
      "fp" :shader-type-fragment
      "cp" :shader-type-compute)))

(defn- shader-type->pb-shader-type
  ^Graphics$ShaderDesc$ShaderType [shader-type]
  (case shader-type
    :shader-type-vertex Graphics$ShaderDesc$ShaderType/SHADER_TYPE_VERTEX
    :shader-type-fragment Graphics$ShaderDesc$ShaderType/SHADER_TYPE_FRAGMENT
    :shader-type-compute Graphics$ShaderDesc$ShaderType/SHADER_TYPE_COMPUTE))

(defn filename->pb-shader-type
  ^Graphics$ShaderDesc$ShaderType [^String filename]
  (-> filename
      filename->shader-type
      shader-type->pb-shader-type))

(defn shader-language->pb-shader-language
  ^Graphics$ShaderDesc$Language [shader-language]
  (case shader-language
    :language-glsl-sm120 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM120
    :language-glsl-sm430 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM430
    :language-gles-sm100 Graphics$ShaderDesc$Language/LANGUAGE_GLES_SM100
    :language-gles-sm300 Graphics$ShaderDesc$Language/LANGUAGE_GLES_SM300
    :language-glsl-sm330 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM330
    :language-spirv Graphics$ShaderDesc$Language/LANGUAGE_SPIRV))

;; A resource namespace is the first literal up until the first dot in a
;; resource binding. For example, if we have a uniform buffer with some nested
;; data types:
;;
;;   struct MyMaterial {
;;     vec4 diffuse;
;;     vec4 specular;
;;   };
;;
;;   uniform my_uniforms {
;;     MyMaterial material;
;;   };
;;
;; When crosscompiled to SM120 (which is used by the editor), we will get two
;; uniforms:
;;   _<id>.material.diffuse
;;   _<id>.material.specular
;;
;; To be able to map this in a material constant, we need to strip the namespace
;; from the reflected data when the shader is created (see the implementation of
;; editor.gl.shader/make-shader-program) since there is no way a user can know
;; what the generated id will be for older shaders.
(defn- resource-binding-namespaces [^SPIRVReflector spirv-reflector]
  ;; Storage buffers (also known as SSBOs) will need the same mapping as uniform
  ;; buffers, but since we don't support them in the editor yet, we don't gather
  ;; their namespaces here.
  (mapv (fn [^Shaderc$ShaderResource uniform-buffer-object]
          (str "_" (.id uniform-buffer-object)))
        (.getUBOs spirv-reflector)))

(defn- pb-shader-data-type->vector-type+data-type [^Graphics$ShaderDesc$ShaderDataType pb-shader-data-type]
  (condp = pb-shader-data-type
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_INT (pair :vector-type-scalar :type-int)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_UINT (pair :vector-type-scalar :type-unsigned-int)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_FLOAT (pair :vector-type-scalar :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_VEC2 (pair :vector-type-vec2 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_VEC3 (pair :vector-type-vec3 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_VEC4 (pair :vector-type-vec4 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_MAT2 (pair :vector-type-mat2 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_MAT3 (pair :vector-type-mat3 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_MAT4 (pair :vector-type-mat4 :type-float)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_UVEC2 (pair :vector-type-vec2 :type-unsigned-int)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_UVEC3 (pair :vector-type-vec3 :type-unsigned-int)
    Graphics$ShaderDesc$ShaderDataType/SHADER_TYPE_UVEC4 (pair :vector-type-vec4 :type-unsigned-int)))

(defn- make-attribute-reflection-info [^Shaderc$ShaderResource attribute]
  (let [shader-resource-type (.-type attribute)
        pb-shader-data-type (ShaderProgramBuilder/resourceTypeToShaderDataType shader-resource-type)
        [vector-type data-type] (pb-shader-data-type->vector-type+data-type pb-shader-data-type)]
    ;; We want to keep the keys here in sync with the attribute-info maps in the
    ;; editor.graphics module. The idea is that you should be able to amend this
    ;; map with attribute-info keys from the material to get a fully formed
    ;; attribute-info map.
    {:name (.-name attribute)
     :location (.-location attribute)
     :vector-type vector-type
     :data-type data-type}))

(def ^:private transpile-target-pb-shader-language
  ;; Use the old GLES2-compatible shaders for rendering in the editor.
  (shader-language->pb-shader-language :language-glsl-sm120))

(defn- decorate-transpile-error
  [^Exception cause shader-type ^String shader-proj-path ^String shader-source max-page-count & extra-key-value-pairs]
  (let [ex-message
        (format "Failed to transpile shader '%s' with max-page-count %d."
                shader-proj-path
                max-page-count)

        ex-map
        (cond-> {:ex-type ::shader-transpile-error
                 :shader-type shader-type
                 :shader-proj-path shader-proj-path
                 :shader-source shader-source
                 :max-page-count max-page-count}

                (coll/not-empty extra-key-value-pairs)
                (into (partition-all 2)
                      extra-key-value-pairs))]

    (ex-info ex-message ex-map cause)))

(defn shader-transpile-ex-data? [value]
  (case (:ex-type value)
    ::shader-transpile-error true
    false))

(defn transpile-shader-source
  [^String shader-path-or-url ^String shader-source ^long max-page-count]
  {:pre [(string? shader-path-or-url)
         (pos? (count shader-path-or-url))
         (string? shader-source)
         (pos? (count shader-source))]}
  (let [shader-type (filename->shader-type shader-path-or-url)
        pb-shader-type (shader-type->pb-shader-type shader-type)

        ^ShaderUtil$Common$GLSLCompileResult glsl-compile-result
        (try
          (ShaderProgramBuilderEditor/buildGLSLVariantTextureArray shader-path-or-url shader-source pb-shader-type transpile-target-pb-shader-language max-page-count)
          (catch CompileExceptionError cause
            (let [error-line-number (.getLineNumber cause)
                  error-proj-path (or (some-> cause .getResource .getPath (str "/"))
                                      shader-path-or-url)]
              (throw (decorate-transpile-error
                       cause shader-type shader-path-or-url shader-source max-page-count
                       :error-line-number error-line-number
                       :error-proj-path error-proj-path))))
          (catch Exception cause
            (throw (decorate-transpile-error
                     cause shader-type shader-path-or-url shader-source max-page-count))))

        transpiled-shader-source (.source glsl-compile-result)
        array-sampler-names (vec (.arraySamplers glsl-compile-result))
        spirv-reflector (.reflector glsl-compile-result)
        attribute-reflection-infos (mapv make-attribute-reflection-info (.getInputs spirv-reflector))
        resource-binding-namespaces (resource-binding-namespaces spirv-reflector)]
    {:shader-type shader-type
     :transpiled-shader-source transpiled-shader-source
     :resource-binding-namespaces resource-binding-namespaces
     :array-sampler-names array-sampler-names
     :attribute-reflection-infos attribute-reflection-infos}))

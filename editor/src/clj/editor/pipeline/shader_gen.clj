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
            [editor.graphics.types :as graphics.types]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.eduction :as e])
  (:import [com.dynamo.bob CompileExceptionError]
           [com.dynamo.bob.pipeline ShaderProgramBuilder ShaderProgramBuilderEditor ShaderUtil$Common$GLSLCompileResult Shaderc$ShaderResource Shaderc$ShaderStage]
           [com.dynamo.bob.pipeline.shader SPIRVReflector]
           [com.dynamo.graphics.proto Graphics$ShaderDesc$Language Graphics$ShaderDesc$ShaderDataType]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(defn- shader-resource-type->vector-type+data-type [shader-resource-type]
  (let [pb-shader-data-type (ShaderProgramBuilder/resourceTypeToShaderDataType shader-resource-type)]
    (pb-shader-data-type->vector-type+data-type pb-shader-data-type)))

(defn- make-attribute-reflection-info [^Shaderc$ShaderResource attribute]
  {:post [(graphics.types/attribute-reflection-info? %)]}
  (let [shader-resource-type (.-type attribute)
        reflected-location (.-location attribute)
        array-size (.-arraySize shader-resource-type)
        [vector-type data-type] (shader-resource-type->vector-type+data-type shader-resource-type)
        name (.-name attribute)
        name-key (graphics.types/attribute-name-key name)
        inferred-semantic-type (graphics.types/infer-semantic-type name-key)]
    ;; An attribute-reflection-info is an attribute-info with additional fields.
    {:name name
     :name-key name-key
     :semantic-type inferred-semantic-type
     :vector-type vector-type
     :data-type data-type
     :normalize false ; We could infer this using graphics.types/infer-normalize, but the engine doesn't.
     :coordinate-space :coordinate-space-default
     :step-function :vertex-step-function-vertex
     :location reflected-location
     :array-size array-size}))

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

(defonce ^{:private true :tag 'byte} vertex-shader-stage-flag (byte (.getValue Shaderc$ShaderStage/SHADER_STAGE_VERTEX)))

(defn- vertex-shader-resource? [^Shaderc$ShaderResource shader-resource]
  (pos? (bit-and (.-stageFlags shader-resource)
                 vertex-shader-stage-flag)))

(defn transpile-shader-source
  "Compiles a single shader source file, for example, a .vp or a .fp file into a
  augmented-shader-info map with the transpiled shader source and various
  reflection info."
  [^String shader-path ^String shader-source ^long max-page-count]
  {:pre [(string? shader-path)
         (pos? (count shader-path))
         (string? shader-source)
         (pos? (count shader-source))]}
  (let [shader-type (graphics.types/filename-shader-type shader-path)
        pb-shader-type (graphics.types/shader-type-pb-shader-type shader-type)

        ^ShaderUtil$Common$GLSLCompileResult glsl-compile-result
        (try
          (ShaderProgramBuilderEditor/buildGLSLVariantTextureArray shader-path shader-source pb-shader-type transpile-target-pb-shader-language max-page-count)
          (catch CompileExceptionError cause
            (let [error-line-number (.getLineNumber cause)
                  error-proj-path (or (some-> cause .getResource .getPath (str "/"))
                                      shader-path)]
              (throw (decorate-transpile-error
                       cause shader-type shader-path shader-source max-page-count
                       :error-line-number error-line-number
                       :error-proj-path error-proj-path))))
          (catch Exception cause
            (throw (decorate-transpile-error
                     cause shader-type shader-path shader-source max-page-count))))

        transpiled-shader-source (.source glsl-compile-result)
        array-sampler-names (vec (.arraySamplers glsl-compile-result))
        spirv-reflector (.reflector glsl-compile-result)
        resource-binding-namespaces (resource-binding-namespaces spirv-reflector)

        attribute-reflection-infos
        (coll/transfer (.getInputs spirv-reflector) []
          (filter vertex-shader-resource?)
          (map make-attribute-reflection-info))]

    {:shader-type shader-type
     :max-page-count max-page-count
     :transpiled-shader-source transpiled-shader-source
     :resource-binding-namespaces resource-binding-namespaces
     :array-sampler-names array-sampler-names
     :attribute-reflection-infos attribute-reflection-infos}))

(defn- resource-binding-namespaces->regex-str
  ^String [resource-binding-namespaces]
  (str "^(" (string/join "|" resource-binding-namespaces) ")\\."))

(defn combined-shader-info
  "Combines several augmented-shader-infos from, for example, a `.vp` and a
  `.fp` file returned from the transpile-shader-source function into a map with
  all the information required to construct a ShaderLifecycle."
  [augmented-shader-infos]
  (let [max-page-count
        (or (coll/unanimous-value (e/map :max-page-count augmented-shader-infos))
            (throw (ex-info "The max page counts differ among the shaders."
                            {:augmented-shader-infos augmented-shader-infos})))

        shader-type+source-pairs
        (coll/transfer augmented-shader-infos []
          (map (fn [{:keys [shader-type transpiled-shader-source]}]
                 (pair shader-type transpiled-shader-source))))

        array-sampler-name->slice-sampler-names
        (coll/transfer augmented-shader-infos {}
          (mapcat :array-sampler-names)
          (distinct)
          (map (fn [array-sampler-name]
                 (pair array-sampler-name
                       (mapv (fn [page-index]
                               (str array-sampler-name "_" page-index))
                             (range max-page-count))))))

        strip-resource-binding-namespace-regex-str
        (resource-binding-namespaces->regex-str
          (coll/transfer augmented-shader-infos (sorted-set)
            (mapcat :resource-binding-namespaces)))

        attribute-reflection-infos
        (->> augmented-shader-infos
             (transduce
               (comp (mapcat :attribute-reflection-infos)
                     (util/distinct-by :name-key)) ; The first encountered shader attribute takes precedence.
               (fn
                 ([result] result)
                 ([[attribute-reflection-infos taken-locations] attribute-reflection-info]
                  ;; Ensure each attribute maps to a specific shader location.
                  ;; We will associate the attribute names with these locations
                  ;; as we link the shader.
                  (let [attribute-count (graphics.types/vector-type-attribute-count (:vector-type attribute-reflection-info))
                        reflected-location (:location attribute-reflection-info)
                        base-location (coll/first-where
                                        #(not (contains? taken-locations %))
                                        (iterate inc reflected-location))
                        attribute-reflection-info (assoc attribute-reflection-info :location base-location)
                        attribute-reflection-infos (conj attribute-reflection-infos attribute-reflection-info)
                        location-range (range base-location (+ (long base-location) attribute-count))
                        taken-locations (into taken-locations location-range)]
                    [attribute-reflection-infos taken-locations])))
               (pair [] #{}))
             (first)
             (sort-by :location)
             (vec))

        location+attribute-name-pairs
        (mapv (coll/pair-fn :location :name)
              attribute-reflection-infos)]

    {:array-sampler-name->slice-sampler-names array-sampler-name->slice-sampler-names
     :attribute-reflection-infos attribute-reflection-infos
     :location+attribute-name-pairs location+attribute-name-pairs
     :max-page-count max-page-count
     :shader-type+source-pairs shader-type+source-pairs
     :strip-resource-binding-namespace-regex-str strip-resource-binding-namespace-regex-str}))

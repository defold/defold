;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.code.shader
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.resource :as r]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [schema.core :as s])
  (:import [com.dynamo.bob.pipeline ShaderProgramBuilderEditor]
           [com.dynamo.graphics.proto Graphics$ShaderDesc Graphics$ShaderDesc$Language Graphics$ShaderDesc$ShaderType]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def glsl-grammar
  {:name "GLSL"
   :scope-name "source.glsl"
   :indent {:begin #"^.*\{[^}\"\']*$|^.*\([^\)\"\']*$|^\s*\{\}$"
            :end #"^\s*(\s*/[*].*[*]/\s*)*\}|^\s*(\s*/[*].*[*]/\s*)*\)"}
   :line-comment "//"
   :patterns [{:captures {1 {:name "storage.type.glsl"}
                          2 {:name "entity.name.function.glsl"}}
               :match #"^([a-zA-Z_][\w\s]*)\s+([a-zA-Z_]\w*)(?=\s*\()"
               :name "meta.function.glsl"}
              {:name #"comment.line.block.glsl"
               :begin #"/\*"
               :end #"\*/"}
              {:name "comment.line.double-slash.glsl"
               :begin #"//"
               :end #"$"}
              {:match #"<=|>=|!=|[<>=|&+-]{1,2}|[*/!^~?:]|\bdefined\b"
               :name "keyword.operator.glsl"}
              {:match #"\b(break|case|continue|default|discard|do|else|for|if|return|switch|while)\b"
               :name "keyword.control.glsl"}
              {:match #"^\s*#\s*(define|elif|else|endif|error|extension|if|ifdef|ifndef|line|pragma|undef|version|include)\b"
               :name "keyword.preprocessor.glsl"}
              {:match #"\b(bool|bvec[2-4]|dmat[2-4]|dmat[2-4]x[2-4]|double|dvec[2-4]|float|int|isampler2DMS|isampler2DMSArray|isampler2DRect|isamplerBuffer|isamplerCube|isampler[1-3]D|isampler[12]DArray|ivec[2-4]|mat[2-4]|mat[2-4]x[2-4]|sampler2DMS|sampler2DMSArray|sampler2DRect|sampler2DRectShadow|samplerBuffer|samplerCube|sampler[1-3]D|sampler[12]DArray|sampler[12]DArrayShadow|sampler[12]DShadow|struct|uint|usampler2DMS|usampler2DMSArray|usampler2DRect|usamplerBuffer|usamplerCube|usampler[1-3]D|usampler[12]DArray|uvec[2-4]|vec[2-4]|void)\b"
               :name "storage.type.glsl"}
              {:match #"\b(attribute|buffer|centroid|const|flat|highp|in|inout|invariant|lowp|mediump|noperspective|out|patch|precision|sample|shared|smooth|uniform|varying)\b"
               :name "storage.modifier.glsl"}
              {:match #"\b(gl_BackColor|gl_BackLightModelProduct|gl_BackLightProduct|gl_BackMaterial|gl_BackSecondaryColor|gl_ClipDistance|gl_ClipPlane|gl_ClipVertex|gl_Color|gl_DepthRange|gl_DepthRangeParameters|gl_EyePlaneQ|gl_EyePlaneR|gl_EyePlaneS|gl_EyePlaneT|gl_Fog|gl_FogCoord|gl_FogFragCoord|gl_FogParameters|gl_FragColor|gl_FragCoord|gl_FragData|gl_FragDepth|gl_FrontColor|gl_FrontFacing|gl_FrontLightModelProduct|gl_FrontLightProduct|gl_FrontMaterial|gl_FrontSecondaryColor|gl_InstanceID|gl_Layer|gl_LightModel|gl_LightModelParameters|gl_LightModelProducts|gl_LightProducts|gl_LightSource|gl_LightSourceParameters|gl_MaterialParameters|gl_ModelViewMatrix|gl_ModelViewMatrixInverse|gl_ModelViewMatrixInverseTranspose|gl_ModelViewMatrixTranspose|gl_ModelViewProjectionMatrix|gl_ModelViewProjectionMatrixInverse|gl_ModelViewProjectionMatrixInverseTranspose|gl_ModelViewProjectionMatrixTranspose|gl_MultiTexCoord[0-7]|gl_Normal|gl_NormalMatrix|gl_NormalScale|gl_ObjectPlaneQ|gl_ObjectPlaneR|gl_ObjectPlaneS|gl_ObjectPlaneT|gl_Point|gl_PointCoord|gl_PointParameters|gl_PointSize|gl_Position|gl_PrimitiveIDIn|gl_ProjectionMatrix|gl_ProjectionMatrixInverse|gl_ProjectionMatrixInverseTranspose|gl_ProjectionMatrixTranspose|gl_SecondaryColor|gl_TexCoord|gl_TextureEnvColor|gl_TextureMatrix|gl_TextureMatrixInverse|gl_TextureMatrixInverseTranspose|gl_TextureMatrixTranspose|gl_Vertex|gl_VertexID)\b"
               :name "support.variable.glsl"}
              {:match #"\b(gl_MaxClipPlanes|gl_MaxCombinedTextureImageUnits|gl_MaxDrawBuffers|gl_MaxFragmentUniformComponents|gl_MaxLights|gl_MaxTextureCoords|gl_MaxTextureImageUnits|gl_MaxTextureUnits|gl_MaxVaryingFloats|gl_MaxVertexAttribs|gl_MaxVertexTextureImageUnits|gl_MaxVertexUniformComponents)\b"
               :name "support.constant.glsl"}
              {:match #"\b(abs|acos|all|any|asin|atan|ceil|clamp|cos|cross|dFdx|dFdy|degrees|distance|dot|equal|exp|exp2|faceforward|floor|fract|ftransform|fwidth|greaterThan|greaterThanEqual|inversesqrt|length|lessThan|lessThanEqual|log|log2|matrixCompMult|max|min|mix|mod|noise[1-4]|normalize|not|notEqual|outerProduct|pow|radians|reflect|refract|shadow1D|shadow1DLod|shadow1DProj|shadow1DProjLod|shadow2D|shadow2DLod|shadow2DProj|shadow2DProjLod|sign|sin|smoothstep|sqrt|step|tan|texture1D|texture1DLod|texture1DProj|texture1DProjLod|texture2D|texture2DLod|texture2DProj|texture2DProjLod|texture3D|texture3DLod|texture3DProj|texture3DProjLod|textureCube|textureCubeLod|transpose)(?=\s*\()"
               :name "support.function.glsl"}
              {:match #"\b([A-Za-z_]\w*)(?=\s*\()"
               :name "support.function.any.glsl"}
              {:match #"\b(false|true)\b"
               :name "constant.language.glsl"}
              {:match #"\b(0x[0-9A-Fa-f]+)\b"
               :name "constant.numeric.hex.glsl"}
              {:match #"\b[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?(lf|LF|[fFuU])?\b"
               :name "constant.numeric.glsl"}
              {:match #"\b(asm|enum|extern|goto|inline|long|short|sizeof|static|typedef|union|unsigned|volatile)\b"
               :name "invalid.illegal.glsl"}]})

(def ^:private glsl-opts {:code {:grammar glsl-grammar}})

(def shader-defs [{:ext "vp"
                   :language "glsl"
                   :label "Vertex Program"
                   :icon "icons/32/Icons_32-Vertex-shader.png"
                   :icon-class :script
                   :view-types [:code :default]
                   :view-opts glsl-opts}
                  {:ext "fp"
                   :language "glsl"
                   :label "Fragment Program"
                   :icon "icons/32/Icons_33-Fragment-shader.png"
                   :icon-class :script
                   :view-types [:code :default]
                   :view-opts glsl-opts}
                  {:ext "cp"
                   :language "glsl"
                   :label "Compute Program"
                   :icon "icons/64/Icons_29-AT-Unknown.png"
                   :icon-class :script
                   :view-types [:code :default]
                   :view-opts glsl-opts}
                  {:ext "glsl"
                   :label "Shader Include"
                   :icon "icons/64/Icons_29-AT-Unknown.png"
                   :icon-class :script
                   :view-types [:code :default]
                   :view-opts glsl-opts}])

(defn shader-type-from-ext
  ^Graphics$ShaderDesc$ShaderType [^String resource-ext]
  (case resource-ext
    "fp" Graphics$ShaderDesc$ShaderType/SHADER_TYPE_FRAGMENT
    "vp" Graphics$ShaderDesc$ShaderType/SHADER_TYPE_VERTEX
    "cp" Graphics$ShaderDesc$ShaderType/SHADER_TYPE_COMPUTE))

(defn shader-language-to-java
  ^Graphics$ShaderDesc$Language [language]
  (case language
    :language-glsl-sm120 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM120
    :language-glsl-sm140 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM140
    :language-glsl-sm430 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM430
    :language-gles-sm100 Graphics$ShaderDesc$Language/LANGUAGE_GLES_SM100
    :language-gles-sm300 Graphics$ShaderDesc$Language/LANGUAGE_GLES_SM300
    :language-glsl-sm330 Graphics$ShaderDesc$Language/LANGUAGE_GLSL_SM330
    :language-spirv Graphics$ShaderDesc$Language/LANGUAGE_SPIRV))

(defn- error-string->error-value [^String error-string]
  (g/error-fatal (string/trim error-string)))

(defonce ^:private ^"[Lcom.dynamo.graphics.proto.Graphics$ShaderDesc$Language;" java-shader-languages-without-spirv
  (into-array Graphics$ShaderDesc$Language
              (map shader-language-to-java
                   [:language-glsl-sm140 :language-glsl-sm330 :language-gles-sm300 :language-gles-sm100 :language-glsl-sm430])))

(defonce ^:private ^"[Lcom.dynamo.graphics.proto.Graphics$ShaderDesc$Language;" java-shader-languages-with-spirv
  (into-array Graphics$ShaderDesc$Language
              (map shader-language-to-java
                   [:language-glsl-sm140 :language-glsl-sm330 :language-gles-sm300 :language-gles-sm100 :language-glsl-sm430 :language-spirv])))

(defn- build-shader [build-resource _dep-resources user-data]
  (let [{:keys [compile-spirv ^long max-page-count ^String shader-source resource-ext]} user-data
        resource-path (resource/path build-resource)
        java-shader-languages (if compile-spirv
                                java-shader-languages-with-spirv
                                java-shader-languages-without-spirv)
        shader-stage (shader-type-from-ext resource-ext)
        result (ShaderProgramBuilderEditor/makeShaderDescWithVariants resource-path shader-source shader-stage java-shader-languages max-page-count)
        compile-warning-messages (.-buildWarnings result)
        compile-error-values (mapv error-string->error-value compile-warning-messages)]
    (g/precluding-errors compile-error-values
      {:resource build-resource
       :content (protobuf/pb->bytes (.-shaderDesc result))})))

(defn make-shader-build-target [shader-source-info compile-spirv max-page-count]
  {:pre [(map? shader-source-info)
         (string? (:shader-source shader-source-info))
         (boolean? compile-spirv)
         (integer? max-page-count)]}
  (let [shader-resource (:resource shader-source-info)
        workspace (resource/workspace shader-resource)
        shader-resource-type (resource/resource-type shader-resource)
        user-data {:compile-spirv compile-spirv
                   :shader-source (:shader-source shader-source-info)
                   :max-page-count max-page-count
                   :resource-ext (resource/type-ext shader-resource)}
        memory-resource (resource/make-memory-resource workspace shader-resource-type user-data)]
    (bt/with-content-hash
      {:node-id (:node-id shader-source-info)
       :resource (workspace/make-build-resource memory-resource)
       :build-fn build-shader
       :user-data user-data})))

(def ^:private include-pattern #"^\s*\#include\s+(?:<([^\"<>]+)>|\"([^\"<>]+)\")\s*(?:\/\/.*)?$")

(defn- try-parse-include [^String line]
  ;; #include "../shaders/light.glsl" -> "../shaders/light.glsl"
  (->> line
       (re-seq include-pattern)
       (first)
       (drop 1)
       (filter (complement nil?))
       (first)))

(defn- try-parse-included-proj-path [base-resource ^String line]
  (some->> (try-parse-include line)
           (workspace/resolve-resource base-resource)
           (resource/proj-path)))

(g/defnk produce-proj-path+full-lines [resource lines proj-path->full-lines]
  (let [proj-path (resource/proj-path resource)
        full-lines (into []
                         (mapcat (fn [line]
                                   (if-some [included-proj-path (try-parse-included-proj-path resource line)]
                                     (proj-path->full-lines included-proj-path)
                                     [line])))
                         lines)]
    [proj-path full-lines]))

(g/defnk produce-shader-source-info [_node-id resource proj-path+full-lines]
  {:node-id _node-id
   :resource resource
   :shader-source (string/join "\n" (second proj-path+full-lines))})

(g/deftype ^:private ProjPath+Lines [(s/one s/Str "proj-path") (s/one [String] "lines")])

(g/defnode ShaderNode
  (inherits r/CodeEditorResourceNode)

  ;; Overrides modified-lines property in CodeEditorResourceNode.
  (property modified-lines types/Lines
            (dynamic visible (g/constantly false))
            (set (fn [_evaluation-context self _old-value new-value]
                   (let [includes (into #{}
                                        (keep try-parse-include)
                                        new-value)]
                     (g/set-property self :includes includes)))))

  (property includes g/Any
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         resource (resource-node/resource basis self)
                         project (project/get-project basis self)
                         connections [[:proj-path+full-lines :included-proj-paths+full-lines]]]
                     (concat
                       (g/disconnect-sources basis self :included-proj-paths+full-lines)
                       (map (fn [include]
                              (let [included-resource (workspace/resolve-resource resource include)]
                                (:tx-data (project/connect-resource-node evaluation-context project included-resource self connections))))
                            new-value))))))

  (input included-proj-paths+full-lines ProjPath+Lines :array)

  (output proj-path->full-lines g/Any (g/fnk [included-proj-paths+full-lines]
                                                (into {} included-proj-paths+full-lines)))
  (output proj-path+full-lines ProjPath+Lines :cached produce-proj-path+full-lines)
  (output shader-source-info g/Any :cached produce-shader-source-info))

(defn register-resource-types [workspace]
  (for [def shader-defs
        :let [args (assoc def
                     :node-type ShaderNode
                     :built-pb-class Graphics$ShaderDesc
                     :lazy-loaded false)]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))

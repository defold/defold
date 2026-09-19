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

(ns editor.font-shader
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.gl.shader :as shader]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn preview-shader-info
  "Adapts the built-in Slug shader to the editor's GL 2 texture representation.
  The fragment source includes its resolved dependencies from the resource graph."
  [fragment-source picking]
  (let [preview-directives (str "#version 120\n#define SLUG_LEGACY_GL\n#define highp\n#define mediump\n#define lowp\n"
                               (when picking "#define FONT_VECTOR_PICKING\n"))]
    ;; Reflect the vertex attributes through Bob. The fragment shader retains its
    ;; GL 2 adapter and plain uniforms, without passing through the transpiler.
    (update (shader/read-combined-shader-info ["shaders/font_vector.vp"] {} (comp slurp io/resource))
            :shader-type+source-pairs conj
            [:shader-type-fragment (string/replace-first fragment-source
                                                         #"(?m)^[ \t]*#version[^\r\n]*"
                                                         preview-directives)])))

(defn make-preview-shader [node-id shader-source-info picking]
  (let [{:keys [shader-type+source-pairs
               location+attribute-name-pairs
               array-sampler-name->slice-sampler-names
               strip-resource-binding-namespace-regex-str
               attribute-reflection-infos]}
        (preview-shader-info (:shader-source shader-source-info) picking)

        request-data
        (shader/make-shader-request-data
          shader-type+source-pairs
          location+attribute-name-pairs
          array-sampler-name->slice-sampler-names
          strip-resource-binding-namespace-regex-str)]

    (shader/make-shader-lifecycle [node-id :vector picking] request-data attribute-reflection-infos
                                  {"view_proj" :view-proj "id" :id
                                   "effect_bitmap" 0 "curve_texture" 1 "band_texture" 2})))

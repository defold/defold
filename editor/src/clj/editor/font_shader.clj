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

(defn with-vector-samplers [material-shader]
  (update material-shader :uniforms assoc
          "effect_bitmap" 0
          "curve_texture" 1
          "band_texture" 2))

(defn make-selection-shader [node-id shader-source-info]
  (let [fragment-source (string/replace-first (:shader-source shader-source-info)
                                              #"(?m)^([ \t]*#version[^\r\n]*)"
                                              "$1\n#define FONT_VECTOR_PICKING")
        {:keys [shader-type+source-pairs
                location+attribute-name-pairs
                array-sampler-name->slice-sampler-names
                strip-resource-binding-namespace-regex-str
                attribute-reflection-infos]}
        (shader/read-combined-shader-info ["shaders/font_vector.vp" "font_vector.fp"] {}
                                         (fn [path]
                                           (if (= "font_vector.fp" path)
                                             fragment-source
                                             (slurp (io/resource path)))))

        request-data
        (shader/make-shader-request-data
          shader-type+source-pairs
          location+attribute-name-pairs
          array-sampler-name->slice-sampler-names
          strip-resource-binding-namespace-regex-str)]

    (with-vector-samplers
      (shader/make-shader-lifecycle [node-id :vector-selection] request-data attribute-reflection-infos
                                    {"view_proj" :view-proj
                                     "id" :id}))))

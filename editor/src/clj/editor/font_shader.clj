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

(defn preview-sources
  "Adapts the built-in Slug shader to the editor's GL 2 texture representation.
  The fragment source includes its resolved dependencies from the resource graph."
  [fragment-source picking]
  (let [version-directive (str "#version 120\n#define SLUG_LEGACY_GL\n#define highp\n#define mediump\n#define lowp\n"
                               (when picking "#define FONT_VECTOR_PICKING\n"))]
    [[:shader-type-vertex (slurp (io/resource "shaders/font_vector.vp"))]
     [:shader-type-fragment (string/replace-first fragment-source
                                                 #"(?m)^[ \t]*#version[^\r\n]*"
                                                 version-directive)]]))

(defn make-preview-shader [node-id shader-source-info picking]
  (let [[[_ vertex-source] [_ fragment-source]] (preview-sources (:shader-source shader-source-info) picking)]
    (shader/make-shader [node-id :vector picking] vertex-source fragment-source
                       {"view_proj" :view-proj "id" :id
                        "effect_bitmap" 0 "curve_texture" 1 "band_texture" 2})))

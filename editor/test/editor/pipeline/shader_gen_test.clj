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

(ns editor.pipeline.shader-gen-test
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.pipeline.shader-gen :as shader-gen])
  (:import [com.dynamo.bob Bob]
           [java.io File]))

(defn- validate-source! [path source]
  (let [file (File/createTempFile "editor-sm330-" ".glsl")]
    (try
      (spit file source)
      (let [{:keys [exit out err]} (shell/sh (Bob/getHostExeOnce "glslang" nil)
                                           "-S" (if (string/ends-with? path ".vp") "vert" "frag")
                                           (.getAbsolutePath file))]
        (is (zero? exit) (str path "\n" out err)))
      (finally (.delete file)))))

(deftest representative-preview-shaders-test
  (doseq [path ["shaders/basic-color.vp"
                "shaders/basic-color.fp"
                "shaders/basic-texture-paged.vp"
                "shaders/basic-texture-paged-color.fp"
                "shaders/cubemap.fp"
                "test_project/materials/test_attributes.vp"
                "test_project/materials/test_attributes.fp"]]
    (testing path
      (let [source (slurp (io/resource path))
            legacy-info (shader-gen/transpile-shader-source path source 2 "mediump" "highp" :language-glsl-sm120)
            core-info (shader-gen/transpile-shader-source path source 2 "mediump" "highp" :language-glsl-sm330)]
        (is (re-find #"#version 330" (:transpiled-shader-source core-info)))
        (is (not (re-find #"\b(attribute|varying|gl_FragColor|gl_ModelViewProjectionMatrix)\b|\btexture2D\s*\(" (:transpiled-shader-source core-info))))
        (is (= (:attribute-reflection-infos legacy-info) (:attribute-reflection-infos core-info)))
        (is (= (:array-sampler-names legacy-info) (:array-sampler-names core-info)))
        (validate-source! path (:transpiled-shader-source core-info))))))

(deftest sm330-explicit-matrix-attributes-test
  (let [info (shader-gen/transpile-shader-source
               "matrix.vp"
               "#version 330\nlayout(location=2) in vec4 position;\nlayout(location=4) in mat4 transform;\nvoid main() { gl_Position = transform * position; }"
               0 "mediump" "highp" :language-glsl-sm330)
        combined (shader-gen/combined-shader-info [info])]
    (is (= [[2 "position"] [4 "transform"]] (:location+attribute-name-pairs combined)))
    (is (= [:vector-type-vec4 :vector-type-mat4] (mapv :vector-type (:attribute-reflection-infos combined))))
    (validate-source! "matrix.vp" (:transpiled-shader-source info))))

(deftest sm330-binding-metadata-test
  (let [vertex (shader-gen/transpile-shader-source
                 "binding.vp"
                 "#version 140\nin vec4 position;\nuniform uniforms { mat4 view_proj; };\nvoid main() { gl_Position = view_proj * position; }"
                 2 "mediump" "highp" :language-glsl-sm330)
        fragment (shader-gen/transpile-shader-source
                   "binding.fp"
                   "#version 140\nuniform sampler2DArray pages;\nout vec4 color;\nvoid main() { color = texture(pages, vec3(0.5, 0.5, 1.0)); }"
                   2 "mediump" "highp" :language-glsl-sm330)
        combined (shader-gen/combined-shader-info [vertex fragment])
        namespace (first (:resource-binding-namespaces vertex))]
    (is (= [[0 "position"]] (:location+attribute-name-pairs combined)))
    (is (= :vector-type-vec4 (:vector-type (first (:attribute-reflection-infos combined)))))
    (is (= {"pages" ["pages_0" "pages_1"]} (:array-sampler-name->slice-sampler-names combined)))
    (is (some? namespace))
    (is (= "view_proj"
           (string/replace (str namespace ".view_proj")
                                   (re-pattern (:strip-resource-binding-namespace-regex-str combined)) "")))))

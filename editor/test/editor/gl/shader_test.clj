;; Copyright 2020 The Defold Foundation
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

(ns editor.gl.shader-test
  (:require [clojure.test :refer :all]
            [editor.gl.shader :as shader]))

(deftest insert-directives-test
  (testing "Insertion rules"
    (is (= ["#ours"
            "#line 0"]
           (shader/insert-directives []
                                     ["#ours"])))
    (is (= [""
            "#ours"
            "#line 1"]
           (shader/insert-directives [""]
                                     ["#ours"])))
    (is (= ["#theirs"
            "#ours"
            "#line 1"]
           (shader/insert-directives ["#theirs"]
                                     ["#ours"])))
    (is (= ["#theirs"
            "#ours"
            "#line 1"
            "theirs"]
           (shader/insert-directives ["#theirs"
                                      "theirs"]
                                     ["#ours"])))
    (is (= [""
            "#ours"
            "#line 1"
            "theirs"]
           (shader/insert-directives [""
                                      "theirs"]
                                     ["#ours"])))
    (are [prefix]
      (let [their-first-line (str prefix "theirs")]
        (= [their-first-line
            "#ours"
            "#line 1"
            "theirs"]
           (shader/insert-directives [their-first-line
                                      "theirs"]
                                     ["#ours"])))
      "#"
      "\t#"
      "    #"
      "  \t  #"
      "//"
      "\t//"
      "    //"
      "  \t  //"
      "//#"
      "\t//#"
      "    //#"
      "  \t  //#")

    (are [suffix]
      (let [their-first-line (str "#theirs" suffix)]
        (= [their-first-line
            "#ours"
            "#line 1"
            "theirs"]
           (shader/insert-directives [their-first-line
                                      "theirs"]
                                     ["#ours"])))
      "//"
      "\t//"
      "    //"
      "  \t  //"))

  (testing "Real-world use"
    (is (= ["#version 300"
            "#extension GL_EXT_shader_texture_lod : enable"
            "#extension GL_OES_standard_derivatives : enable"
            "#defold-one"
            "#defold-two"
            "#line 3"]
           (shader/insert-directives ["#version 300"
                                      "#extension GL_EXT_shader_texture_lod : enable"
                                      "#extension GL_OES_standard_derivatives : enable"]
                                     ["#defold-one"
                                      "#defold-two"])))
    (is (= ["#version 300"
            "#extension GL_EXT_shader_texture_lod : enable"
            "#extension GL_OES_standard_derivatives : enable"
            ""
            "#defold-one"
            "#defold-two"
            "#line 4"
            "uniform sampler2D myTexture;"
            "varying vec2 texcoord;"
            ""
            "void main() {"
            "    gl_FragColor = texture2DGradEXT(myTexture, mod(texcoord, vec2(0.1, 0.5)), dFdx(texcoord), dFdy(texcoord));"
            "}"]
           (shader/insert-directives ["#version 300"
                                      "#extension GL_EXT_shader_texture_lod : enable"
                                      "#extension GL_OES_standard_derivatives : enable"
                                      ""
                                      "uniform sampler2D myTexture;"
                                      "varying vec2 texcoord;"
                                      ""
                                      "void main() {"
                                      "    gl_FragColor = texture2DGradEXT(myTexture, mod(texcoord, vec2(0.1, 0.5)), dFdx(texcoord), dFdy(texcoord));"
                                      "}"]
                                     ["#defold-one"
                                      "#defold-two"])))
    (is (= ["#version 300"
            "// #extension GL_EXT_shader_texture_lod : enable"
            "// #extension GL_OES_standard_derivatives : enable"
            ""
            "#defold-one"
            "#defold-two"
            "#line 4"
            "uniform sampler2D myTexture;"
            "varying vec2 texcoord;"
            ""
            "void main() {"
            "    // gl_FragColor = texture2DGradEXT(myTexture, mod(texcoord, vec2(0.1, 0.5)), dFdx(texcoord), dFdy(texcoord));"
            "    gl_FragColor = texture2D(myTexture, texcoord.x, texcoord.y);"
            "}"]
           (shader/insert-directives ["#version 300"
                                      "// #extension GL_EXT_shader_texture_lod : enable"
                                      "// #extension GL_OES_standard_derivatives : enable"
                                      ""
                                      "uniform sampler2D myTexture;"
                                      "varying vec2 texcoord;"
                                      ""
                                      "void main() {"
                                      "    // gl_FragColor = texture2DGradEXT(myTexture, mod(texcoord, vec2(0.1, 0.5)), dFdx(texcoord), dFdy(texcoord));"
                                      "    gl_FragColor = texture2D(myTexture, texcoord.x, texcoord.y);"
                                      "}"]
                                     ["#defold-one"
                                      "#defold-two"])))))

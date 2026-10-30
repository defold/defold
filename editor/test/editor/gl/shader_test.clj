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

(ns editor.gl.shader-test
  (:require [clojure.test :refer :all]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [support.gl-support :as gl-support])
  (:import [com.jogamp.opengl GL3]))

(when (gl/supported?)

  ;; Verifies material opt-in against compiled shader reflection, including
  ;; unused samplers and native array samplers, without opening a window.
  (deftest has-configured-sampler-test
    (gl-support/with-offscreen-gl
      (doseq [[description fragment-path opts expected]
              [["active sampler omitted from the material"
                "shaders/font-effects/font-effects.fp"
                {} false]
               ["active sampler configured by the material"
                "shaders/font-effects/font-effects.fp"
                {:uniforms {"font_effects" nil}} true]
               ["configured but unused sampler"
                "shaders/font-effects/unused-font-effects.fp"
                {:uniforms {"font_effects" nil}} false]
               ["configured array sampler"
                "shaders/font-effects/font-effects-paged.fp"
                {:uniforms {"font_effects" nil}} true]
               ["obsolete slice name does not configure an array"
                "shaders/font-effects/font-effects-paged.fp"
                {:uniforms {"font_effects_0" nil}} false]]]
        (testing description
          (let [test-shader (shader/classpath-shader
                              (assoc opts :coordinate-space :coordinate-space-world)
                              "shaders/font-effects/position.vp"
                              fragment-path)]
            (gl/with-gl-bindings gl {} [test-shader]
              (is (pos? (gl/gl-current-program gl)))
              (is (= expected (shader/has-configured-sampler? test-shader gl "font_effects")))))))))

  ;; Verifies binding a texture to a shader without samplers does not throw,
  ;; guarding against aborted GUI rendering and repeated redraw exceptions.
  (deftest set-samplers-by-index-without-samplers-test
    (gl-support/with-offscreen-gl
      (let [test-shader (shader/classpath-shader
                          {:coordinate-space :coordinate-space-world}
                          "shaders/font-effects/position.vp"
                          "shaders/font-effects/solid.fp")]
        (gl/with-gl-bindings gl {} [test-shader]
          (is (pos? (gl/gl-current-program gl)))
          (is (nil? (shader/set-samplers-by-index test-shader gl 0 [3])))
          (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

  ;; Verifies a native array sampler uses one nonzero unit in the GL program,
  ;; rather than merely observing calls to the uniform setter.
  (deftest set-samplers-by-index-with-samplers-test
    (gl-support/with-offscreen-gl
      (let [test-shader (shader/classpath-shader
                          {:coordinate-space :coordinate-space-world}
                          "shaders/font-effects/position.vp"
                          "shaders/font-effects/font-effects-paged.fp")]
        (gl/with-gl-bindings gl {} [test-shader]
          (shader/set-samplers-by-index test-shader gl 0 [5])
          (is (shader/is-using-array-samplers? test-shader))
          (is (= 5 (gl-support/sampler-unit gl "font_effects")))
          (is (= -1 (.glGetUniformLocation gl (gl/gl-current-program gl) "font_effects_0")))
          (is (= GL3/GL_NO_ERROR (.glGetError gl))))))))

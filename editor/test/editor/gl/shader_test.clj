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
  ;; unused samplers and transpiled array slices, without opening a window.
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
               ["configured array sampler slices"
                "shaders/font-effects/font-effects-paged.fp"
                {:uniforms {"font_effects_0" nil "font_effects_1" nil}
                 :max-page-count 2} true]
               ["partially configured array sampler slices"
                "shaders/font-effects/font-effects-paged.fp"
                {:uniforms {"font_effects_0" nil}
                 :max-page-count 2} false]]]
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

  ;; Verifies sampler slice assignments reach the linked GL program,
  ;; rather than merely observing calls to the uniform setter.
  (deftest set-samplers-by-index-with-samplers-test
    (gl-support/with-offscreen-gl
      (let [test-shader (shader/classpath-shader
                          {:coordinate-space :coordinate-space-world
                           :max-page-count 2}
                          "shaders/font-effects/position.vp"
                          "shaders/font-effects/font-effects-paged.fp")]
        (gl/with-gl-bindings gl {} [test-shader]
          (shader/set-samplers-by-index test-shader gl 0 [2 5])
          (is (= 2 (gl-support/sampler-unit gl "font_effects_0")))
          (is (= 5 (gl-support/sampler-unit gl "font_effects_1")))
          (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

  (deftest link-failure-reported-once-test
    (gl-support/with-offscreen-gl
      (let [reported-lines (atom [])
            request-data (-> (shader/make-shader-request-data
                               [[:shader-type-vertex
                                 "#version 330\nin vec4 position;\nout vec4 var_normal;\nvoid main() { var_normal = position; gl_Position = position; }\n"]
                                [:shader-type-fragment
                                 "#version 330\nin vec3 var_normal;\nout vec4 color;\nvoid main() { color = vec4(var_normal, 1.0); }\n"]]
                               []
                               {}
                               nil)
                             (shader/with-proj-path "/materials/mismatched-varyings.material"))
            test-shader (shader/make-shader-lifecycle ::mismatched-varyings request-data [] {})]
        (binding [shader/*report-failure-line!* (fn collect-failure-line! [line] (swap! reported-lines conj line))]
          (gl/with-gl-bindings gl {} [test-shader]
            (is (zero? (gl/gl-current-program gl))))
          (let [first-bind-lines @reported-lines]
            (is (= "ERROR:SHADER: Failed to create shader program for /materials/mismatched-varyings.material"
                   (first first-bind-lines)))
            (is (= "Program link failure." (second first-bind-lines)))
            (gl/with-gl-bindings gl {} [test-shader]
              (is (zero? (gl/gl-current-program gl))))
            (is (= first-bind-lines @reported-lines))))))))

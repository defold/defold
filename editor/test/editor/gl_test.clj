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

(ns editor.gl-test
  (:require [clojure.test :refer :all]
            [editor.gl :as gl]
            [editor.gl.vertex2 :as vtx]
            [editor.geom :as geom]
            [editor.shaders :as shaders]
            [editor.scene-cache :as scene-cache])
  (:import [com.jogamp.opengl GL GL3]
           [java.nio ByteBuffer]))

(deftest core-support-test
  (let [supported {:core-profile true :version "3.3 Vendor" :shading-language-version "3.30 Vendor"}]
    (is (nil? (#'gl/support-error supported)))
    (is (nil? (#'gl/support-error (assoc supported :version "4.1" :shading-language-version "4.10"))))
    (doseq [unsupported [(assoc supported :core-profile false)
                         (assoc supported :version "3.2")
                         (assoc supported :shading-language-version "3.20")
                         (assoc supported :version "OpenGL ES 3.3")
                         {:error "Profile unavailable"}]]
      (is (re-find #"OpenGL 3.3 core" (#'gl/support-error unsupported))))))

(deftest ^:integration core-context-test
  (is (nil? (gl/gl-support-error)) (gl/gl-support-error))
  (let [drawable (gl/offscreen-drawable 16 16)]
    (try
      (dotimes [_ 2]
        (let [ran (atom false)]
          (gl/with-drawable-as-current drawable
            (reset! ran true)
            (is (.isGLCoreProfile gl-context))
            (is (instance? GL3 gl))
            (is (pos? (gl/gl-max-texture-units gl)))
            (let [binding (int-array 1)]
              (.glGetIntegerv gl GL3/GL_VERTEX_ARRAY_BINDING binding 0)
              (is (pos? (aget binding 0))))
            (is (= GL/GL_NO_ERROR (.glGetError gl)))
            ;; The next acquisition must restore the context's default VAO.
            (.glBindVertexArray gl 0))
          (is @ran)))
      (finally (.destroy drawable)))))

(deftest ^:integration core-shaders-and-rendering-test
  (let [drawable (gl/offscreen-drawable 16 16)]
    (try
      (gl/with-drawable-as-current drawable
        ;; Compile/link every shared editor shader with the actual driver.
        (doseq [[name var] (ns-publics 'editor.shaders)
                :let [value @var]
                :when (instance? editor.gl.shader.ShaderLifecycle value)]
          (testing (str name)
            (gl/with-gl-bindings gl {} [value]
              (is (pos? (gl/gl-current-program gl))))))
        ;; Exercise streamed geometry, uniform upload, and exact color readback
        ;; as used by the color picking pass.
        (let [shader shaders/basic-color-straight-alpha-local-space
              vertices (vtx/make-vertex-buffer (shaders/vertex-description shader) :stream 3)
              floats (.asFloatBuffer (vtx/buf vertices))
              pixels (ByteBuffer/allocateDirect 4)]
          (doseq [component [-1 -1 0 1 0 0 1
                             3 -1 0 1 0 0 1
                             -1  3 0 1 0 0 1]]
            (.put floats (float component)))
          (.position (vtx/buf vertices) (* Float/BYTES (.position floats)))
          (vtx/flip! vertices)
          (.glViewport gl 0 0 16 16)
          (.glDisable gl GL/GL_BLEND)
          (.glDisable gl GL/GL_DITHER)
          (gl/gl-clear gl 0 0 0 1)
          (gl/with-gl-bindings gl {:world-view-proj geom/Identity4d}
            [shader (vtx/use-with ::triangle vertices shader)]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 3))
          (.glReadPixels gl 8 8 1 1 GL/GL_RGBA GL/GL_UNSIGNED_BYTE pixels)
          (is (= [255 0 0 255] (mapv #(bit-and 255 (.get pixels (int %))) (range 4)))))
        (is (= GL/GL_NO_ERROR (.glGetError gl)))
        (scene-cache/drop-context! gl))
      (finally (.destroy drawable)))))

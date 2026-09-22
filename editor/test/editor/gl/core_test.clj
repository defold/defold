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

(ns editor.gl.core-test
  (:require [clojure.test :refer :all]
            [editor.gl :as gl])
  (:import [com.jogamp.opengl GL3 GLException]
           [java.nio ByteBuffer FloatBuffer]))

(defn- bound-vao [^GL3 gl]
  (let [value (int-array 1)]
    (.glGetIntegerv gl GL3/GL_VERTEX_ARRAY_BINDING value 0)
    (aget value 0)))

(deftest minimum-profile-test
  (let [supported {:requested-profile "GL3"
                   :actual-profile "GL3"
                   :core-profile true
                   :version "3.3 vendor"
                   :shading-language-version "3.30 vendor"
                   :missing-functions []}]
    (is (nil? (#'gl/info->support-error supported)))
    (is (nil? (#'gl/info->support-error (assoc supported :version "4.1 Metal" :shading-language-version "4.10"))))
    (doseq [unsupported [(assoc supported :core-profile false)
                         (assoc supported :version "3.2")
                         (assoc supported :version nil)
                         (assoc supported :version "unknown")
                         (assoc supported :shading-language-version "3.20")
                         (assoc supported :shading-language-version nil)
                         (assoc supported :missing-functions ["glGenVertexArrays"])
                         {:requested-profile "GL3" :error "No core profile"}]]
      (is (re-find #"OpenGL 3.3 core" (#'gl/info->support-error unsupported))))))

(deftest texture-data-without-core-profile-test
  ;; Profile discovery still needs a graphics-capable machine, even though this
  ;; test only constructs CPU-side data and simulates an unavailable core profile.
  (when (Boolean/getBoolean "defold.test.core-gl")
    (with-redefs [gl/profile (fn [] (throw (GLException. "GL3 unavailable")))]
      (is (= {:error "GL3 unavailable"} (#'gl/query-context-info)))
      (let [make-texture-request-data (requiring-resolve 'editor.gl.texture/make-texture-request-data)
            texture-request-data? (requiring-resolve 'editor.gl.texture/texture-request-data?)]
        (is (texture-request-data? (make-texture-request-data (ByteBuffer/allocateDirect 4) 0 :rgba 1 1 false)))
        (is (= 1 (count @(requiring-resolve 'editor.gl.texture/placeholder-texture-request-datas))))))))

(defn- check-rendering! [^GL3 gl program]
  (let [buffer (gl/gl-gen-buffer gl)
        pixels (ByteBuffer/allocateDirect 4)]
    (try
      (.glViewport gl 0 0 8 8)
      (gl/gl-clear gl 0.0 0.0 0.0 1.0)
      (gl/with-gl-bindings gl {} [program]
        (let [program-id (gl/gl-current-program gl)
              location (.glGetAttribLocation gl (int program-id) "position")]
          (is (pos? program-id))
          (is (<= 0 location))
          (.glBindBuffer gl GL3/GL_ARRAY_BUFFER buffer)
          (.glBufferData gl GL3/GL_ARRAY_BUFFER 24 (FloatBuffer/wrap (float-array [-1 -1 3 -1 -1 3])) GL3/GL_STATIC_DRAW)
          (.glVertexAttribPointer gl location 2 GL3/GL_FLOAT false 0 (long 0))
          (.glEnableVertexAttribArray gl location)
          (try
            (.glDrawArrays gl GL3/GL_TRIANGLES 0 3)
            (finally (.glDisableVertexAttribArray gl location)))))
      (.glReadPixels gl 4 4 1 1 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE pixels)
      (doseq [[index expected] [[0 64] [1 128] [2 191] [3 255]]]
        (is (<= (Math/abs (long (- expected (bit-and 255 (.get pixels (int index)))))) 1)))
      (is (= GL3/GL_NO_ERROR (.glGetError gl)))
      (finally
        (.glBindBuffer gl GL3/GL_ARRAY_BUFFER 0)
        (gl/gl-delete-buffers gl [buffer])))))

(deftest core-rendering-smoke-test
  ;; Opt in on machines with graphics support using -Ddefold.test.core-gl=true.
  (if-not (Boolean/getBoolean "defold.test.core-gl")
    (println "Skipping core GL smoke test; enable with -Ddefold.test.core-gl=true.")
    (let [read-shader (requiring-resolve 'editor.gl.shader/read-shader)
          drop-context! (requiring-resolve 'editor.scene-cache/drop-context!)
          program (read-shader ::smoke ["smoke.vp" "smoke.fp"]
                               {:coordinate-space :coordinate-space-local
                                :uniforms {"tint" (float-array [0.25 0.5 0.75 1.0])}}
                               {"smoke.vp" "#version 140\nin vec2 position;\nvoid main() { gl_Position = vec4(position, 0.0, 1.0); }"
                                "smoke.fp" "#version 140\nuniform uniforms { vec4 tint; };\nout vec4 color;\nvoid main() { color = tint; }"})]
      (is (nil? (gl/gl-support-error)))
      (dotimes [_ 2]
        (let [drawable (gl/offscreen-drawable 8 8)]
          (is (some? drawable))
          (when drawable
            (try
              (let [vao (gl/with-drawable-as-current drawable
                          (is (.isGLCoreProfile gl-context))
                          (let [vao (bound-vao gl)]
                            (is (pos? vao))
                            (is (.glIsVertexArray gl vao))
                            (try
                              (check-rendering! gl program)
                              (finally (drop-context! gl)))
                            (.glBindVertexArray gl 0)
                            vao))]
                (is (some? vao) "The rendering context must become current.")
                (is (true?
                      (gl/with-drawable-as-current drawable
                        (is (= vao (bound-vao gl)))
                        (is (= GL3/GL_NO_ERROR (.glGetError gl)))
                        true))))
              (finally (.destroy drawable)))))))))

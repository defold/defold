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

(ns support.gl-support
  (:require [editor.gl :as gl]
            [editor.scene-cache :as scene-cache])
  (:import [com.jogamp.opengl GL3 GLContext GLOffscreenAutoDrawable]))

(set! *warn-on-reflection* true)

(defmacro with-offscreen-gl
  "Evaluates body with a real GL3 context bound to gl, without opening an Editor
  window. Requires OpenGL 3.3; Linux CI supplies Mesa and Xvfb. Fails instead of
  silently skipping assertions when a drawable or context cannot be created."
  [& body]
  `(let [^GLOffscreenAutoDrawable drawable#
         (or (gl/offscreen-drawable 16 16)
             (throw (IllegalStateException. (str "Offscreen OpenGL is unavailable. " (gl/gl-support-error)))))]
     (try
       (.display drawable#)
       (let [^GLContext context#
             (or (gl/make-current drawable#)
                 (throw (IllegalStateException. "Cannot make offscreen OpenGL context current.")))
             ^GL3 ~'gl (.getGL3 (.getGL context#))]
         (try
           ~@body
           (finally
             (try
               (scene-cache/drop-context! ~'gl)
               (finally
                 (.release context#))))))
       (finally
         (.destroy drawable#)))))

(defn sampler-unit
  "Reads a sampler from the currently bound program using OpenGL reflection."
  [^GL3 gl ^String uniform-name]
  (let [program (gl/gl-current-program gl)
        location (.glGetUniformLocation gl program uniform-name)
        value (int-array 1)]
    (when (neg? location)
      (throw (IllegalArgumentException. (str "Sampler is not active: " uniform-name))))
    (.glGetUniformiv gl program location value 0)
    (aget value 0)))

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

(ns editor.render-util-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as buffers]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.math :as math]
            [editor.pose :as pose]
            [editor.render-util :as render-util]
            [support.gl-support :as gl-support])
  (:import [clojure.lang ExceptionInfo]
           [com.jogamp.opengl GL3]
           [javax.vecmath Matrix4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest make-outlined-textured-quad-scene-accepts-pose-or-matrix-test
  (let [quad-pose (pose/translation-pose 1.0 2.0 3.0)
        transform (doto (Matrix4d.)
                    (.setIdentity)
                    (.setTranslation (Vector3d. 1.0 2.0 3.0)))]
    (is (= quad-pose
           (:pose (render-util/make-outlined-textured-quad-scene #{} quad-pose 10 20 @texture/white-pixel 0))))
    (is (= quad-pose
           (:pose (render-util/make-outlined-textured-quad-scene #{} transform 10 20 @texture/white-pixel 0))))))

(deftest make-outlined-textured-quad-scene-rejects-unsupported-transform-test
  (is (thrown-with-msg?
        ExceptionInfo
        #"Expected Pose or Matrix4d"
        (render-util/make-outlined-textured-quad-scene #{} ::invalid 10 20 @texture/white-pixel 0))))

;; Tone-mapped previews use separate batches so SDR quads keep their own shader.
(deftest tone-mapped-textured-quad-uses-separate-shader-test
  (let [regular-scene (render-util/make-outlined-textured-quad-scene #{} pose/default 10 20 @texture/white-pixel 0)
        tone-mapped-scene (render-util/make-outlined-tone-mapped-textured-quad-scene #{} pose/default 10 20 @texture/white-pixel 0)]
    (is (not (identical? (get-in regular-scene [:renderable :user-data :shader])
                         (get-in tone-mapped-scene [:renderable :user-data :shader]))))
    (is (not= (get-in regular-scene [:renderable :batch-key])
              (get-in tone-mapped-scene [:renderable :batch-key])))))

(when (gl/supported?)

  ;; Verifies paged and 2D preview shaders share the quad buffer correctly and
  ;; sample a nonzero texture unit, guarding against stride and sampler errors.
  (deftest textured-quad-preview-rendering-test
    (gl-support/with-offscreen-gl
      (let [gpu-texture (texture/set-base-unit @texture/white-pixel 3)
            transform (doto (Matrix4d.)
                        (.setIdentity)
                        (.setTranslation (Vector3d. -1.0 -1.0 0.0)))
            rendered-pixel (buffers/new-byte-buffer 4 :byte-order/native)]
        (.glViewport gl 0 0 16 16)
        (.glDisable gl GL3/GL_DITHER)
        (doseq [[description make-scene uses-array-samplers expected-channel]
                [["ordinary paged preview" render-util/make-outlined-textured-quad-scene true 255]
                 ["tone-mapped 2D preview" render-util/make-outlined-tone-mapped-textured-quad-scene false 186]]]
          (testing description
            (let [scene (make-scene #{} pose/default 2 2 gpu-texture 0)
                  renderable (assoc (:renderable scene) :world-transform transform)
                  quad-shader (get-in renderable [:user-data :shader])]
              (is (= uses-array-samplers (shader/is-using-array-samplers? quad-shader)))
              (gl/gl-clear gl 0.0 0.0 0.0 1.0)
              ((:render-fn renderable) gl {:view-proj math/identity-mat4} [renderable] 1)
              (.glReadPixels gl 8 8 1 1 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE rendered-pixel)
              (doseq [^long channel-index [0 1 2]]
                (is (<= (dec ^long expected-channel)
                        (Byte/toUnsignedInt (.get rendered-pixel (int channel-index)))
                        (inc ^long expected-channel))))
              (is (= 255 (Byte/toUnsignedInt (.get rendered-pixel 3))))
              (is (= GL3/GL_NO_ERROR (.glGetError gl))))))))))

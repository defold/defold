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

(ns editor.array-preview-test
  (:require [clojure.test :refer :all]
            [editor.camera :as camera]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gui :as gui]
            [editor.render-util :as render-util]
            [editor.shaders :as shaders]
            [editor.sprite :as sprite]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [support.gl-support :as gl-support])
  (:import [com.jogamp.opengl GL3]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(defn- call-with-rgba-target [test-fn]
  (gl-support/with-offscreen-gl
    ;; The default offscreen framebuffer may not have alpha bits on macOS.
    (let [framebuffer (int-array 1)
          color-buffer (int-array 1)]
      (.glGenFramebuffers gl 1 framebuffer 0)
      (.glGenRenderbuffers gl 1 color-buffer 0)
      (try
        (.glBindFramebuffer gl GL3/GL_FRAMEBUFFER (aget framebuffer 0))
        (.glBindRenderbuffer gl GL3/GL_RENDERBUFFER (aget color-buffer 0))
        (.glRenderbufferStorage gl GL3/GL_RENDERBUFFER GL3/GL_RGBA8 16 16)
        (.glFramebufferRenderbuffer gl GL3/GL_FRAMEBUFFER GL3/GL_COLOR_ATTACHMENT0 GL3/GL_RENDERBUFFER (aget color-buffer 0))
        (.glDrawBuffer gl GL3/GL_COLOR_ATTACHMENT0)
        (.glReadBuffer gl GL3/GL_COLOR_ATTACHMENT0)
        (is (= GL3/GL_FRAMEBUFFER_COMPLETE (.glCheckFramebufferStatus gl GL3/GL_FRAMEBUFFER)))
        (test-fn gl)
        (finally
          (.glBindFramebuffer gl GL3/GL_FRAMEBUFFER 0)
          (.glBindRenderbuffer gl GL3/GL_RENDERBUFFER 0)
          (.glDeleteRenderbuffers gl 1 color-buffer 0)
          (.glDeleteFramebuffers gl 1 framebuffer 0))))))

(defn- solid-layer [color]
  (let [buffer (ByteBuffer/allocateDirect 4)]
    (doseq [value color]
      (.put buffer (unchecked-byte value)))
    (.flip buffer)
    (texture/make-texture-request-data buffer (hash color) :rgba 1 1 false)))

(defn- read-center [^GL3 gl]
  (let [buffer (ByteBuffer/allocateDirect 4)]
    (.glReadPixels gl 8 8 1 1 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE buffer)
    (mapv #(bit-and 255 (.get buffer (int %))) (range 4))))

(defn- clear-target! [^GL3 gl]
  (.glViewport gl 0 0 16 16)
  (.glDisable gl GL3/GL_BLEND)
  (.glDisable gl GL3/GL_DEPTH_TEST)
  (.glDisable gl GL3/GL_CULL_FACE)
  (.glClearColor gl 0.0 0.0 0.0 0.0)
  (.glClear gl GL3/GL_COLOR_BUFFER_BIT))

;; Verifies preview consumers render the requested array layer and discard
;; transparent pixels when picking, including one-layer arrays and 2D atlases.
(deftest native-array-preview-and-picking-test
  (call-with-rgba-target
    (fn check-preview! [^GL3 gl]
      (let [identity-matrix (doto (Matrix4d.) (.setIdentity))
            render-args {:view-proj identity-matrix :world-view-proj identity-matrix}
            red [255 0 0 255]
            green [0 255 0 255]
            transparent [0 0 0 0]
            params (assoc texture/default-image-texture-params :min-filter gl/nearest :mag-filter gl/nearest)]
        (doseq [[description array-texture colors page-index expected]
                [["ordinary atlas" false [red] 0 red]
                 ["one-page paged atlas" true [red] 0 red]
                 ["second page" true [transparent green] 1 green]
                 ["transparent first page" true [transparent green] 0 transparent]
                 ["ninth page" true (conj (vec (repeat 8 transparent)) green) 8 green]]]
          (testing description
            (let [layers (mapv solid-layer colors)
                  gpu-texture (if array-texture
                                (texture/make-gpu-array-texture [::preview description] layers 3 params)
                                (texture/make-gpu-texture [::preview description] layers (vector-of :int 3) params))
                  frame {:width 2.0 :height 2.0 :page-index page-index
                         :tex-coords [[0.0 0.0] [0.0 1.0] [1.0 1.0] [1.0 0.0]]}
                  renderable {:world-transform identity-matrix :picking-id 0x112233
                              :user-data {:gpu-texture gpu-texture :page-index page-index
                                          :x -1.0 :y -1.0 :width 2.0 :height 2.0
                                          :geom-data [[-1 -1 0] [1 -1 0] [1 1 0] [-1 -1 0] [1 1 0] [-1 1 0]]
                                          :uv-data (vec (repeat 6 [0.5 0.5]))
                                          :color [1.0 1.0 1.0 1.0] :blend-mode :blend-mode-alpha
                                          :size-mode :size-mode-auto :size [2.0 2.0] :slice9 [0.0 0.0 0.0 0.0]
                                          :material-attribute-infos []
                                          :shader (if array-texture shaders/basic-texture-paged-world-space shaders/basic-texture-world-space)
                                          :scene-infos [{:gpu-texture gpu-texture :sampler "texture_sampler"
                                                         :animation {:frames [frame]}}]}}]
              (clear-target! gl)
              (#'render-util/render-textured-quad gl render-args [renderable] 1)
              (is (= expected (read-center gl)) "Atlas page quad")
              (clear-target! gl)
              (gui/render-tris gl (assoc render-args :pass pass/transparent) [renderable] 1)
              (is (= expected (read-center gl)) "GUI fallback shader")
              (clear-target! gl)
              (sprite/render-sprites gl (assoc render-args :pass pass/transparent) [renderable] 1)
              (is (= expected (read-center gl)) "Sprite material shader")
              (doseq [[label render!] [["GUI picking" gui/render-tris]
                                       ["Sprite picking" sprite/render-sprites]]]
                (clear-target! gl)
                (render! gl (assoc render-args :pass pass/selection) [renderable] 1)
                (is (= (if (= transparent expected) transparent [17 34 51 255])
                       (read-center gl)) label))
              (when-not (= transparent expected)
                (clear-target! gl)
                (let [overlay-projection (doto (Matrix4d.)
                                           (.setIdentity)
                                           (.setElement 0 3 -41.0)
                                           (.setElement 1 3 -59.0))]
                  (texture-set/render-animation-overlay
                    gl (assoc render-args
                         :world-view-proj overlay-projection
                         :camera (camera/make-camera :orthographic identity {:fov-x 100.0 :fov-y 100.0})
                         :viewport (types/->Region 0.0 100.0 0.0 100.0))
                    [(-> renderable
                         (assoc :updatable {:state {:frame 0}})
                         (assoc-in [:user-data :anim-data] {:width 2.0 :height 2.0 :frames [frame]}))])
                  (is (= expected (read-center gl)) "Animation overlay")))
              (is (= GL3/GL_NO_ERROR (.glGetError gl))))))))))

;; Verifies two array samplers and an ordinary sampler coexist, and sprite unit
;; allocation advances once per array rather than once per atlas page.
(deftest mixed-array-samplers-test
  (call-with-rgba-target
    (fn check-mixed-samplers! [^GL3 gl]
      (let [params (assoc texture/default-image-texture-params :min-filter gl/nearest :mag-filter gl/nearest)
            transparent (solid-layer [0 0 0 0])
            first-array (texture/make-gpu-array-texture ::first [transparent (solid-layer [255 0 0 255])] 3 params)
            second-array (texture/make-gpu-array-texture ::second [transparent (solid-layer [0 255 0 255])] 0 params)
            ordinary (texture/make-gpu-texture ::ordinary [(solid-layer [0 0 255 255])] (vector-of :int 0) params)
            frame {:width 2.0 :height 2.0 :page-index 1
                   :tex-coords [[0.0 0.0] [0.0 1.0] [1.0 1.0] [1.0 0.0]]}
            scene-infos (sprite/produce-scene-infos
                          {:scene-infos (mapv (fn [[sampler gpu-texture]]
                                                {:sampler sampler :gpu-texture gpu-texture
                                                 :anim-data {"default" {:frames [frame]}}})
                                              [["first_array" first-array] ["second_array" second-array] ["ordinary" ordinary]])
                           :default-animation "default" :material-samplers [] :default-tex-params params})
            material-shader (shader/read-shader
                              ::mixed-samplers ["mixed.vp" "mixed.fp"]
                              {:coordinate-space :coordinate-space-world}
                              {"mixed.vp" "#version 140
                                         in vec4 position;
                                         void main() { gl_Position = position; }"
                               "mixed.fp" "#version 140
                                         uniform sampler2DArray first_array;
                                         uniform sampler2DArray second_array;
                                         uniform sampler2D ordinary;
                                         out vec4 color;
                                         void main() {
                                             color = (texture(first_array, vec3(0.5, 0.5, 1.0))
                                                    + texture(second_array, vec3(0.5, 0.5, 1.0))
                                                    + texture(ordinary, vec2(0.5))) / 3.0;
                                         }"})
            renderable {:world-transform (doto (Matrix4d.) (.setIdentity))
                        :user-data {:scene-infos scene-infos :shader material-shader
                                    :material-attribute-infos [] :blend-mode :blend-mode-alpha
                                    :size-mode :size-mode-auto :size [2.0 2.0] :slice9 [0.0 0.0 0.0 0.0]}}]
        (is (= [[3] [4] [5]] (mapv (comp :texture-units :gpu-texture) scene-infos)))
        (clear-target! gl)
        (sprite/render-sprites gl {:pass pass/transparent} [renderable] 1)
        (is (= [85 85 85 255] (read-center gl)))
        (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))

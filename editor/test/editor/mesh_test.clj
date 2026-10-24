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

(ns editor.mesh-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as buffers]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.math :as math]
            [editor.mesh :as mesh]
            [support.gl-support :as gl-support]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL3]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(defn- material-attribute [attribute-name semantic-type step-function vector-type values]
  (let [attribute-info {:name attribute-name
                        :name-key (graphics.types/attribute-name-key attribute-name)
                        :semantic-type semantic-type
                        :vector-type vector-type
                        :data-type :type-float
                        :normalize false
                        :coordinate-space :coordinate-space-local
                        :step-function step-function
                        :values (or values (graphics.types/default-attribute-doubles semantic-type vector-type))}]
    (assoc attribute-info :bytes (first (graphics/attribute->bytes+error-message attribute-info)))))

(defn- make-preview-scene [world-name normal-name uniform-transforms vertex-space {:keys [extra-streams extra-material-attributes]}]
  (let [test-shader
        (shader/read-shader
          ::mesh-preview
          ["mesh-preview.vp" "mesh-preview.fp"]
          {:coordinate-space :coordinate-space-local
           :uniforms (if uniform-transforms
                       {world-name :world normal-name :normal}
                       {})}
          {"mesh-preview.vp"
           (format "#version 140
in vec3 mesh_position;
in vec3 custom_data;
%s
out vec4 var_color;
void main() {
    gl_Position = %s * vec4(mesh_position, 1.0);
    var_color = %s * vec4(custom_data, 0.0) + vec4(0.0, 0.0, 0.0, 1.0);
}"
                   (format (if uniform-transforms
                             "uniform transforms { mat4 %s; mat4 %s; };"
                             "in mat4 %s; in mat4 %s;")
                           world-name normal-name)
                   world-name normal-name)

           "mesh-preview.fp"
           "#version 140
in vec4 var_color;
out vec4 color;
void main() { color = var_color; }"})

        material-attribute-infos
        (if (or uniform-transforms (= "mtx_world" world-name))
          []
          (mapv (fn [attribute-name semantic-type]
                  (material-attribute attribute-name semantic-type :vertex-step-function-instance :vector-type-mat4 nil))
                [world-name normal-name]
                [:semantic-type-world-matrix :semantic-type-normal-matrix]))]

    (mesh/produce-scene
      {:_node-id 1
       :aabb geom/empty-bounding-box
       :streams (into [{:name "mesh_position"
                        :type :value-type-float32
                        :count 3
                        :data [-1.0 -1.0 0.0 1.0 -1.0 0.0 0.0 1.0 0.0]}
                       {:name "custom_data"
                        :type :value-type-float32
                        :count 3
                        :data [0.25 0.125 0.5 0.25 0.125 0.5 0.25 0.125 0.5]}]
                      extra-streams)
       :primitive-type :primitive-triangles
       :position-stream "mesh_position"
       :normal-stream ""
       :shader test-shader
       :gpu-textures {}
       :vertex-space vertex-space
       :material-attribute-infos (into material-attribute-infos extra-material-attributes)})))

;; Verifies inferred and explicitly named matrix semantics preserve arbitrary
;; mesh stream names, while uniform-based materials need no attribute bindings.
(deftest mesh-matrix-attribute-bindings-test
  (doseq [[world-name normal-name uniform-transforms expected-keys]
          [["mtx_world" "mtx_normal" false #{:mtx-world :mtx-normal}]
           ["object_world" "object_normal" false #{:object-world :object-normal}]
           ["world" "normal_matrix" true #{}]]]
    (let [scene (make-preview-scene world-name normal-name uniform-transforms :vertex-space-local {})
          user-data (get-in scene [:renderable :user-data])]
      (is (= ["mesh_position" "custom_data"] (mapv :name (:vertex-attributes user-data))))
      (is (= expected-keys (set (coll/keys (:generated-attribute-bindings user-data))))))))

(when (gl/supported?)

  ;; Verifies the mesh preview actually supplies world and inverse-transpose
  ;; normal matrices to the shader, preserving legacy local/world-space draws.
  (deftest mesh-instanced-shader-preview-test
    (gl-support/with-offscreen-gl
      (doseq [[world-name normal-name uniform-transforms vertex-space expected-rgb options]
              [["mtx_world" "mtx_normal" false :vertex-space-local [128 128 128] {}]
               ["object_world" "object_normal" false :vertex-space-local [128 128 128] {}]
               ["world" "normal_matrix" true :vertex-space-local [128 128 128] {}]
               ["mtx_world" "mtx_normal" false :vertex-space-world [64 32 128] {}]
               ["world" "normal_matrix" true :vertex-space-world [64 32 128] {}]
               ["mtx_world" "mtx_normal" false :vertex-space-local [128 128 128]
                {:extra-streams [{:name "mtx_world"
                                  :type :value-type-float32
                                  :count 4
                                  :data [0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0]}]}]
               ["mtx_world" "mtx_normal" false :vertex-space-local [64 64 64]
                {:extra-material-attributes [(material-attribute "custom_data" :semantic-type-none :vertex-step-function-instance :vector-type-vec3 [0.125 0.0625 0.25])]}]
               ["mtx_world" "mtx_normal" false :vertex-space-local [128 128 128]
                {:extra-material-attributes [(material-attribute "custom_data" :semantic-type-world-matrix :vertex-step-function-vertex :vector-type-vec3 nil)]}]
               ["mtx_world" "mtx_normal" false :vertex-space-local [128 128 128]
                {:extra-material-attributes [(material-attribute "mtx_world" :semantic-type-world-matrix :vertex-step-function-vertex :vector-type-mat4 nil)]}]]]
        (testing (str world-name " " vertex-space " " options)
          (let [scene (make-preview-scene world-name normal-name uniform-transforms vertex-space options)
                renderable (assoc (:renderable scene)
                             :node-id 1
                             :world-transform (Matrix4d. 0.5 0.0 0.0 0.5
                                                         0.0 0.25 0.0 0.5
                                                         0.0 0.0 1.0 0.0
                                                         0.0 0.0 0.0 1.0))
                render-args {:pass pass/opaque
                             :view math/identity-mat4
                             :projection math/identity-mat4
                             :texture math/identity-mat4}
                rendered-pixel (buffers/new-byte-buffer 4 :byte-order/native)]
            (.glViewport gl 0 0 16 16)
            (.glDisable gl GL3/GL_DITHER)
            (gl/gl-clear gl 0.0 0.0 0.0 1.0)
            ((:render-fn renderable) gl render-args [renderable] 1)
            (.glReadPixels gl 12 11 1 1 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE rendered-pixel)
            (is (= (conj expected-rgb 255)
                   (mapv #(Byte/toUnsignedInt (.get rendered-pixel ^int %)) (range 4))))
            (.glReadPixels gl 4 4 1 1 GL3/GL_RGBA GL3/GL_UNSIGNED_BYTE rendered-pixel)
            (is (= [0 0 0 255]
                   (mapv #(Byte/toUnsignedInt (.get rendered-pixel ^int %)) (range 4))))
            (is (= GL3/GL_NO_ERROR (.glGetError gl)))))))))

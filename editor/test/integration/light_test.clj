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

(ns integration.light-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.scene-tools :as scene-tools]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Vector3d]))

(set! *warn-on-reflection* true)

;; Compiles and links the actual lit-model preview on GL2, guarding against unsupported GLSL 1.20 overloads.
(deftest builtin-lit-model-preview-driver-test
  (test-util/with-loaded-project "test/resources/empty_project"
    (let [request-data (g/node-value (test-util/resource-node project "/builtins/materials/model_lit.material")
                                     :shader-request-data)
          drawable (gl/offscreen-drawable 16 16)]
      (is (= 8 (:preview-light-capacity request-data)))
      (is drawable "An offscreen GL2 drawable is required to verify preview shader compilation.")
      (when drawable
        (try
          (is (true?
                (gl/with-drawable-as-current drawable
                  (let [{:keys [program uniform-infos]} (#'shader/make-shader-program gl request-data)]
                    (try
                      (is (pos? program))
                      (doseq [uniform ["light_info"
                                       "lights[0].position"
                                       "lights[0].color"
                                       "lights[0].direction_range"
                                       "lights[0].params"]]
                        (is (contains? uniform-infos uniform) uniform))
                      true
                      (finally
                        (.glDeleteProgram ^GL2 gl (int program)))))))
              "The shader must compile and link with the editor's current GL2 context.")
          (finally
            (.destroy drawable)))))))

;; Verifies sparse area resources receive defaults and uniform scaling updates dimensions and range in save data.
(deftest area-light-defaults-and-manipulation-test
  (test-util/with-scratch-project "test/resources/empty_project"
    (test-util/write-file-resource! workspace "/test.area_light" {:data {}})
    (workspace/resource-sync! workspace)
    (let [node-id (test-util/resource-node project "/test.area_light")]
      (is (= {:data {"color" [1.0 1.0 1.0]
                    "intensity" 1.0
                    "range" 10.0
                    "width" 1.0
                    "height" 1.0}}
             (g/node-value node-id :save-value)))
      (test-util/manip-scale! node-id [2.0 2.0 2.0])
      (is (= 2.0 (g/node-value node-id :width)))
      (is (= 2.0 (g/node-value node-id :height)))
      (is (= 20.0 (g/node-value node-id :range)))
      (is (= ["light" "area_light"] (g/node-value node-id :rt-tags)))
      (is (= 2.0 (get-in (g/node-value node-id :save-value) [:data "width"])))
      (is (= 2.0 (get-in (g/node-value node-id :save-value) [:data "height"])))
      (is (= 20.0 (get-in (g/node-value node-id :save-value) [:data "range"]))))))

;; Guards against range staying fixed during uniform scaling or changing when resizing only the emitter rectangle.
(deftest area-light-scale-preview-and-commit-test
  (test-util/with-scratch-project "test/resources/empty_project"
    (test-util/write-file-resource! workspace "/test.area_light" {:data {}})
    (workspace/resource-sync! workspace)
    (let [node-id (test-util/resource-node project "/test.area_light")]
      (doseq [[scale expected]
              [[[2.0 2.0 2.0] {:width 8.0 :height 12.0 :range 20.0}]
               [[0.5 0.5 0.5] {:width 2.0 :height 3.0 :range 5.0}]
               [[-2.0 -2.0 -2.0] {:width 8.0 :height 12.0 :range 20.0}]
               [[0.0 0.0 0.0] {:width 0.0 :height 0.0 :range 0.0}]
               [[2.0 1.0 1.0] {:width 8.0 :height 6.0 :range 10.0}]
               [[1.0 0.5 1.0] {:width 4.0 :height 3.0 :range 10.0}]
               [[0.5 2.0 1.0] {:width 2.0 :height 12.0 :range 10.0}]]]
        (testing (str "Scale " scale)
          (g/transact (g/set-properties node-id :width 4.0 :height 6.0 :range 10.0))
          (is (= expected
                 (g/with-auto-evaluation-context evaluation-context
                   (:manip/prop-kw->override-value
                     (scene-tools/manip-scale node-id (Vector3d. (double-array scale))
                                              :manip-phase/preview evaluation-context)))))
          (test-util/manip-scale! node-id scale)
          (is (= expected
                 {:width (g/node-value node-id :width)
                  :height (g/node-value node-id :height)
                  :range (g/node-value node-id :range)})))))))

;; Verifies area lights accept zero extents but reject negative, nonfinite, or overflowing values before build.
(deftest area-light-validation-test
  (test-util/with-scratch-project "test/resources/empty_project"
    (test-util/write-file-resource! workspace "/test.area_light" {:data {}})
    (workspace/resource-sync! workspace)
    (let [node-id (test-util/resource-node project "/test.area_light")]
      (doseq [property [:intensity :range :width :height]]
        (doseq [value [0.0 1.0]]
          (g/set-property! node-id property value)
          (is (nil? (g/node-value node-id :own-build-errors))))
        (doseq [value [-1.0 1.0e100 Double/NaN Double/POSITIVE_INFINITY Double/NEGATIVE_INFINITY]]
          (g/set-property! node-id property value)
          (is (g/error-package? (g/node-value node-id :own-build-errors)))
          (is (g/error-value? (g/node-value node-id :build-targets))))
        (g/set-property! node-id property 1.0))
      (doseq [value [1.0e100 Double/NaN Double/POSITIVE_INFINITY]]
        (g/set-property! node-id :color [1.0 value 1.0])
        (is (g/error-package? (g/node-value node-id :own-build-errors)))))))

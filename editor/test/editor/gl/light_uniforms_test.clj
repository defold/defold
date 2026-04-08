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

(ns editor.gl.light-uniforms-test
  (:require [clojure.test :refer :all]
            [editor.gl.light-uniforms :as light-u]
            [editor.gl.pass :as pass])
  (:import [javax.vecmath Matrix4d Vector3d Vector4d]))

(deftest default-preview-lights-shape-test
  (let [lights light-u/default-preview-lights]
    (is (= 1 (count lights)))
    (let [m (first lights)]
      (is (every? #(contains? m %) [:position :color :direction_range :params]))
      (is (instance? Vector4d (:position m))))))

(deftest pack-lights-roundtrip-test
  (is (= light-u/default-preview-lights (light-u/pack-lights light-u/default-preview-lights))))

(deftest lights-std140-float-array-order-test
  (let [lights [{:position (Vector4d. 1.0 2.0 3.0 4.0)
                 :color (Vector4d. 5.0 6.0 7.0 8.0)
                 :direction_range (Vector4d. 9.0 10.0 11.0 12.0)
                 :params (Vector4d. 13.0 14.0 15.0 16.0)}]
        ^floats a (light-u/lights-std140->float-array lights)]
    (is (= 16 (alength a)))
    (dotimes [i 16]
      (is (= (float (inc i)) (aget a i)) (str "index " i)))))

(deftest lights-std140-two-lights-test
  (let [l1 {:position (Vector4d. 1 0 0 0)
            :color (Vector4d. 2 0 0 0)
            :direction_range (Vector4d. 3 0 0 0)
            :params (Vector4d. 4 0 0 0)}
        l2 {:position (Vector4d. 5 0 0 0)
            :color (Vector4d. 6 0 0 0)
            :direction_range (Vector4d. 7 0 0 0)
            :params (Vector4d. 8 0 0 0)}
        ^floats a (light-u/lights-std140->float-array [l1 l2])]
    (is (= 32 (alength a)))
    (is (= 1.0 (aget a 0)))
    (is (= 5.0 (aget a 16)))))

(deftest renderable->std140-point-red-test
  (let [m (light-u/renderable->std140-light
            {:world-translation (Vector3d. 3.0 4.0 5.0)
             :world-transform (doto (Matrix4d.) (.setIdentity))
             :user-data {:editor-preview-light {:light-type :point
                                                :color [1.0 0.0 0.0 1.0]
                                                :intensity 1.0
                                                :range 170.0
                                                :direction [0.0 0.0 -1.0]
                                                :inner-cone-angle 0.0
                                                :outer-cone-angle 45.0}}})]
    (is (< (Math/abs (- 1.0 (.x ^Vector4d (:color m)))) 1e-6))
    (is (< (Math/abs (- 170.0 (.w ^Vector4d (:direction_range m)))) 1e-6))
    (is (< (Math/abs (- 1.0 (.x ^Vector4d (:params m)))) 1e-6))))

(deftest packed-lights-from-scene-prefers-scene-test
  (let [r {:node-id-path [:a :b]
            :world-translation (Vector3d. 0.0 0.0 0.0)
            :world-transform (doto (Matrix4d.) (.setIdentity))
            :user-data {:editor-preview-light {:light-type :point
                                               :color [1.0 0.0 0.0 1.0]
                                               :intensity 1.0
                                               :range 10.0
                                               :direction [0.0 0.0 -1.0]
                                               :inner-cone-angle 0.0
                                               :outer-cone-angle 45.0}}}
        pl (light-u/packed-lights-from-scene {pass/outline [r]})]
    (is (= 1 (count pl)))
    (is (< (Math/abs (- 1.0 (.x ^Vector4d (:color (first pl))))) 1e-6))))

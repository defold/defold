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
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.light :as light])
  (:import [javax.vecmath Matrix4d Vector3d Vector4d]))

(defn- ^Matrix4d identity-m4 []
  (doto (Matrix4d.) (.setIdentity)))

(deftest renderable->std140-point-red-test
  (let [m (shader/renderable->std140-light
            {:world-translation (Vector3d. 3.0 4.0 5.0)
             :world-transform (identity-m4)
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

(deftest packed-lights-from-scene-transparent-pass-test
  (let [r {:node-id-path [:a :b]
           :world-translation (Vector3d. 0.0 0.0 0.0)
           :world-transform (identity-m4)
           :user-data {:editor-preview-light {:light-type :point
                                              :color [1.0 0.0 0.0 1.0]
                                              :intensity 1.0
                                              :range 10.0
                                              :direction [0.0 0.0 -1.0]
                                              :inner-cone-angle 0.0
                                              :outer-cone-angle 45.0}}}
        pl (shader/packed-lights-from-scene {pass/transparent [r]})]
    (is (= 1 (count pl)))
    (is (< (Math/abs (- 1.0 (.x ^Vector4d (:color (first pl))))) 1e-6))))

(deftest packed-lights-from-scene-ignores-non-transparent-passes-test
  (let [r {:node-id-path [:a :b]
           :world-translation (Vector3d. 0.0 0.0 0.0)
           :world-transform (identity-m4)
           :user-data {:editor-preview-light {:light-type :point
                                              :color [1.0 1.0 1.0 1.0]
                                              :intensity 1.0
                                              :range 10.0
                                              :inner-cone-angle 0.0
                                              :outer-cone-angle 45.0}}}]
    (is (= [] (shader/packed-lights-from-scene {pass/outline [r]})))
    (is (= [] (shader/packed-lights-from-scene {pass/selection [r]})))
    (is (= [] (shader/packed-lights-from-scene {})))))

(deftest packed-lights-from-scene-dedupes-by-node-id-path-test
  (let [preview {:light-type :point
                 :color [1.0 1.0 1.0 1.0]
                 :intensity 1.0
                 :range 10.0
                 :inner-cone-angle 0.0
                 :outer-cone-angle 45.0}
        r1 {:node-id-path [:a :b]
            :world-translation (Vector3d. 1.0 0.0 0.0)
            :world-transform (identity-m4)
            :user-data {:editor-preview-light preview}}
        r2 {:node-id-path [:a :b]
            :world-translation (Vector3d. 2.0 0.0 0.0)
            :world-transform (identity-m4)
            :user-data {:editor-preview-light preview}}
        pl (shader/packed-lights-from-scene {pass/transparent [r1 r2]})]
    (is (= 1 (count pl)))
    (is (< (Math/abs (- 1.0 (.x ^Vector4d (:position (first pl))))) 1e-6))))

(deftest packed-lights-from-scene-caps-at-max-test
  (let [preview {:light-type :point
                 :color [1.0 1.0 1.0 1.0]
                 :intensity 1.0
                 :range 10.0
                 :inner-cone-angle 0.0
                 :outer-cone-angle 45.0}
        renderables (mapv (fn [i]
                            {:node-id-path [:r i]
                             :world-translation (Vector3d. (double i) 0.0 0.0)
                             :world-transform (identity-m4)
                             :user-data {:editor-preview-light preview}})
                          (range (+ 4 (long shader/default-max-preview-lights))))
        pl (shader/packed-lights-from-scene {pass/transparent renderables})]
    (is (= (long shader/default-max-preview-lights) (count pl)))))

(deftest point-light-preview-updates-shader-range-test
  (let [[_ user-data] (#'light/point-light-preview-fn
                        geom/null-aabb
                        {:range 10.0
                         :editor-preview-light {:light-type :point
                                                :color [1.0 1.0 1.0 1.0]
                                                :intensity 1.0
                                                :range 10.0
                                                :inner-cone-angle 0.0
                                                :outer-cone-angle 45.0}}
                        {:range 25.0})
        packed-light (shader/renderable->std140-light
                       {:world-translation (Vector3d. 0.0 0.0 0.0)
                        :world-transform (identity-m4)
                        :user-data user-data})]
    (is (= 25.0 (get-in user-data [:editor-preview-light :range])))
    (is (< (Math/abs (- 25.0 (.w ^Vector4d (:direction_range packed-light)))) 1e-6))))

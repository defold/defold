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
            [editor.camera :as camera]
            [editor.geom :as geom]
            [editor.gl.light :as light]
            [editor.gl.pass :as pass]
            [editor.light :as editor-light]
            [editor.scene :as scene]
            [editor.types :as types])
  (:import [javax.vecmath Matrix4d Vector3d Vector4d]))

(defn- ^Matrix4d identity-m4 []
  (doto (Matrix4d.) (.setIdentity)))

(deftest renderable->std140-point-red-test
  (let [m (light/renderable->std140-light
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
        pl (light/packed-lights-from-scene {pass/transparent [r]})]
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
    (is (= [] (light/packed-lights-from-scene {pass/outline [r]})))
    (is (= [] (light/packed-lights-from-scene {pass/selection [r]})))
    (is (= [] (light/packed-lights-from-scene {})))))

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
        pl (light/packed-lights-from-scene {pass/transparent [r1 r2]})]
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
                          (range (+ 4 (long light/default-max-preview-lights))))
        pl (light/packed-lights-from-scene {pass/transparent renderables})]
    (is (= (long light/default-max-preview-lights) (count pl)))))

(deftest point-light-preview-updates-shader-range-test
  (let [[_ user-data] (#'editor-light/point-light-preview-fn
                        geom/null-aabb
                        {:range 10.0
                         :editor-preview-light {:light-type :point
                                                :color [1.0 1.0 1.0 1.0]
                                                :intensity 1.0
                                                :range 10.0
                                                :inner-cone-angle 0.0
                                                :outer-cone-angle 45.0}}
                        {:range 25.0})
        packed-light (light/renderable->std140-light
                       {:world-translation (Vector3d. 0.0 0.0 0.0)
                        :world-transform (identity-m4)
                        :user-data user-data})]
    (is (= 25.0 (get-in user-data [:editor-preview-light :range])))
    (is (< (Math/abs (- 25.0 (.w ^Vector4d (:direction_range packed-light)))) 1e-6))))

(deftest renderable->std140-point-uses-absolute-world-scale-test
  (let [packed-light (light/renderable->std140-light
                       {:world-translation (Vector3d. 0.0 0.0 0.0)
                        :world-transform (identity-m4)
                        :world-scale (Vector3d. -2.0 3.0 4.0)
                        :user-data {:editor-preview-light {:light-type :point
                                                           :color [1.0 1.0 1.0 1.0]
                                                           :intensity 1.0
                                                           :range 10.0
                                                           :inner-cone-angle 0.0
                                                           :outer-cone-angle 45.0}}})]
    (is (< (Math/abs (- 20.0 (.w ^Vector4d (:direction_range packed-light)))) 1e-6))))

(deftest spot-light-preview-uses-preview-cone-angle-test
  (let [[_ user-data] (#'editor-light/spot-light-preview-fn
                        geom/null-aabb
                        {:editor-preview-light {:light-type :spot
                                                :color [1.0 1.0 1.0 1.0]
                                                :intensity 1.0
                                                :range 10.0
                                                :inner-cone-angle 10.0
                                                :outer-cone-angle 30.0}}
                        {:range 20.0})
        ^floats point-scale (:point-scale user-data)
        expected-radius (max (* 20.0 (Math/tan (* 0.5 (Math/toRadians 30.0)))) 0.02)]
    (is (= 20.0 (get-in user-data [:editor-preview-light :range])))
    (is (< (Math/abs (- expected-radius (aget point-scale 0))) 1e-6))
    (is (< (Math/abs (- expected-radius (aget point-scale 1))) 1e-6))))

(deftest hidden-light-renderables-still-provide-preview-lights-test
  (let [camera (camera/make-camera)
        viewport (types/->Region 0 100 0 100)
        scene {:node-id :light-node
               :node-outline-key "Light"
               :renderable {:render-fn :light-render-fn
                            :passes [pass/transparent]
                            :tags #{:light}
                            :user-data {:editor-preview-light {:light-type :point
                                                               :color [1.0 1.0 1.0 1.0]
                                                               :intensity 1.0
                                                               :range 10.0
                                                               :inner-cone-angle 0.0
                                                               :outer-cone-angle 45.0}}}}
        scene-render-data (scene/produce-scene-render-data {:scene scene
                                                            :preview-overrides {}
                                                            :selection []
                                                            :hidden-renderable-tags #{:light}
                                                            :hidden-node-outline-key-paths #{}
                                                            :local-camera camera})
        pass->render-args (scene/produce-pass->render-args {:viewport viewport
                                                            :camera camera
                                                            :scene scene
                                                            :preview-overrides {}
                                                            :hidden-renderable-tags #{:light}
                                                            :hidden-node-outline-key-paths #{}
                                                            :local-camera camera})]
    (is (= [] (get (:renderables scene-render-data) pass/transparent [])))
    (is (= 1 (count (:editor/preview-lights (get pass->render-args pass/transparent)))))))

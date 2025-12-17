;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.pipeline.tex-gen-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.pipeline.tex-gen :as tex-gen])
  (:import [com.dynamo.bob.pipeline TextureGenerator$GenerateResult]
           [com.dynamo.graphics.proto Graphics$TextureImage]
           [javax.imageio ImageIO]))

(deftest gen-bytes
  (let [img     (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        generate-result (tex-gen/make-texture-image img {:name      "test-profile"
                                                         :platforms [{:os      :os-id-generic
                                                                      :formats [{:format            :texture-format-rgba
                                                                                 :compression-level :fast}]
                                                                      :mipmaps false}]})
        tex-img (.textureImage generate-result)
        alt     (.getAlternatives tex-img 0)]
    (is (= 64 (.getWidth alt)))
    (is (= 32 (.getHeight alt)))))

(deftest make-texture-image-test
  (let [img     (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        ^TextureGenerator$GenerateResult
        generate-result (tex-gen/make-texture-image img {:name      "test-profile"
                                                         :platforms [{:os                :os-id-generic
                                                                      :formats           [{:format            :texture-format-rgb
                                                                                           :compression-level :best}
                                                                                          {:format            :texture-format-rgba
                                                                                           :compression-level :best}
                                                                                          {:format            :texture-format-luminance
                                                                                           :compression-level :best}]
                                                                      :mipmaps           false
                                                                      :premultiply-alpha true}]}
                                                    false)
        tex-img (.-textureImage generate-result)]
    (is (= 3 (.getAlternativesCount tex-img)))
    (is (= (* 3 32 64) (.. tex-img (getAlternatives 0) (getDataSize))))
    (is (= (* 4 32 64) (.. tex-img (getAlternatives 1) (getDataSize))))
    (is (= (* 1 32 64) (.. tex-img (getAlternatives 2) (getDataSize))))))

(deftest make-preview-texture-image-test
  (let [img (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        ^TextureGenerator$GenerateResult
        generator-result (tex-gen/make-preview-texture-image
                           img
                           {:name "test-profile"
                            :platforms [{:os :os-id-generic
                                         :formats [{:format :texture-format-rgb
                                                    :compression-level :best}
                                                   {:format :texture-format-rgba
                                                    :compression-level :best}
                                                   {:format :texture-format-luminance
                                                    :compression-level :best}]
                                         :mipmaps false
                                         :premultiply-alpha true}]}
                           true)
        ^Graphics$TextureImage tex-img (.-textureImage generator-result)
        total-data-size (reduce + (map #(count %) (.-imageDatas generator-result)))]
    (is (= 1 (.getAlternativesCount tex-img)))
    (is (= (* 3 32 64) total-data-size))))

(deftest match-texture-profile-test
  (let [texture-profiles {:path-settings [{:path "/**/photos/*.png" :profile "Photo"}
                                          {:path "**" :profile "Default"}]
                          :profiles      [{:name "Default"}
                                          {:name "Photo"}]}]
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/bar.atlas"))))
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/not-a-png.atlas"))))
    (is (= "Photo"   (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/a-png.png"))))))

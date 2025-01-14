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
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [editor.pipeline.tex-gen :as tex-gen])
  (:import [javax.imageio ImageIO]
           [com.dynamo.graphics.proto Graphics$TextureImage]))

(deftest gen-bytes
  (let [img     (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        bytes   (protobuf/pb->bytes (tex-gen/make-texture-image img {:name      "test-profile"
                                                                     :platforms [{:os      :os-id-generic
                                                                                  :formats [{:format            :texture-format-rgba
                                                                                             :compression-level :fast}]
                                                                                  :mipmaps false}]}))
        tex-img (Graphics$TextureImage/parseFrom bytes)
        alt     (.getAlternatives tex-img 0)]
    (is (= 64 (.getWidth alt)))
    (is (= 32 (.getHeight alt)))))

(deftest make-texture-image-test
  (let [img     (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        tex-img (tex-gen/make-texture-image img {:name      "test-profile"
                                                 :platforms [{:os                :os-id-generic
                                                              :formats           [{:format            :texture-format-rgb
                                                                                   :compression-level :best}
                                                                                  {:format            :texture-format-rgba
                                                                                   :compression-level :best}
                                                                                  {:format            :texture-format-luminance
                                                                                   :compression-level :best}]
                                                              :mipmaps           false
                                                              :premultiply-alpha true}]}
                                            false)]
    (is (= 3 (.getAlternativesCount tex-img)))
    (is (= (* 3 32 64) (.. tex-img (getAlternatives 0) (getData) (size))))
    (is (= (* 4 32 64) (.. tex-img (getAlternatives 1) (getData) (size))))
    (is (= (* 1 32 64) (.. tex-img (getAlternatives 2) (getData) (size))))))

(deftest make-preview-texture-image-test
  (let [img     (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        tex-img (tex-gen/make-preview-texture-image img {:name      "test-profile"
                                                         :platforms [{:os                :os-id-generic
                                                                      :formats           [{:format            :texture-format-rgb
                                                                                           :compression-level :best}
                                                                                          {:format            :texture-format-rgba
                                                                                           :compression-level :best}
                                                                                          {:format            :texture-format-luminance
                                                                                           :compression-level :best}]
                                                                      :mipmaps           false
                                                                      :premultiply-alpha true}]})]
    (is (= 1 (.getAlternativesCount tex-img)))
    (is (= (* 3 32 64) (.. tex-img (getAlternatives 0) (getData) (size))))))

(deftest match-texture-profile-test
  (let [texture-profiles {:path-settings [{:path "/**/photos/*.png" :profile "Photo"}
                                          {:path "**" :profile "Default"}]
                          :profiles      [{:name "Default"}
                                          {:name "Photo"}]}]
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/bar.atlas"))))
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/not-a-png.atlas"))))
    (is (= "Photo"   (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/a-png.png"))))))

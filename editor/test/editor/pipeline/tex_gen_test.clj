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

(ns editor.pipeline.tex-gen-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.bob.pipeline TextureGenerator$GenerateResult]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$CompressionType Graphics$TextureImage$TextureFormat Graphics$TextureProfiles]
           [java.nio ByteBuffer ByteOrder]
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

;; HDR sources default to RGBA32F without quantizing their pixels to bytes.
(deftest make-hdr-texture-image-test
  (let [image-bytes (with-open [input-stream (io/input-stream (io/resource "test_project/graphics/hdr_2x2.hdr"))]
                      (.readAllBytes input-stream))
        generate-result (tex-gen/make-texture-image image-bytes nil false false)
        ^Graphics$TextureImage texture-image (.-textureImage generate-result)
        alternative (.getAlternatives texture-image 0)]
    (is (= 1 (.getAlternativesCount texture-image)))
    (is (= Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA32F (.getFormat alternative)))
    (is (= 2 (.getWidth alternative)))
    (is (= 2 (.getHeight alternative)))))

;; Preview selection skips SDR alternatives and preserves float formats.
(deftest make-hdr-preview-texture-image-test
  (let [image-bytes (with-open [input-stream (io/input-stream (io/resource "test_project/graphics/hdr_2x2.hdr"))]
                      (.readAllBytes input-stream))]
    (doseq [[format expected-format] [[:texture-format-rgba16f Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA16F]
                                      [:texture-format-rgba32f Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA32F]
                                      [:texture-format-rgba-astc-4x4 Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA32F]]]
      (let [profile {:name "HDR"
                     :platforms [{:os :os-id-generic
                                  :formats [{:format :texture-format-rgba}
                                            {:format format}]
                                  :mipmaps false}]}
            ^TextureGenerator$GenerateResult result (tex-gen/make-preview-texture-image image-bytes profile true)
            alternative (.. result textureImage (getAlternatives 0))]
        (is (= expected-format (.getFormat alternative)) (name format))
        (is (= 1 (count (.-imageDatas result))))))))

;; Float previews preserve values above one and follow the requested Y flip.
(deftest hdr-preview-pixels-test
  (let [image-bytes (with-open [stream (io/input-stream (io/resource "test_project/graphics/hdr_2x2.hdr"))]
                      (.readAllBytes stream))]
    ;; The fixture starts with a red pixel. Double its RGBE exponent to make
    ;; its red component 2.0, which an SDR conversion would clamp.
    (aset-byte image-bytes 48 (unchecked-byte 130))
    (doseq [flip-y [false true]]
      (let [^TextureGenerator$GenerateResult result (tex-gen/make-preview-texture-image image-bytes nil flip-y)
            pixels (doto (ByteBuffer/wrap (first (.-imageDatas result)))
                     (.order ByteOrder/LITTLE_ENDIAN))]
        (is (= (if flip-y 0.0 2.0) (.getFloat pixels 0)))
        (is (= (if flip-y 2.0 0.0) (.getFloat pixels 32)))))))

(deftest make-preview-texture-image-test
  (let [img (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        ^TextureGenerator$GenerateResult
        generator-result (tex-gen/make-preview-texture-image img nil true)
        ^Graphics$TextureImage tex-img (.-textureImage generator-result)
        total-data-size (reduce + (map #(count %) (.-imageDatas generator-result)))]
    (is (= 1 (.getAlternativesCount tex-img)))
    (is (= (* 4 32 64) total-data-size))))

(deftest match-texture-profile-test
  (let [texture-profiles {:path-settings [{:path "/**/photos/*.png" :profile "Photo"}
                                          {:path "**" :profile "Default"}]
                          :profiles      [{:name "Default"}
                                          {:name "Photo"}]}]
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/bar.atlas"))))
    (is (= "Default" (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/not-a-png.atlas"))))
    (is (= "Photo"   (:name (tex-gen/match-texture-profile texture-profiles "/foo/photos/a-png.png"))))))

;; New projects select the HDR profile only for HDR files.
(deftest default-texture-profiles-test
  (let [texture-profiles (with-open [reader (io/reader (io/resource "templates/template.texture_profiles"))]
                           (protobuf/read-map-with-defaults Graphics$TextureProfiles reader))
        hdr-profile (tex-gen/match-texture-profile texture-profiles "/graphics/environment.hdr")
        default-profile (tex-gen/match-texture-profile texture-profiles "/graphics/albedo.png")]
    (is (= "HDR" (:name hdr-profile)))
    (is (= :texture-format-rgba32f
           (get-in hdr-profile [:platforms 0 :formats 0 :format])))
    (is (= "Uncompressed"
           (get-in hdr-profile [:platforms 0 :formats 0 :compressor])))
    (is (= "Default" (:name default-profile)))
    (is (= :texture-format-rgba
           (get-in default-profile [:platforms 0 :formats 0 :format])))))

;; KTX2 format preservation works without explicit format alternatives.
;; Compressed builds and uncompressed previews honor size and mipmap settings.
(deftest keep-ktx2-format-without-format-alternatives
  (let [bytes (with-open [stream (io/input-stream "../com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/ktx2/etc1s.ktx2")]
                (.readAllBytes stream))
        profile {:name "Keep"
                 :platforms [{:os :os-id-generic
                              :keep-ktx2-format true
                              :mipmaps false
                              :max-texture-size 4
                              :premultiply-alpha false}]}
        built (tex-gen/make-ktx2-texture-image bytes profile true false)
        preview (tex-gen/make-ktx2-texture-image bytes profile false true)]
    (is (= Graphics$TextureImage$CompressionType/COMPRESSION_TYPE_BASIS_ETC1S
           (.. built textureImage (getAlternatives 0) getCompressionType)))
    (is (= Graphics$TextureImage$CompressionType/COMPRESSION_TYPE_DEFAULT
           (.. preview textureImage (getAlternatives 0) getCompressionType)))
    (doseq [^TextureGenerator$GenerateResult result [built preview]]
      (is (= 4 (.. result textureImage (getAlternatives 0) getWidth)))
      (is (= 2 (.. result textureImage (getAlternatives 0) getHeight)))
      (is (= 1 (count (.-imageDatas result)))))))

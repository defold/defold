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

(ns editor.image-test
  (:require [clojure.java.io :as io :refer [as-url file]]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.gl.texture :as texture]
            [editor.image :as image]
            [editor.image-util :as image-util]
            [editor.resource :as resource]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [schema.test]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline TextureGenerator$GenerateResult]
           [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.jogamp.opengl GL]
           [editor.gl.texture RawTextureData TextureRequestData]
           [java.awt.image BufferedImage]))

(use-fixtures :once schema.test/validate-schemas)

(deftest image-loading
  (let [img (image-util/make-image (as-url (file "foo")) (BufferedImage. 128 192 BufferedImage/TYPE_4BYTE_ABGR))]
    (is (= 128 (.width img)))
    (is (= 192 (.height img)))))

;; HDR resources retain float pixels for preview and build, and expose metadata.
(deftest hdr-resource-test
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/hdr_2x2.hdr")
          hdr-resource (g/node-value node-id :resource)
          content-generator (g/node-value node-id :content-generator)
          gpu-texture (g/node-value node-id :gpu-texture)
          ^TextureRequestData texture-request-data (first (:texture-request-datas gpu-texture))
          ^RawTextureData texture-data (.-texture-data texture-request-data)
          build-target (first (g/node-value node-id :build-targets))
          build-result ((:build-fn build-target) (:resource build-target) {} (:user-data build-target))
          ^TextureGenerator$GenerateResult texture-generator-result (get-in build-result [:user-data :texture-generator-result])
          texture-image (.-textureImage texture-generator-result)]
      (is (g/node-instance? image/ImageNode node-id))
      (is (image/image-resource? hdr-resource))
      (is (not (image/sdr-resource? hdr-resource)))
      (is (= {:width 2 :height 2} (g/node-value node-id :size)))
      (is (bytes? (texture-util/call-generator content-generator)))
      (is (texture/texture-lifecycle? gpu-texture))
      (is (= GL/GL_RGBA32F (.-internal-format texture-data)))
      (is (= GL/GL_RGBA (.-pixel-format texture-data)))
      (is (= GL/GL_FLOAT (.-pixel-type texture-data)))
      (is (= "texturec" (resource/ext (:resource build-target))))
      (is (= Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA32F
             (.. texture-image (getAlternatives 0) getFormat))))))

;; Malformed HDR pixels report resource errors instead of throwing during
;; preview, scene evaluation, or building.
(deftest malformed-hdr-resource-test
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [hdr-resource (test-util/resource workspace "/malformed.hdr")]
      (with-open [stream (io/output-stream hdr-resource)]
        (.write stream (.getBytes "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n" "UTF-8"))
        ;; The scanline declares eight pixels, but its first run contains nine.
        (.write stream (byte-array [2 2 0 8 137 255])))
      (workspace/resource-sync! workspace)
      (let [node-id (test-util/resource-node project "/malformed.hdr")]
        (is (= {:width 8 :height 1} (g/node-value node-id :size)))
        (let [preview-error (texture-util/generate-gpu-texture (g/node-value node-id :gpu-texture-generator))]
          (is (g/error-value? preview-error))
          (is (= :invalid-content (get-in preview-error [:user-data :type])))
          (is (= (g/node-value node-id :resource) (get-in preview-error [:user-data :resource])))
          (is (= node-id (:_node-id preview-error)))
          (is (g/error-value? (g/node-value node-id :gpu-texture)))
          (is (g/error-value? (g/node-value node-id :scene))))
        (let [build-error (test-util/build-error! node-id)
              cause (coll/first-where :message (eduction (coll/tree-xf :causes :causes) [build-error]))]
          (is (g/error-fatal? build-error))
          (is (= :invalid-content (get-in cause [:user-data :type])))
          (is (= hdr-resource (get-in cause [:user-data :resource])))
          (is (= node-id (:_node-id cause))))))))

;; SDR-only custom profiles still preview HDR images, but building reports an
;; actionable resource error instead of throwing a texture-generation exception.
(deftest hdr-resource-with-sdr-profile-test
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [hdr-resource (test-util/resource workspace "/image.hdr")]
      (with-open [input-stream (io/input-stream (io/resource "test_project/graphics/hdr_2x2.hdr"))
                  output-stream (io/output-stream hdr-resource)]
        (io/copy input-stream output-stream))
      (test-util/write-file-resource! workspace "/sdr.texture_profiles"
        {:path-settings [{:path "**" :profile "SDR"}]
         :profiles [{:name "SDR"
                     :platforms [{:os :os-id-generic
                                  :formats [{:format :texture-format-rgba}]
                                  :mipmaps false}]}]})
      (workspace/resource-sync! workspace)
      (test-util/set-setting! (test-util/resource-node project "/game.project")
                              ["graphics" "texture_profiles"]
                              (workspace/find-resource workspace "/sdr.texture_profiles"))
      (let [node-id (test-util/resource-node project "/image.hdr")
            gpu-texture (g/node-value node-id :gpu-texture)
            ^TextureRequestData texture-request-data (first (:texture-request-datas gpu-texture))
            ^RawTextureData texture-data (.-texture-data texture-request-data)
            build-error (test-util/build-error! node-id)
            cause (coll/first-where :message (eduction (coll/tree-xf :causes :causes) [build-error]))]
        (is (= "SDR" (:name (g/node-value node-id :texture-profile))))
        (is (texture/texture-lifecycle? gpu-texture))
        (is (= GL/GL_RGBA32F (.-internal-format texture-data)))
        (is (g/error-fatal? build-error))
        (is (= :invalid-content (get-in cause [:user-data :type])))
        (is (= hdr-resource (get-in cause [:user-data :resource])))
        (is (= node-id (:_node-id cause)))
        (is (re-find #"HDR textures require a texture profile with RGBA16F, RGBA32F, or ASTC texture formats\."
                     (test-util/localization (g/error-message build-error))))))))

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
  (:require [clojure.java.io :refer [as-url file]]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.gl.texture :as texture]
            [editor.image :as image :refer :all]
            [editor.image-util :refer :all]
            [editor.geom :refer :all]
            [editor.resource :as resource]
            [editor.texture-util :as texture-util]
            [integration.test-util :as test-util]
            [schema.test])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.jogamp.opengl GL]
           [editor.gl.texture RawTextureData TextureRequestData]
           [java.awt.image BufferedImage]))

(use-fixtures :once schema.test/validate-schemas)

(deftest image-loading
  (let [img (make-image (as-url (file "foo")) (BufferedImage. 128 192 BufferedImage/TYPE_4BYTE_ABGR))]
    (is (= 128 (.width img)))
    (is (= 192 (.height img)))))

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
          texture-generator-result (get-in build-result [:user-data :texture-generator-result])
          texture-image (.-textureImage texture-generator-result)]
      (is (g/node-instance? image/ImageNode node-id))
      (is (image/image-resource? hdr-resource))
      (is (not (image/buffered-image-resource? hdr-resource)))
      (is (= {:width 2 :height 2} (g/node-value node-id :size)))
      (is (bytes? (texture-util/call-generator content-generator)))
      (is (texture/texture-lifecycle? gpu-texture))
      (is (= GL/GL_RGBA32F (.-internal-format texture-data)))
      (is (= GL/GL_RGBA (.-pixel-format texture-data)))
      (is (= GL/GL_FLOAT (.-pixel-type texture-data)))
      (is (= "texturec" (resource/ext (:resource build-target))))
      (is (= Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA32F
             (.. texture-image (getAlternatives 0) getFormat))))))

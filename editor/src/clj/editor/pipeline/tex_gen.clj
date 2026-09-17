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

(ns editor.pipeline.tex-gen
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [internal.util :as util]
            [util.coll :as coll]
            [util.digest :as digest])
  (:import [com.dynamo.bob.pipeline Texc$FlipAxis]
           [com.dynamo.bob.pipeline TextureGenerator TextureGenerator$GenerateResult]
           [com.dynamo.bob.util TextureUtil]
           [com.dynamo.graphics.proto Graphics$TextureImage$Type Graphics$TextureProfile Graphics$TextureProfiles]
           [java.awt.image BufferedImage]
           [java.util EnumSet]))

(set! *warn-on-reflection* true)

(def ^:private texture-format->editor-format
  {:texture-format-luminance         :texture-format-luminance
   :texture-format-rgb               :texture-format-rgb
   :texture-format-rgba              :texture-format-rgba
   :texture-format-rgb-pvrtc-2bppv1  :texture-format-rgb
   :texture-format-rgb-pvrtc-4bppv1  :texture-format-rgb
   :texture-format-rgba-pvrtc-2bppv1 :texture-format-rgba
   :texture-format-rgba-pvrtc-4bppv1 :texture-format-rgba
   :texture-format-rgb-etc1          :texture-format-rgb
   :texture-format-rgb-16bpp         :texture-format-rgb
   :texture-format-rgba-16bpp        :texture-format-rgba

   ;; ASTC formats
   :texture-format-rgba-astc-4x4     :texture-format-rgba
   :texture-format-rgba-astc-5x4     :texture-format-rgba
   :texture-format-rgba-astc-5x5     :texture-format-rgba
   :texture-format-rgba-astc-6x5     :texture-format-rgba
   :texture-format-rgba-astc-6x6     :texture-format-rgba
   :texture-format-rgba-astc-8x5     :texture-format-rgba
   :texture-format-rgba-astc-8x6     :texture-format-rgba
   :texture-format-rgba-astc-8x8     :texture-format-rgba
   :texture-format-rgba-astc-10x5    :texture-format-rgba
   :texture-format-rgba-astc-10x6    :texture-format-rgba
   :texture-format-rgba-astc-10x8    :texture-format-rgba
   :texture-format-rgba-astc-10x10   :texture-format-rgba
   :texture-format-rgba-astc-12x10   :texture-format-rgba
   :texture-format-rgba-astc-12x12   :texture-format-rgba

   ;; This is incorrect, but it seems like jogl does not define or
   ;; support a pixelformat of L8A8. So we use RGBA instead to at
   ;; least previewing with alpha.
   :texture-format-luminance-alpha   :texture-format-rgba})

(defn match-texture-profile-pb
  ^Graphics$TextureProfile [texture-profiles ^String path]
  (let [texture-profiles-data (some->> texture-profiles (protobuf/map->pb Graphics$TextureProfiles))
        path (if (.startsWith path "/") (subs path 1) path)]
    (TextureUtil/getTextureProfileByPath texture-profiles-data path)))

(defn match-texture-profile
  [texture-profiles ^String path]
  (when-some [texture-profile (match-texture-profile-pb texture-profiles path)]
    (protobuf/pb->map-with-defaults texture-profile)))

(defn make-texture-image
  (^TextureGenerator$GenerateResult [^BufferedImage image texture-profile]
   (make-texture-image image texture-profile false))
  (^TextureGenerator$GenerateResult [^BufferedImage image texture-profile compress?]
   (make-texture-image image texture-profile compress? true))
  (^TextureGenerator$GenerateResult [^BufferedImage image texture-profile compress? flip-y?]
   (let [^Graphics$TextureProfile texture-profile-data (some->> texture-profile (protobuf/map->pb Graphics$TextureProfile))
         texture-generator-result (TextureGenerator/generate image texture-profile-data ^boolean compress? (if ^boolean flip-y? (EnumSet/of Texc$FlipAxis/FLIP_AXIS_Y) (EnumSet/noneOf Texc$FlipAxis)))]
     texture-generator-result)))

(defn- make-preview-profile
  "Given a texture-profile, return a simplified texture-profile that can be used
  for previewing purposes in editor. Will only produce data for one texture
  format."
  [texture-profile]
  (let [platforms (:platforms texture-profile)
        platform-profile (or (coll/first-where #(= :os-id-generic (:os %)) platforms)
                             (first platforms))
        texture-format (first (:formats platform-profile))]
    (when (and platform-profile texture-format)
      {:name      "editor"
       :platforms [{:os                :os-id-generic
                    :formats           [(update texture-format :format texture-format->editor-format)]
                    :mipmaps           (:mipmaps platform-profile)
                    :max-texture-size  (:max-texture-size platform-profile)
                    :premultiply-alpha (:premultiply-alpha platform-profile)}]})))

;; SDK api (DEPRECATE 2-arity version with the next release of extension-texturepacker).
(defn make-preview-texture-image
  (^TextureGenerator$GenerateResult [^BufferedImage image texture-profile]
   (let [preview-profile (make-preview-profile texture-profile)]
     (make-texture-image image preview-profile false)))
  (^TextureGenerator$GenerateResult [^BufferedImage image texture-profile flip-y]
   (if flip-y
     ;; TODO: We might be able to pass a flip-y bool arg to TexcLib.CreatePreviewImage and make this work for all
     (TextureGenerator/generateAtlasPreview image) ;; Fast path
     (let [preview-profile (make-preview-profile texture-profile)]
       (make-texture-image image preview-profile false flip-y)))))

(defn make-cubemap-texture-images
  ^TextureGenerator$GenerateResult [images texture-profile compress?]
  (let [^Graphics$TextureProfile texture-profile-data (some->> texture-profile (protobuf/map->pb Graphics$TextureProfile))
        flip-axis (EnumSet/noneOf Texc$FlipAxis)]
    (util/map-vals #(TextureGenerator/generate ^BufferedImage % texture-profile-data ^boolean compress? flip-axis)
                   images)))

(defn assemble-texture-images
  ^TextureGenerator$GenerateResult [texture-generator-results max-page-count]
  (let [texture-type (if (pos? max-page-count)
                       Graphics$TextureImage$Type/TYPE_2D_ARRAY
                       Graphics$TextureImage$Type/TYPE_2D)]
    (TextureUtil/createCombinedTextureImage (into-array texture-generator-results) texture-type)))

(defn assemble-cubemap-texture-images
  ^TextureGenerator$GenerateResult [side->texture-image]
  (let [texture-images (into-array ((juxt :px :nx :py :ny :pz :nz) side->texture-image))
        type Graphics$TextureImage$Type/TYPE_CUBEMAP]
    (TextureUtil/createCombinedTextureImage texture-images type)))

(defn write-texturec-content-fn [resource user-data]
  (let [digest-output-stream
        (-> resource
            (io/output-stream)
            (digest/make-digest-output-stream "SHA-1"))]
    ;; writeGenerateResultToOutputStream flushes the stream
    (TextureUtil/writeGenerateResultToOutputStream (:texture-generator-result user-data) digest-output-stream)
    (digest/completed-stream->hex digest-output-stream)))

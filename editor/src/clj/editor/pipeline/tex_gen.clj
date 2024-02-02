;; Copyright 2020-2024 The Defold Foundation
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
  (:require [editor.protobuf :as protobuf]
            [internal.util :as util])
  (:import [com.dynamo.bob TexcLibrary$FlipAxis]
           [com.dynamo.bob.pipeline TextureGenerator]
           [com.dynamo.bob.util TextureUtil]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Type Graphics$TextureProfile Graphics$TextureProfiles]
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
    (protobuf/pb->map texture-profile)))

(defn make-texture-image
  (^Graphics$TextureImage [^BufferedImage image texture-profile]
   (make-texture-image image texture-profile false))
  (^Graphics$TextureImage [^BufferedImage image texture-profile compress?]
   (make-texture-image image texture-profile compress? true))
  (^Graphics$TextureImage [^BufferedImage image texture-profile compress? flip-y?]
   (let [^Graphics$TextureProfile texture-profile-data (some->> texture-profile (protobuf/map->pb Graphics$TextureProfile))]
     (TextureGenerator/generate image texture-profile-data ^boolean compress? (if ^boolean flip-y? (EnumSet/of TexcLibrary$FlipAxis/FLIP_AXIS_Y) (EnumSet/noneOf TexcLibrary$FlipAxis))))))

(defn- make-preview-profile
  "Given a texture-profile, return a simplified texture-profile that can be used
  for previewing purposes in editor. Will only produce data for one texture
  format."
  [{:keys [name platforms] :as texture-profile}]
  (let [platform-profile (or (first (filter #(= :os-id-generic (:os %)) (:platforms texture-profile)))
                             (first (:platforms texture-profile)))
        texture-format   (first (:formats platform-profile))]
    (when (and platform-profile texture-format)
      {:name      "editor"
       :platforms [{:os                :os-id-generic
                    :formats           [(update texture-format :format texture-format->editor-format)]
                    :mipmaps           (:mipmaps platform-profile)
                    :max-texture-size  (:max-texture-size platform-profile)
                    :premultiply-alpha (:premultiply-alpha platform-profile)}]})))

(defn make-preview-texture-image
  ^Graphics$TextureImage [^BufferedImage image texture-profile]
  (let [preview-profile (make-preview-profile texture-profile)]
    (make-texture-image image preview-profile false)))

(defn make-cubemap-texture-images
  ^Graphics$TextureImage [images texture-profile compress?]
  (let [^Graphics$TextureProfile texture-profile-data (some->> texture-profile (protobuf/map->pb Graphics$TextureProfile))
        flip-axis (EnumSet/noneOf TexcLibrary$FlipAxis)]
    (util/map-vals #(TextureGenerator/generate ^BufferedImage % texture-profile-data ^boolean compress? flip-axis)
                   images)))

(defn assemble-texture-images
  ^Graphics$TextureImage [texture-images max-page-count]
  (let [texture-images (into-array Graphics$TextureImage texture-images)
        texture-type (if (pos? max-page-count)
                       Graphics$TextureImage$Type/TYPE_2D_ARRAY
                       Graphics$TextureImage$Type/TYPE_2D)]
    (TextureUtil/createCombinedTextureImage texture-images texture-type)))

(defn assemble-cubemap-texture-images
  ^Graphics$TextureImage [side->texture-image]
  (let [texture-images (into-array ((juxt :px :nx :py :ny :pz :nz) side->texture-image))
        type Graphics$TextureImage$Type/TYPE_CUBEMAP]
    (TextureUtil/createCombinedTextureImage texture-images type)))

(defn make-preview-cubemap-texture-images
  ^Graphics$TextureImage [images texture-profile]
  (let [preview-profile (make-preview-profile texture-profile)]
    (make-cubemap-texture-images images preview-profile false)))

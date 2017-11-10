(ns editor.pipeline.tex-gen
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline TextureGenerator TextureUtil]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$TextureFormat Graphics$TextureProfiles Graphics$TextureProfile]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(def ^:private texture-format->editor-format
  {:texture-format-luminance         :texture-format-luminance
   :texture-format-rgb               :texture-format-rgb
   :texture-format-rgba              :texture-format-rgba
   :texture-format-rgb-pvrtc-2bppv   :texture-format-rgb
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

(defn match-texture-profile
  [texture-profiles ^String path]
  (let [texture-profiles-data (some->> texture-profiles (protobuf/map->pb Graphics$TextureProfiles))
        path (if (.startsWith path "/") (subs path 1) path)]
    (when-some [texture-profile (TextureUtil/getTextureProfileByPath texture-profiles-data path)]
      (protobuf/pb->map texture-profile))))

(defn make-texture-image
  (^Graphics$TextureImage [^BufferedImage image texture-profile]
   (make-texture-image image texture-profile false))
  (^Graphics$TextureImage [^BufferedImage image texture-profile compress?]
   (let [^Graphics$TextureProfile texture-profile-data (some->> texture-profile (protobuf/map->pb Graphics$TextureProfile))]
     (TextureGenerator/generate image texture-profile-data ^boolean compress?))))

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

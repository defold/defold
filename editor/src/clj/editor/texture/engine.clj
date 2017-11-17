(ns editor.texture.engine
  (:require [editor.image-util :refer [image-pixels image-convert-type image-color-components]]
            [editor.types :refer [map->EngineFormatTexture]]
            [editor.buffers :refer [little-endian new-byte-buffer]]
            [editor.texture.math :refer [closest-power-of-two]])
  (:import [java.awt.image BufferedImage ColorModel]
           [java.nio ByteBuffer]
           [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat Graphics$TextureFormatAlternative$CompressionLevel]
           [com.defold.libs TexcLibrary TexcLibrary$ColorSpace TexcLibrary$PixelFormat TexcLibrary$CompressionType TexcLibrary$DitherType]
           [com.sun.jna Pointer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* true)

; Set up some compile-time aliases for those long Java constant names
(def L8       TexcLibrary$PixelFormat/L8)
(def R8G8B8   TexcLibrary$PixelFormat/R8G8B8)
(def R8G8B8A8 TexcLibrary$PixelFormat/R8G8B8A8)

(def TEXTURE_FORMAT_LUMINANCE Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_LUMINANCE)
(def TEXTURE_FORMAT_RGB       Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGB)
(def TEXTURE_FORMAT_RGBA      Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA)

(def SRGB     TexcLibrary$ColorSpace/SRGB)

(def default-formats
  [R8G8B8A8 TEXTURE_FORMAT_RGBA])

(def formats
  {1 [L8       TEXTURE_FORMAT_LUMINANCE]
   3 [R8G8B8   TEXTURE_FORMAT_RGB]
   4 [R8G8B8A8 TEXTURE_FORMAT_RGBA]})

(defn- image->byte-buffer
  "This is specialized for use in texture writing. It assumes the image
   has already been converted to a 4-byte packed (not planar) format."
  [^BufferedImage img]
  (let [width     (.getWidth img)
        height    (.getHeight img)
        data-size (* 4 width height)
        raster    (image-pixels img)
        buffer    (little-endian (new-byte-buffer data-size))]
    (doseq [b raster]
      (.put buffer (byte (bit-and b 0xff))))
    (.flip buffer)
    buffer))

(defmacro do-or-do-not!
  [& pairs]
  (list* 'do
         (for [[action msg] pairs]
           `(when-not ~action
              (throw (Exception. (str ~msg)))))))

(defn- resize [texture width height width-pot height-pot]
  (or (and (= width width-pot) (= height height-pot))
      (TexcLibrary/TEXC_Resize texture width-pot height-pot)))

(defn- premultiply-alpha [texture]
  (or (.. ColorModel getRGBdefault isAlphaPremultiplied)
      (TexcLibrary/TEXC_PreMultiplyAlpha texture)))

(defn- gen-mipmaps [texture]
  (TexcLibrary/TEXC_GenMipMaps texture))

(defn- transcode [texture pixel-format color-model compression-level]
  (TexcLibrary/TEXC_Transcode texture pixel-format color-model compression-level TexcLibrary$CompressionType/CT_DEFAULT TexcLibrary$DitherType/DT_DEFAULT))

(defn double-down [[n m]] [(max (bit-shift-right n 1) 1)
                           (max (bit-shift-right m 1) 1)])

(defn double-down-pairs [m n]
  (assert (and (> m 0) (> n 0)) "Both arguments must be greater than zero")
  (concat (take-while #(not= [1 1] %) (iterate double-down [m n])) [[1 1]]))

(defn- mipmap-sizes [width height color-count]
  (let [dims (double-down-pairs width height)]
    (map (fn [[w h]] (int (* w h color-count))) dims)))

(defn- mipmap-offsets [sizes]
  (reductions (fn [off sz] (int (+ off sz))) (int 0) sizes))

(defn texture-engine-format-generate
  [^BufferedImage img]
  (let [img                           (image-convert-type img BufferedImage/TYPE_4BYTE_ABGR)
        width                         (.getWidth img)
        height                        (.getHeight img)
        width-pot                     (int (closest-power-of-two width))
        height-pot                    (int (closest-power-of-two height))
        color-count                   (image-color-components img)
        [pixel-format texture-format] (get formats color-count default-formats)
        texture                       (TexcLibrary/TEXC_Create width height R8G8B8A8 SRGB (image->byte-buffer img))
        compression-level             Graphics$TextureFormatAlternative$CompressionLevel/FAST]
    (try
      (do-or-do-not!
        (resize texture width height width-pot height-pot)      "could not resize texture to POT"
        (premultiply-alpha texture)                             "could not premultiply alpha"
        (gen-mipmaps texture)                                   "could not generate mip-maps"
        (transcode texture pixel-format SRGB compression-level) "could not transcode")
      (let [buffer-size  (* width-pot height-pot color-count 2)
            buffer       (little-endian (new-byte-buffer buffer-size))
            data-size    (TexcLibrary/TEXC_GetData texture buffer buffer-size)
            mipmap-sizes (mipmap-sizes width-pot height-pot color-count)]
        (map->EngineFormatTexture
          {:width           width-pot
           :height          height-pot
           :original-width  width
           :original-height height
           :format          texture-format
           :data            (.limit buffer data-size)
           :mipmap-sizes    mipmap-sizes
           :mipmap-offsets (mipmap-offsets mipmap-sizes)}))
      (finally
        (TexcLibrary/TEXC_Destroy texture)))))

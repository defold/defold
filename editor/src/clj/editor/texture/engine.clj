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

(ns editor.texture.engine
  (:require [editor.buffers :refer [little-endian new-byte-buffer]]
            [editor.image-util :refer [image-color-components image-convert-type image-pixels]]
            [editor.texture.math :refer [closest-power-of-two]]
            [editor.types :refer [map->EngineFormatTexture]])
  (:import [com.dynamo.bob.pipeline Texc Texc$DefaultEncodeSettings TexcLibraryJni Texc$ColorSpace Texc$CompressionLevel Texc$CompressionType Texc$PixelFormat]
           [com.dynamo.graphics.proto Graphics$TextureFormatAlternative$CompressionLevel Graphics$TextureImage$TextureFormat]
           [java.awt.image BufferedImage ColorModel]
           [java.nio ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* true)

; Set up some compile-time aliases for those long Java constant names
(def L8       Texc$PixelFormat/PF_L8)
(def R8G8B8   Texc$PixelFormat/PF_R8G8B8)
(def R8G8B8A8 Texc$PixelFormat/PF_R8G8B8A8)

(def CL_FAST   Texc$CompressionLevel/CL_FAST)
(def CL_NORMAL Texc$CompressionLevel/CL_NORMAL)
(def CL_HIGH   Texc$CompressionLevel/CL_HIGH)
(def CL_BEST   Texc$CompressionLevel/CL_BEST)

(def CT_DEFAULT     Texc$CompressionType/CT_DEFAULT)

(def TEXTURE_FORMAT_LUMINANCE Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_LUMINANCE)
(def TEXTURE_FORMAT_RGB       Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGB)
(def TEXTURE_FORMAT_RGBA      Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA)

(def SRGB     Texc$ColorSpace/CS_SRGB)

(def default-formats
  [R8G8B8A8 TEXTURE_FORMAT_RGBA])

(def formats
  {1 [L8       TEXTURE_FORMAT_LUMINANCE]
   3 [R8G8B8   TEXTURE_FORMAT_RGB]
   4 [R8G8B8A8 TEXTURE_FORMAT_RGBA]})

(defn- channel-count->pixel-format [^long channel-count]
  (case channel-count
    1 L8
    3 R8G8B8
    4 R8G8B8A8))

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
      (TexcLibraryJni/Resize texture width-pot height-pot)))

(defn- premultiply-alpha [texture]
  (or (.. ColorModel getRGBdefault isAlphaPremultiplied)
      (TexcLibraryJni/PreMultiplyAlpha texture)))

(defn- gen-mipmaps [texture])

(defn num-texc-threads []
  (let [count (.availableProcessors (Runtime/getRuntime))]
    (cond (> count 4) (- count 2)
          (> count 1) (- count 1)
          :else 1)))

(defn- transcode [texture pixel-format color-space compression-type _compression-level]
  (if (= compression-type CT_DEFAULT)
    (let [^Texc$DefaultEncodeSettings encode-settings (Texc$DefaultEncodeSettings.)
          data-uncompressed (TexcLibraryJni/GetData texture)
          width             (TexcLibraryJni/GetWidth texture)
          height            (TexcLibraryJni/GetHeight texture)]
      (set! (. encode-settings width) width)
      (set! (. encode-settings height) height)
      (set! (. encode-settings pixelFormat) pixel-format)
      (set! (. encode-settings colorSpace) color-space)
      (set! (. encode-settings data) data-uncompressed)
      (set! (. encode-settings numThreads) (num-texc-threads))
      (set! (. encode-settings outPixelFormat) pixel-format)
      (TexcLibraryJni/DefaultEncode encode-settings))))

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
        name                          nil ; for easier debugging
        texture                       (TexcLibraryJni/CreateImage name, width height R8G8B8A8 SRGB (image->byte-buffer img))
        compression-level             Graphics$TextureFormatAlternative$CompressionLevel/FAST
        mipmaps                       false
        texture-mip-zero              (resize texture width height width-pot height-pot)]

    (try
      (do-or-do-not!
        (premultiply-alpha texture-mip-zero) "could not premultiply alpha"
        (gen-mipmaps texture-mip-zero)       "could not generate mip-maps")
      (let [buffer       (transcode texture-mip-zero pixel-format SRGB compression-level CT_DEFAULT)
            mipmap-sizes (mipmap-sizes width-pot height-pot color-count)]
        (map->EngineFormatTexture
          {:width           width-pot
           :height          height-pot
           :original-width  width
           :original-height height
           :format          texture-format
           :data            buffer
           :mipmap-sizes    mipmap-sizes
           :mipmap-offsets (mipmap-offsets mipmap-sizes)}))
      (finally
        (TexcLibraryJni/DestroyImage texture)))))

(defn compress-rgba-buffer
  ^ByteBuffer [^ByteBuffer rgba-buffer ^long width ^long height ^long channel-count]
  (assert (pos? width))
  (assert (pos? height))
  (assert (pos? channel-count))
  (assert (= (* width height channel-count) (.remaining rgba-buffer)) "Buffer size mismatch.")
  (let [rgba-buffer-size (.remaining rgba-buffer)
        uncompressed-bytes (byte-array rgba-buffer-size)
        _ (.get rgba-buffer uncompressed-bytes)
        compressed-buffer (TexcLibraryJni/CompressBuffer uncompressed-bytes) ; Texc.Buffer from Texc.java
        compressed-data (.data compressed-buffer)
        data-with-header (doto (ByteBuffer/allocateDirect (+ 1 (alength compressed-data)))
                                                  (.put (byte 1)) ; First byte is the header, where 0 = uncompressed, and 1 = compressed
                                                  (.put compressed-data) ; the payload
                                                  (.flip))]
    data-with-header))

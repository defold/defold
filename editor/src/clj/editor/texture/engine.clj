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

(ns editor.texture.engine
  (:require [editor.buffers :as buffers]
            [editor.dialogs :as dialogs]
            [editor.localization :as localization]
            [editor.image-util :refer [image-pixels]]
            [editor.ui :as ui]
            [internal.java :as java]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :as coll :refer [pair]])
  (:import [com.defold.extension.pipeline.texture TextureCompression ITextureCompressor]
           [com.dynamo.bob ClassLoaderScanner]
           [com.dynamo.bob.pipeline Texc$PixelFormat TexcLibraryJni]
           [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* true)

(defonce ^:private ^String scanned-package-name "com.defold.extension.pipeline.texture")

; Set up some compile-time aliases for those long Java constant names
(def L8       Texc$PixelFormat/PF_L8)
(def R8G8B8   Texc$PixelFormat/PF_R8G8B8)
(def R8G8B8A8 Texc$PixelFormat/PF_R8G8B8A8)


(def TEXTURE_FORMAT_LUMINANCE Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_LUMINANCE)
(def TEXTURE_FORMAT_RGB       Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGB)
(def TEXTURE_FORMAT_RGBA      Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA)

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
  (let [width (.getWidth img)
        height (.getHeight img)
        data-size (* 4 width height)
        raster (image-pixels img)
        buffer (buffers/new-byte-buffer data-size :byte-order/little-endian)]
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

(defn double-down [[n m]] [(max (bit-shift-right n 1) 1)
                           (max (bit-shift-right m 1) 1)])

(defn double-down-pairs [m n]
  (assert (and (> m 0) (> n 0)) "Both arguments must be greater than zero")
  (concat (take-while #(not= [1 1] %) (iterate double-down [m n])) [[1 1]]))

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
                           ;; First byte is the header, where 0 = uncompressed, and 1 = compressed
                           (.put (byte 1))
                           ;; the payload
                           (.put compressed-data)
                           (.flip))]
    data-with-header))

(defn- try-initialize-texture-compressors [^ClassLoader class-loader]
  (util/group-into
    {} []
    key val
    (keep (fn [^String class-name]
            (try
              (let [uninitialized-class (Class/forName class-name false class-loader)]
                (when (java/public-implementation? uninitialized-class ITextureCompressor)
                  (let [initialized-class (Class/forName class-name true class-loader)]
                    (pair :texture-compressor-classes initialized-class))))
              (catch Exception e
                (log/error :msg (str "Exception in static initializer of ITextureCompressor implementation " class-name ": " (.getMessage e))
                           :exception e)
                (pair :faulty-class-names class-name))))
          (ClassLoaderScanner/scanClassLoader class-loader scanned-package-name))))

(defn- set-texture-compressors! [texture-compressor-classes]
  (run! (fn [^Class texture-compressor-class]
          (let [texture-compressor-instance (java/invoke-no-arg-constructor texture-compressor-class)]
            (TextureCompression/registerCompressor texture-compressor-instance)))
        texture-compressor-classes))

(defn- report-error! [faulty-class-names localization]
  (ui/run-later
    (dialogs/make-info-dialog
      localization
      {:title (localization/message "dialog.texture-compressor-error.title")
       :size :large
       :icon :icon/triangle-error
       :always-on-top true
       :header (localization/message "dialog.texture-compressor-error.header")
       :content (localization/message "dialog.texture-compressor-error.content"
                                      {"classes" (->> faulty-class-names
                                                      sort
                                                      (map dialogs/indent-with-bullet)
                                                      (coll/join-to-string "\n"))})})))

(defn reload-texture-compressors! [^ClassLoader class-loader localization]
  (let [{:keys [texture-compressor-classes faulty-class-names]}
        (try-initialize-texture-compressors class-loader)]
    (if (seq faulty-class-names)
      (report-error! faulty-class-names localization)
      (set-texture-compressors! texture-compressor-classes))))

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

(ns editor.gl.texture
  "Functions for creating and using textures"
  (:require [editor.gl :as gl]
            [editor.gl.types :as gl.types]
            [editor.graphics.types :as graphics.types]
            [editor.image-util :as image-util]
            [editor.scene-cache :as scene-cache]
            [internal.util :as util]
            [service.log :as log]
            [util.defonce :as defonce])
  (:import [com.dynamo.bob.pipeline TextureGenerator$GenerateResult]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$TextureFormat]
           [com.jogamp.opengl GL GL2 GL3 GLProfile]
           [com.jogamp.opengl.util.awt ImageUtil]
           [com.jogamp.opengl.util.texture Texture TextureData TextureIO]
           [java.awt.image BufferedImage DataBufferByte]
           [java.nio Buffer ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def
  ^{:doc "Special constant used as the page-count for non-paged textures.
          A texture can be paged, but only have one page. A value of zero means
          it is not a paged texture. Built as TYPE_2D, not TYPE_2D_ARRAY."}
  ^:const ^:long non-paged-page-count 0)

(defn texture-param->gl-texture-param
  "This function translates Clojure keywords into OpenGL constants. You can use
  the keywords in texture parameter maps."
  ^long [texture-param]
  (case texture-param
    :base-level GL2/GL_TEXTURE_BASE_LEVEL
    :border-color GL2/GL_TEXTURE_BORDER_COLOR
    :compare-func GL2/GL_TEXTURE_COMPARE_FUNC
    :compare-mode GL2/GL_TEXTURE_COMPARE_MODE
    :lod-bias GL2/GL_TEXTURE_LOD_BIAS
    :min-filter GL/GL_TEXTURE_MIN_FILTER
    :mag-filter GL/GL_TEXTURE_MAG_FILTER
    :min-lod GL2/GL_TEXTURE_MIN_LOD
    :max-lod GL2/GL_TEXTURE_MAX_LOD
    :max-level GL2/GL_TEXTURE_MAX_LEVEL
    :swizzle-r GL3/GL_TEXTURE_SWIZZLE_R
    :swizzle-g GL3/GL_TEXTURE_SWIZZLE_G
    :swizzle-b GL3/GL_TEXTURE_SWIZZLE_B
    :swizzle-a GL3/GL_TEXTURE_SWIZZLE_A
    :wrap-s GL2/GL_TEXTURE_WRAP_S
    :wrap-t GL2/GL_TEXTURE_WRAP_T
    :wrap-r GL2/GL_TEXTURE_WRAP_R
    -1))

(defn- gl-texture-filter->non-mipmap
  ^long [^long gl-texture-filter]
  (if (or (= GL2/GL_NEAREST gl-texture-filter)
          (= GL2/GL_NEAREST_MIPMAP_NEAREST gl-texture-filter)
          (= GL2/GL_NEAREST_MIPMAP_LINEAR gl-texture-filter))
    GL2/GL_NEAREST
    GL2/GL_LINEAR))

(defn- apply-params!
  [^GL2 gl ^long texture-target params has-mipmaps]
  ;; TODO(instancing): Our texture parameters are associated with a named
  ;; sampler in the material, not with a particular texture resource. When
  ;; applying texture parameters, we should match the texture-target to the
  ;; uniform it is assigned to. The code below is a hack that will apply the
  ;; default texture parameters if the bound shader does not have a uniform
  ;; matching the sampler-name, but this needs to be fixed properly.
  ;;
  ;; HACK: If the texture params apply to a specific sampler, but that sampler
  ;; does not exist in the shader, use the default texture params.
  (let [program (gl/gl-current-program gl)]
    (when-not (zero? program)
      (let [sampler-name (:name params)
            params (if (nil? sampler-name)
                     ;; These params are not associated with a specific sampler.
                     params

                     ;; These params are associated with a specific sampler. Use
                     ;; the default params if we can't find a matching uniform
                     ;; for the sampler-name (HACK).
                     (if (= -1 (.glGetUniformLocation gl program sampler-name))
                       (:default-tex-params params)
                       params))]
        (doseq [[param ^int value] params]
          (let [gl-param (int (if (integer? param)
                                param
                                (texture-param->gl-texture-param param)))]
            (case gl-param
              -1 (case param
                   (:default-tex-params :name) nil ; Silently ignore extra sampler data
                   (log/warn :message "ignoring unknown texture parameter" :param param :value value))
              (let [gl-value (int (if (and (= GL/GL_TEXTURE_MIN_FILTER gl-param)
                                           (not has-mipmaps))
                                    (gl-texture-filter->non-mipmap value)
                                    value))]
                (.glTexParameteri gl texture-target gl-param gl-value)))))))))

(defn- merge-request-ids [request-id sub-request-id]
  (if (vector? request-id)
    (conj request-id sub-request-id)
    [request-id sub-request-id]))

(defn- texture-data-has-mipmaps? [^TextureData texture-data]
  (< 1 (count (.getMipmapData texture-data))))

(defonce/protocol TextureProxy
  (->texture ^Texture [this ^GL2 gl texture-array-index]))

(declare texture-lifecycle->texture bind-texture-lifecycle! unbind-texture-lifecycle!)

(defonce/record TextureLifecycle [request-id cache-id params texture-datas texture-units]
  TextureProxy
  (->texture [this gl texture-array-index]
    (texture-lifecycle->texture this gl texture-array-index))

  gl.types/GLBinding
  (bind! [this gl _render-args]
    (bind-texture-lifecycle! this gl))

  (unbind! [this gl _render-args]
    (unbind-texture-lifecycle! this gl)))

(defn- texture-lifecycle->texture
  ^Texture [^TextureLifecycle texture-lifecycle gl texture-array-index]
  (let [request-id (.-request-id texture-lifecycle)
        cache-id (.-cache-id texture-lifecycle)
        texture-datas (.-texture-datas texture-lifecycle)
        texture-request-id (merge-request-ids request-id texture-array-index)
        texture-data (texture-datas texture-array-index)]
    (scene-cache/request-object! cache-id texture-request-id gl texture-data)))

(defn- bind-texture-lifecycle!
  [^TextureLifecycle texture-lifecycle gl]
  (let [params (.-params texture-lifecycle)
        texture-datas (.-texture-datas texture-lifecycle)
        texture-units (.-texture-units texture-lifecycle)]
    (doseq [texture-unit-index (range 0 (min (count texture-units) (count texture-datas)))]
      (let [texture-unit (int (texture-units texture-unit-index))
            texture-data (texture-datas texture-unit-index)
            gl-texture-unit (+ texture-unit GL2/GL_TEXTURE0)
            has-mipmaps (if (map? texture-data) ; Cubemaps have maps of side keywords to TextureData.
                          (some-> texture-data first val texture-data-has-mipmaps?)
                          (texture-data-has-mipmaps? texture-data))]
        (.glActiveTexture ^GL2 gl gl-texture-unit) ; Set the active texture unit. Implicit parameter to (.bind ...) and (texture-lifecycle->texture ...)
        (let [texture (texture-lifecycle->texture texture-lifecycle gl texture-unit-index)
              gl-target (.getTarget texture)]
          (.enable texture gl)                                 ; Enable the type of texturing e.g. GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP
          (.bind texture gl)                                   ; Bind our texture to the active texture unit. Used for subsequent render calls. Also implicit parameter to (apply-params! ...)
          (apply-params! gl gl-target params has-mipmaps)))))) ; Apply filtering settings to the bound texture

(defn- unbind-texture-lifecycle!
  [^TextureLifecycle texture-lifecycle gl]
  (let [texture-datas (.-texture-datas texture-lifecycle)
        texture-units (.-texture-units texture-lifecycle)]
    (doseq [texture-unit-index (range 0 (min (count texture-units) (count texture-datas)))]
      (let [texture-unit (int (texture-units texture-unit-index))
            gl-texture-unit (+ texture-unit GL2/GL_TEXTURE0)]
        (.glActiveTexture ^GL2 gl gl-texture-unit) ; Set the active texture unit. Implicit parameter to (.glBindTexture ...) and (texture-lifecycle->texture ...)
        (let [tex (texture-lifecycle->texture texture-lifecycle gl texture-unit-index)
              tgt (.getTarget tex)]
          (.glBindTexture ^GL2 gl tgt 0)             ; Re-bind default "no-texture" to the active texture unit
          (.glActiveTexture ^GL2 gl GL/GL_TEXTURE0)  ; Set TEXTURE0 as the active texture unit in case anything outside of the bind / unbind cycle forgets to call (.glActiveTexture ...)
          (.disable tex gl))))))                     ; Disable the type of texturing e.g. GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP

(defn texture-lifecycle? [value]
  (instance? TextureLifecycle value))

(defn set-params [^TextureLifecycle tlc params]
  (update tlc :params merge params))

(def
  ^{:doc "If you do not supply parameters to `image-texture`, these will be used as defaults."}
  default-image-texture-params
  {:min-filter gl/linear-mipmap-linear
   :mag-filter gl/linear
   :wrap-s     gl/clamp
   :wrap-t     gl/clamp})

(defn- data-format->internal-format [data-format]
  ;; Internal format signifies only color content, not channel order.
  (case data-format
    :gray GL2/GL_LUMINANCE
    :bgr  GL2/GL_RGB
    :abgr GL2/GL_RGBA
    :rgb  GL2/GL_RGB
    :rgba GL2/GL_RGBA))

(defn- data-format->pixel-format [data-format]
  ;; Pixel format signifies channel order.
  (case data-format
    :gray GL2/GL_LUMINANCE
    :bgr  GL2/GL_BGR
    :abgr GL2/GL_RGBA ;; There is no GL_ABGR, so this is swizzled into ABGR by the GL_UNSIGNED_INT_8_8_8_8 type returned by data-format->type.
    :rgb  GL2/GL_RGB
    :rgba GL2/GL_RGBA))

(defn- data-format->type [data-format]
  ;; Type signifies packing / endian order.
  (case data-format
    :gray GL2/GL_UNSIGNED_BYTE
    :bgr  GL2/GL_UNSIGNED_BYTE
    :abgr GL2/GL_UNSIGNED_INT_8_8_8_8
    :rgb  GL2/GL_UNSIGNED_BYTE
    :rgba GL2/GL_UNSIGNED_BYTE))

(defn- image-type->data-format [^long image-type]
  (condp = image-type
    BufferedImage/TYPE_BYTE_GRAY :gray
    BufferedImage/TYPE_3BYTE_BGR :bgr
    BufferedImage/TYPE_4BYTE_ABGR :abgr
    BufferedImage/TYPE_4BYTE_ABGR_PRE :abgr))

(defn- ->texture-data [width height data-format data mipmap]
  (let [internal-format (data-format->internal-format data-format)
        pixel-format (data-format->pixel-format data-format)
        type (data-format->type data-format)
        border 0]
    (TextureData. (GLProfile/getGL2GL3) internal-format width height border pixel-format type mipmap false false data nil)))

(defn- image->texture-data ^TextureData [img mipmap]
  (let [channels (image-util/image-color-components img)
        type (case channels
               4 BufferedImage/TYPE_4BYTE_ABGR_PRE
               3 BufferedImage/TYPE_3BYTE_BGR
               1 BufferedImage/TYPE_BYTE_GRAY)
        img (image-util/image-convert-type img type)
        data-type (image-type->data-format type)
        data (ByteBuffer/wrap (.getData ^DataBufferByte (.getDataBuffer (.getRaster img))))]
    (->texture-data (.getWidth img) (.getHeight img) data-type data mipmap)))

(defn- flip-y [^BufferedImage img]
  ;; Flip the image before we create a OGL texture, to mimic how UVs are handled in engine.
  ;; We do this since all our generated UVs are based on bottom-left being texture coord (0,0).
  (let [cm (.getColorModel img)
        raster (.copyData img nil)]
    (doto (BufferedImage. cm raster (.isAlphaPremultiplied cm) nil)
      (ImageUtil/flipImageVertically))))

(defn image-texture
  "Create an image texture from a BufferedImage. The returned value satisfies
  the GLBinding protocol.

  If supplied, the params argument must be a map of parameter name to value.
  Parameter names can be OpenGL constants (e.g., GL_TEXTURE_WRAP_S) or their
  keyword equivalents from `texture-params` (e.g., :wrap-s).

  If you supply parameters, then those parameters are used. If you do not supply
  parameters, then defaults in `default-image-texture-params` are used.

  If supplied, the unit is the offset of GL_TEXTURE0, i.e. 0 => GL_TEXTURE0. The
  default is 0."
  ([request-id img]
   (image-texture request-id img default-image-texture-params 0))
  ([request-id img params]
   (image-texture request-id img params 0))
  ([request-id ^BufferedImage img params unit-index]
   {:pre [(graphics.types/request-id? request-id)]}
   (let [texture-datas [(image->texture-data (flip-y (or img (:contents image-util/placeholder-image))) true)]
         texture-units (vector-of :int unit-index)]
     (->TextureLifecycle request-id ::texture params texture-datas texture-units))))


(def format->gl-format
  {Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_LUMINANCE
   GL2/GL_LUMINANCE

   Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGB
   GL2/GL_RGB

   Graphics$TextureImage$TextureFormat/TEXTURE_FORMAT_RGBA
   GL2/GL_RGBA})

(defn- select-texture-image-image
  ^Graphics$TextureImage$Image [^Graphics$TextureImage texture-image]
  (first (filter #(format->gl-format (.getFormat ^Graphics$TextureImage$Image %))
                 (.getAlternativesList texture-image))))

(defn- image->mipmap-buffers
  ^"[Ljava.nio.Buffer;" [^Graphics$TextureImage$Image image mip-image-byte-arrays]
  (assert (= (.getMipMapSizeCount image) (.getMipMapOffsetCount image) (count mip-image-byte-arrays)))
  (let [mipmap-count (.getMipMapSizeCount image)
        ^"[Ljava.nio.Buffer;" bufs (make-array Buffer mipmap-count)]
    (loop [i 0]
      (if (< i mipmap-count)
        (let [buf (ByteBuffer/wrap (nth mip-image-byte-arrays i))]
          (aset bufs i buf)
          (recur (inc i)))
        bufs))))

(defn- texture-image->texture-data
  ^TextureData [^TextureGenerator$GenerateResult texture-generate-result]
  (let [texture-image (.textureImage texture-generate-result)
        mip-image-byte-arrays (.imageDatas texture-generate-result)
        image (select-texture-image-image texture-image)
        gl-profile (GLProfile/getGL2GL3)
        gl-format (int (format->gl-format (.getFormat image)))
        mipmap-buffers (image->mipmap-buffers image mip-image-byte-arrays)]
    (TextureData. gl-profile
                  gl-format
                  (.getWidth image)
                  (.getHeight image)
                  0                       ; border
                  gl-format
                  GL/GL_UNSIGNED_BYTE     ; gl type
                  false                   ; compressed?
                  false                   ; flip vertically?
                  mipmap-buffers
                  nil)))


(defn texture-images->gpu-texture
  ([request-id texture-images]
   (texture-images->gpu-texture request-id texture-images default-image-texture-params 0))
  ([request-id texture-images params]
   (texture-images->gpu-texture request-id texture-images params 0))
  ([request-id texture-images params ^long start-texture-unit]
   {:pre [(graphics.types/request-id? request-id)]}
   (let [texture-datas (mapv texture-image->texture-data texture-images)
         texture-units (into (vector-of :int)
                             (range start-texture-unit (+ start-texture-unit (count texture-images))))]
     (->TextureLifecycle request-id ::texture params texture-datas texture-units))))

(defn texture-image->gpu-texture
  "Create an image texture from a generated `TextureImage`. The returned value
  satisfies the GLBinding protocol.

  If supplied, the params argument must be a map of parameter name to value.
  Parameter names can be OpenGL constants (e.g., GL_TEXTURE_WRAP_S) or their
  keyword equivalents from `texture-params` (e.g., :wrap-s).

  If you supply parameters, then those parameters are used. If you do not supply
  parameters, then defaults in `default-image-texture-params` are used.

  If supplied, the unit is the offset of GL_TEXTURE0, i.e. 0 => GL_TEXTURE0. The
  default is 0."
  ([request-id img]
   (texture-image->gpu-texture request-id img default-image-texture-params 0))
  ([request-id img params]
   (texture-image->gpu-texture request-id img params 0))
  ([request-id ^Graphics$TextureImage img params unit-index]
   {:pre [(graphics.types/request-id? request-id)]}
   (let [texture-datas [(texture-image->texture-data img)]
         texture-units (vector-of :int unit-index)]
      (->TextureLifecycle request-id ::texture params texture-datas texture-units))))

(defn empty-texture [request-id width height data-format params unit-index]
  {:pre [(graphics.types/request-id? request-id)]}
  (let [texture-datas [(->texture-data width height data-format nil false)]
        texture-units (vector-of :int unit-index)]
    (->TextureLifecycle request-id ::texture params texture-datas texture-units)))

(defonce white-pixel
  (delay
    (image-texture
      ::white
      (-> (image-util/blank-image 1 1)
          (image-util/flood 1.0 1.0 1.0))
      (assoc default-image-texture-params
        :min-filter GL2/GL_NEAREST
        :mag-filter GL2/GL_NEAREST))))

(defonce black-pixel
  (delay
    (image-texture
      ::black
      (-> (image-util/blank-image 1 1)
          (image-util/flood 0.0 0.0 0.0))
      (assoc default-image-texture-params
        :min-filter GL2/GL_NEAREST
        :mag-filter GL2/GL_NEAREST))))

(defn tex-sub-image [^GL2 gl ^TextureLifecycle texture page-index data x y w h data-format]
  (let [tex (->texture texture gl page-index)
        data (->texture-data w h data-format data false)]
    (.updateSubImage tex gl data 0 x y)))

(def default-cubemap-texture-params
  ^{:doc "If you do not supply parameters to `cubemap-texture-images->gpu-texture`, these will be used as defaults."}
  {:min-filter gl/linear
   :mag-filter gl/linear
   :wrap-s     gl/clamp-to-edge
   :wrap-t     gl/clamp-to-edge})

(defn cubemap-texture-images->gpu-texture
  ([request-id texture-images]
   (cubemap-texture-images->gpu-texture request-id texture-images default-cubemap-texture-params))
  ([request-id texture-images params]
   (cubemap-texture-images->gpu-texture request-id texture-images params 0))
  ([request-id texture-images params unit-index]
   {:pre [(graphics.types/request-id? request-id)]}
   (let [texture-datas [(util/map-vals texture-image->texture-data texture-images)]
         texture-units (vector-of :int unit-index)]
     (->TextureLifecycle request-id ::cubemap-texture params texture-datas texture-units))))

(defn- make-texture [^GL2 gl ^TextureData texture-data]
  (Texture. gl texture-data))

(defn- update-texture [^GL2 gl ^Texture texture ^TextureData texture-data]
  (.updateImage texture gl texture-data)
  texture)

(defn- destroy-textures [^GL2 gl textures _]
  (doseq [^Texture texture textures]
    (.destroy texture gl)))

(scene-cache/register-object-cache! ::texture make-texture update-texture destroy-textures)

(def ^:private cubemap-targets
  {:px GL/GL_TEXTURE_CUBE_MAP_POSITIVE_X
   :nx GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_X
   :py GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Y
   :ny GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
   :pz GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Z
   :nz GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Z})

(defn- update-cubemap-texture [^GL2 gl ^Texture texture texture-datas]
  (doseq [[target-key target] cubemap-targets]
    (.updateImage texture gl ^TextureData (target-key texture-datas) target))
  texture)

(defn- make-cubemap-texture [^GL2 gl texture-datas]
  (let [^Texture texture (TextureIO/newTexture GL/GL_TEXTURE_CUBE_MAP)]
    (update-cubemap-texture gl texture texture-datas)))

(scene-cache/register-object-cache! ::cubemap-texture make-cubemap-texture update-cubemap-texture destroy-textures)

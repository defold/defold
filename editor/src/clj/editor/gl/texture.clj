(ns editor.gl.texture
  "Functions for creating and using textures"
  (:require [editor.image-util :as image-util]
            [editor.gl.protocols :refer [GlBind]]
            [editor.gl :as gl]
            [internal.util :as util]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.scene-cache :as scene-cache])
  (:import [java.awt.image BufferedImage DataBufferByte]
           [java.nio Buffer ByteBuffer]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$TextureFormat]
           [com.jogamp.opengl GL GL2 GL3 GLProfile]
           [com.jogamp.opengl.util.texture Texture TextureIO TextureData]
           [com.jogamp.opengl.util.awt ImageUtil]))

(set! *warn-on-reflection* true)

(def
  ^{:doc "This map translates Clojure keywords into OpenGL constants.
          You can use the keywords in texture parameter maps."}
  texture-params
  {:base-level   GL2/GL_TEXTURE_BASE_LEVEL
   :border-color GL2/GL_TEXTURE_BORDER_COLOR
   :compare-func GL2/GL_TEXTURE_COMPARE_FUNC
   :compare-mode GL2/GL_TEXTURE_COMPARE_MODE
   :lod-bias     GL2/GL_TEXTURE_LOD_BIAS
   :min-filter   GL/GL_TEXTURE_MIN_FILTER
   :mag-filter   GL/GL_TEXTURE_MAG_FILTER
   :min-lod      GL2/GL_TEXTURE_MIN_LOD
   :max-lod      GL2/GL_TEXTURE_MAX_LOD
   :max-level    GL2/GL_TEXTURE_MAX_LEVEL
   :swizzle-r    GL3/GL_TEXTURE_SWIZZLE_R
   :swizzle-g    GL3/GL_TEXTURE_SWIZZLE_G
   :swizzle-b    GL3/GL_TEXTURE_SWIZZLE_B
   :swizzle-a    GL3/GL_TEXTURE_SWIZZLE_A
   :wrap-s       GL2/GL_TEXTURE_WRAP_S
   :wrap-t       GL2/GL_TEXTURE_WRAP_T
   :wrap-r       GL2/GL_TEXTURE_WRAP_R})

(defn- apply-params
  [gl ^Texture texture params]
  (doseq [[p v] params]
    (if-let [pname (texture-params p)]
      (.setTexParameteri texture gl pname v)
      (if (integer? p)
        (.setTexParameteri texture gl p v)
        (println "WARNING: ignoring unknown texture parameter " p)))))

(defprotocol TextureProxy
  (->texture ^Texture [this ^GL2 gl]))

(defrecord TextureLifecycle [request-id cache-id unit params texture-data]
  TextureProxy
  (->texture [this gl]
    (scene-cache/request-object! cache-id request-id gl texture-data))

  GlBind
  (bind [this gl _render-args]
    (let [tex (->texture this gl)]
      (.glActiveTexture ^GL2 gl unit)
      (.enable tex gl)
      (.bind tex gl)
      (apply-params gl tex params)))

  (unbind [this gl _render-args]
    (let [tex (->texture this gl)]
      (.glActiveTexture ^GL2 gl unit)
      (.disable tex gl))
    (.glActiveTexture ^GL2 gl GL/GL_TEXTURE0)))

(defn set-unit-index [^TextureLifecycle tlc ^long unit-index]
  (assert (<= 0 unit-index 7))
  (assoc tlc :unit (+ unit-index GL/GL_TEXTURE0)))

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
  "Create an image texture from a BufferedImage. The returned value
supports GlBind and GlEnable. You can use it in do-gl and with-gl-bindings.

If supplied, the params argument must be a map of parameter name to value. Parameter names
can be OpenGL constants (e.g., GL_TEXTURE_WRAP_S) or their keyword equivalents from
`texture-params` (e.g., :wrap-s).

If you supply parameters, then those parameters are used. If you do not supply parameters,
then defaults in `default-image-texture-params` are used.

If supplied, the unit is the offset of GL_TEXTURE0, i.e. 0 => GL_TEXTURE0. The default is 0."
  ([request-id img]
   (image-texture request-id img default-image-texture-params 0))
  ([request-id img params]
   (image-texture request-id img params 0))
  ([request-id ^BufferedImage img params unit-index]
    (let [texture-data (image->texture-data (flip-y (or img (:contents image-util/placeholder-image))) true)
          unit (+ unit-index GL2/GL_TEXTURE0)]
      (->TextureLifecycle request-id ::texture unit params texture-data))))


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
  ^"[Ljava.nio.Buffer;" [^Graphics$TextureImage$Image image]
  (assert (= (.getMipMapSizeCount image) (.getMipMapOffsetCount image)))
  (let [mipmap-count (.getMipMapSizeCount image)
        data (.toByteArray (.getData image))
        ^"[Ljava.nio.Buffer;" bufs (make-array Buffer mipmap-count)]
    (loop [i 0]
      (if (< i mipmap-count)
        (let [buf (ByteBuffer/wrap data (.getMipMapOffset image i) (.getMipMapSize image i))]
          (aset bufs i buf)
          (recur (inc i)))
        bufs))))

(defn- texture-image->texture-data
  ^TextureData [^Graphics$TextureImage texture-image]
  (let [image            (select-texture-image-image texture-image)
        gl-profile       (GLProfile/getGL2GL3)
        gl-format        (int (format->gl-format (.getFormat image)))
        mipmap-buffers   (image->mipmap-buffers image)]
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

(defn texture-data->gpu-texture
  "Create an image texture from a generated `TextureData`. The returned value
  supports GlBind and GlEnable. You can use it in do-gl and with-gl-bindings.

  If supplied, the params argument must be a map of parameter name to value.
  Parameter names can be OpenGL constants (e.g., GL_TEXTURE_WRAP_S) or their
  keyword equivalents from `texture-params` (e.g., :wrap-s).

  If you supply parameters, then those parameters are used. If you do not supply
  parameters, then defaults in `default-image-texture-params` are used.

  If supplied, the unit is the offset of GL_TEXTURE0, i.e. 0 => GL_TEXTURE0. The
  default is 0."
  ([request-id texture-data]
   (texture-data->gpu-texture request-id texture-data default-image-texture-params 0))
  ([request-id texture-data params]
   (texture-data->gpu-texture request-id texture-data params 0))
  ([request-id texture-data params unit-index]
   (let [unit (+ unit-index GL2/GL_TEXTURE0)]
     (->TextureLifecycle request-id ::texture unit params texture-data))))

(def make-preview-texture-data (comp texture-image->texture-data tex-gen/make-preview-texture-image))
(def make-preview-cubemap-texture-datas (comp (partial util/map-vals texture-image->texture-data) tex-gen/make-preview-cubemap-texture-images))

(defn empty-texture [request-id width height data-format params unit-index]
  (let [unit (+ unit-index GL2/GL_TEXTURE0)]
    (->TextureLifecycle request-id ::texture unit params (->texture-data width height data-format nil false))))

(def white-pixel (image-texture ::white (-> (image-util/blank-image 1 1) (image-util/flood 1.0 1.0 1.0)) (assoc default-image-texture-params
                                                                                                                :min-filter GL2/GL_NEAREST
                                                                                                                :mag-filter GL2/GL_NEAREST)))

(defn tex-sub-image [^GL2 gl ^TextureLifecycle texture data x y w h data-format]
  (let [tex (->texture texture gl)
        data (->texture-data w h data-format data false)]
    (.updateSubImage tex gl data 0 x y)))

(def default-cubemap-texture-params
  ^{:doc "If you do not supply parameters to `cubemap-texture-datas->gpu-texture`, these will be used as defaults."}
  {:min-filter gl/linear
   :mag-filter gl/linear
   :wrap-s     gl/clamp-to-edge
   :wrap-t     gl/clamp-to-edge})

(defn cubemap-texture-datas->gpu-texture
  ([request-id texture-datas]
   (cubemap-texture-datas->gpu-texture request-id texture-datas default-cubemap-texture-params))
  ([request-id texture-datas params]
   (cubemap-texture-datas->gpu-texture request-id texture-datas params 0))
  ([request-id texture-datas params unit-index]
   (let [unit (+ unit-index GL2/GL_TEXTURE0)]
     (->TextureLifecycle request-id ::cubemap-texture unit params texture-datas))))

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


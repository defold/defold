(ns dynamo.gl.texture
  (:refer-clojure :exclude [repeat])
  (:require [dynamo.image :as img :refer [placeholder-image]]
            [dynamo.gl.protocols :refer :all]
            [dynamo.gl :as gl]
            [dynamo.types :refer [IDisposable dispose]])
  (:import [java.awt.image BufferedImage]
           [java.nio IntBuffer]
           [javax.media.opengl GL GL2 GLContext GLProfile]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl.util.texture Texture TextureIO]
           [com.jogamp.opengl.util.texture.awt AWTTextureIO]))

(def texture-params
  {:base-level   GL2/GL_TEXTURE_BASE_LEVEL
   :border-color GL2/GL_TEXTURE_BORDER_COLOR
   :compare-func GL2/GL_TEXTURE_COMPARE_FUNC
   :compare-mode GL2/GL_TEXTURE_COMPARE_MODE
   :lod-bias     GL2/GL_TEXTURE_LOD_BIAS
   :min-filter   GL2/GL_TEXTURE_MIN_FILTER
   :mag-filter   GL2/GL_TEXTURE_MAG_FILTER
   :min-lod      GL2/GL_TEXTURE_MIN_LOD
   :max-lod      GL2/GL_TEXTURE_MAX_LOD
   :max-level    GL2/GL_TEXTURE_MAX_LEVEL
   :swizzle-r    GL2/GL_TEXTURE_SWIZZLE_R
   :swizzle-g    GL2/GL_TEXTURE_SWIZZLE_G
   :swizzle-b    GL2/GL_TEXTURE_SWIZZLE_B
   :swizzle-a    GL2/GL_TEXTURE_SWIZZLE_A
   :wrap-s       GL2/GL_TEXTURE_WRAP_S
   :wrap-t       GL2/GL_TEXTURE_WRAP_T
   :wrap-r       GL2/GL_TEXTURE_WRAP_R})

(def red                    GL2/GL_RED)
(def green                  GL2/GL_GREEN)
(def blue                   GL2/GL_BLUE)
(def alpha                  GL2/GL_ALPHA)
(def zero                   GL2/GL_ZERO)
(def one                    GL2/GL_ONE)
(def lequal                 GL2/GL_LEQUAL)
(def gequal                 GL2/GL_GEQUAL)
(def less                   GL2/GL_LESS)
(def greater                GL2/GL_GREATER)
(def equal                  GL2/GL_EQUAL)
(def notequal               GL2/GL_NOTEQUAL)
(def always                 GL2/GL_ALWAYS)
(def never                  GL2/GL_NEVER)
(def clamp_to_edge          GL2/GL_CLAMP_TO_EDGE)
(def clamp_to_border        GL2/GL_CLAMP_TO_BORDER)
(def mirrored_repeat        GL2/GL_MIRRORED_REPEAT)
(def repeat                 GL2/GL_REPEAT)
(def clamp                  GL2/GL_CLAMP)
(def compare-ref-to-texture GL2/GL_COMPARE_REF_TO_TEXTURE)
(def none                   GL2/GL_NONE)
(def nearest                GL2/GL_NEAREST)
(def linear                 GL2/GL_LINEAR)
(def nearest-mipmap-nearest GL2/GL_NEAREST_MIPMAP_NEAREST)
(def linear-mipmap-nearest  GL2/GL_LINEAR_MIPMAP_NEAREST)
(def nearest-mipmap-linear  GL2/GL_NEAREST_MIPMAP_LINEAR)
(def linear-mipmap-linear   GL2/GL_LINEAR_MIPMAP_LINEAR)

(defonce gl-texture-state (atom {}))

(defn context-local-data
  [gl key]
  (get-in @gl-texture-state [gl key]))

(defn set-context-local-data
  [gl key data]
  (swap! gl-texture-state assoc-in [gl key] data))

(defn context-local-data-forget
  [gl key]
  (swap! gl-texture-state dissoc [gl key]))

(defn texture-occurrences
  [tex]
  (for [[gl asgns]    (dissoc @gl-texture-state :local)
        [u t :as asgn] asgns
        :when (= t tex)]
    [gl asgn]))

(defn unload-texture
  [this]
  (swap! gl-texture-state
    #(reduce-kv
       (fn [state gl-context tex]
         (if (identical? tex this)
           (dissoc state gl-context)
           state))
       % %)))

(defn unload-all
  [gl]
  (swap! gl-texture-state dissoc gl))

(defn- apply-params
  [gl ^Texture texture params]
  (doseq [[p v] params]
    (if-let [pname (texture-params p)]
      (.setTexParameteri texture gl (texture-params p) v)
      (println "WARNING: ignoring unknown texture parameter " p))))

(defrecord TextureLifecycle [unit params ^BufferedImage img]
  GlBind
  (bind [this gl]
    (when-not (context-local-data gl this)
      (.glActiveTexture ^GL2 gl unit)
      (let [texture ^Texture (AWTTextureIO/newTexture (GLProfile/getGL2GL3) img true)]
        (apply-params gl texture params)
        (.enable texture gl)
        (.bind texture gl)
        (set-context-local-data gl this texture))))

  (unbind [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (.destroy texture gl)
      (context-local-data-forget gl this)))

  GlEnable
  (enable [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (gl/gl-active-texture ^GL gl unit)
      (.bind texture gl)
      (.enable texture gl)))

  (disable [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (.disable texture gl))
    (gl/gl-active-texture ^GL gl GL/GL_TEXTURE0))

  IDisposable
  (dispose [this]
    (println "TextureLifecycle.dispose " img)
    (unload-texture this)))

(def default-image-texture-params
  {:min-filter linear-mipmap-linear
   :mag-filter linear
   :wrap-s     clamp
   :wrap-t     clamp})

(defn image-texture
  ([img]
   (image-texture GL/GL_TEXTURE0 default-image-texture-params img))
  ([img params]
   (image-texture GL/GL_TEXTURE0 params img))
  ([unit params ^BufferedImage img]
    (->TextureLifecycle unit params (or img (:contents placeholder-image)))))

(defrecord CubemapTexture [unit params ^BufferedImage right ^BufferedImage left ^BufferedImage top ^BufferedImage bottom ^BufferedImage front ^BufferedImage back]
  GlBind
  (bind [this gl]
    (when-not (context-local-data gl this)
      (let [texture ^Texture (TextureIO/newTexture GL/GL_TEXTURE_CUBE_MAP)]
        (doseq [[img target]
                [[right  GL/GL_TEXTURE_CUBE_MAP_POSITIVE_X]
                 [left   GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_X]
                 [top    GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Y]
                 [bottom GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Y]
                 [front  GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Z]
                 [back   GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Z]]]
          (.updateImage texture gl ^TextureData (AWTTextureIO/newTextureData (GLProfile/getGL2GL3) img false) target))
        (apply-params gl texture params)
        (.enable texture gl)
        (.bind texture gl)
        (set-context-local-data gl this texture))))

  (unbind [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (.destroy texture gl)
      (context-local-data-forget gl this)))

  GlEnable
  (enable [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (gl/gl-active-texture ^GL gl unit)
      (.bind texture gl)
      (.enable texture gl)))

  (disable [this gl]
    (when-let [texture ^Texture (context-local-data gl this)]
      (.disable texture gl))
    (gl/gl-active-texture ^GL gl GL/GL_TEXTURE0))

  IDisposable
  (dispose [this]
    (println "CubemapTexture.dispose")
    (unload-texture this)))

(def default-cubemap-texture-params
  {:min-filter linear
   :mag-filter linear
   :wrap-s     clamp
   :wrap-t     clamp})

(def cubemap-placeholder
  (memoize
    (fn []
      (img/flood (img/blank-image 512 512 BufferedImage/TYPE_3BYTE_BGR) 0.9568 0.0 0.6313))))

(defn- safe-texture
  [x]
  (if (nil? x)
    (cubemap-placeholder)
    (if (= (:contents img/placeholder-image) x)
      (cubemap-placeholder)
      x)))

(defn image-cubemap-texture
  ([right left top bottom front back]
   (image-cubemap-texture GL/GL_TEXTURE0 default-cubemap-texture-params right left top bottom front back))
  ([params right left top bottom front back]
   (image-cubemap-texture GL/GL_TEXTURE0 params right left top bottom front back))
  ([unit params right left top bottom front back]
   (apply ->CubemapTexture unit params (map #(safe-texture %) [right left top bottom front back]))))

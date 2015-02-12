(ns dynamo.gl.texture
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

(set! *warn-on-reflection* true)

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

(defrecord TextureLifecycle [unit ^BufferedImage img]
  GlBind
  (bind [this gl]
    (when-not (context-local-data gl this)
      (.glActiveTexture ^GL2 gl unit)
      (let [texture ^Texture (AWTTextureIO/newTexture (GLProfile/getGL2GL3) img true)]
        (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR_MIPMAP_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
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

(defn image-texture
  ([img]
    (image-texture GL/GL_TEXTURE0 img))
  ([unit ^BufferedImage img]
    (->TextureLifecycle unit (or img (:contents placeholder-image)))))

(defrecord CubemapTexture [unit ^BufferedImage right ^BufferedImage left ^BufferedImage top ^BufferedImage bottom ^BufferedImage front ^BufferedImage back]
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
        (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
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
    (image-cubemap-texture GL/GL_TEXTURE0 right left top bottom front back))
  ([unit right left top bottom front back]
    (apply ->CubemapTexture unit (map #(safe-texture %) [right left top bottom front back]))))

(ns dynamo.gl.texture
  (:require [dynamo.types :refer [IDisposable dispose]]
            [dynamo.gl.protocols :refer :all]
            [dynamo.gl :as gl])
  (:import [java.awt.image BufferedImage]
           [java.nio IntBuffer]
           [javax.media.opengl GL GL2 GLAutoDrawable GLContext GLEventListener GLProfile]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl.util.texture Texture TextureIO]
           [com.jogamp.opengl.util.texture.awt AWTTextureIO]))

(set! *warn-on-reflection* true)

(defprotocol TextureUnit
  (texture-unit-index [this] "Index from zero.")
  (texture-unit-id    [this] "Equivalent to (+ GL_TEXTURE0 texture-unit-index)"))

(defrecord TextureLifecycle [^BufferedImage img context-local-data]
  GlBind
  (bind [this gl]
    (when-not (get @context-local-data gl)
      (let [texture ^Texture (AWTTextureIO/newTexture (GLProfile/getGL2GL3) img true)
            drawable (.getGLDrawable ^GLContext (GLContext/getCurrent))]
        ;; TODO - don't assume GL_TEXTURE0 every time!
        (.glActiveTexture ^GL2 gl GL/GL_TEXTURE0)
        (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR_MIPMAP_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
        (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
        (.enable texture gl)
        (.bind texture gl)
        (when (instance? GLAutoDrawable drawable)
          (.addGLEventListener ^GLAutoDrawable drawable
            (reify GLEventListener
              (init    [this drawable])
              (display [this drawable])
              (dispose [this drawable]
                (unbind this gl))
              (reshape [this drawable x y width height]))))
        (swap! context-local-data assoc gl texture))))

  (unbind [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.destroy texture gl)
      (swap! context-local-data dissoc gl)))

  GlEnable
  (enable [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.enable texture gl)))

  (disable [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.disable texture gl)))

  TextureUnit
  (texture-unit-index [this] (int 0))
  (texture-unit-id    [this] (+ GL/GL_TEXTURE0 (texture-unit-index this))))

(defn image-texture
  [^BufferedImage img]
  (->TextureLifecycle img (atom {})))

(defrecord CubemapTexture [^BufferedImage right ^BufferedImage left ^BufferedImage top ^BufferedImage bottom ^BufferedImage front ^BufferedImage back context-local-data]
  GlBind
  (bind [this gl]
    (when-not (get @context-local-data gl)
      (let [texture ^Texture (TextureIO/newTexture GL/GL_TEXTURE_CUBE_MAP)
            drawable (.getGLDrawable ^GLContext (GLContext/getCurrent))]
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
        (when (instance? GLAutoDrawable drawable)
          (.addGLEventListener ^GLAutoDrawable drawable
            (reify GLEventListener
              (init    [this drawable])
              (display [this drawable])
              (dispose [this drawable]
                (unbind this gl))
              (reshape [this drawable x y width height]))))
        (swap! context-local-data assoc gl texture))))

  (unbind [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.destroy texture gl)
      (swap! context-local-data dissoc gl)))

  GlEnable
  (enable [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.enable texture gl)))

  (disable [this gl]
    (when-let [texture ^Texture (get @context-local-data gl)]
      (.disable texture gl)))

  TextureUnit
  (texture-unit-index [this] (int 0))
  (texture-unit-id    [this] (+ GL/GL_TEXTURE0 (texture-unit-index this))))


(defn image-cubemap-texture
  [right left top bottom front back]
  (->CubemapTexture right left top bottom front back (atom {})))

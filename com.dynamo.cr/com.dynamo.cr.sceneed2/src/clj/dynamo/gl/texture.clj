(ns dynamo.gl.texture
  (:require [dynamo.resource :refer [IDisposable dispose]]
            [dynamo.gl.protocols :refer :all]
            [dynamo.gl.shader :refer [shader-program]])
  (:import [java.awt.image BufferedImage]
           [java.nio IntBuffer]
           [javax.media.opengl GL GL2 GLProfile]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl.util.texture Texture TextureIO]
           [com.jogamp.opengl.util.texture.awt AWTTextureIO]))

(set! *warn-on-reflection* true)

(defprotocol TextureBounds
  (texture-width [this])
  (texture-height [this]))

(defprotocol TextureUnit
  (texture-unit-index [this] "Index from zero.")
  (texture-unit-id    [this] "Equivalent to (+ GL_TEXTURE0 texture-unit-index)"))

;; TODO - don't assume GL_TEXTURE0 every time!
(defn image-texture
  [^GL2 gl ^BufferedImage img]
  (let [texture (AWTTextureIO/newTexture (GLProfile/getGL2GL3) img true)]
    (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR_MIPMAP_LINEAR)
    (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
    (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
    (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
    (reify
      GlEnable
      (enable [this]
        (.glActiveTexture gl GL/GL_TEXTURE0)
        (.enable texture gl)
        (.bind texture gl))

      GlDisable
      (disable [this]
        (.disable texture gl))

      IDisposable
      (dispose [this]
        (.destroy texture gl))

      TextureUnit
      (texture-unit-index [this] (int 0))
      (texture-unit-id    [this] (+ GL/GL_TEXTURE0 (texture-unit-index this)))

      TextureBounds
      (texture-width [this] (.getWidth texture))
      (texture-height [this] (.getHeight texture)))))

(defn image-cubemap-texture
  [^GL2 gl ^BufferedImage right ^BufferedImage left ^BufferedImage top ^BufferedImage bottom ^BufferedImage front ^BufferedImage back]
  (let [texture ^Texture (TextureIO/newTexture GL/GL_TEXTURE_CUBE_MAP)]
    (doseq [[img target]
            [[right GL/GL_TEXTURE_CUBE_MAP_POSITIVE_X]
             [left GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_X]
             [top GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Y]
             [bottom GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Y]
             [front GL/GL_TEXTURE_CUBE_MAP_POSITIVE_Z]
             [back GL/GL_TEXTURE_CUBE_MAP_NEGATIVE_Z]]]
      (.updateImage texture gl ^TextureData (AWTTextureIO/newTextureData (GLProfile/getGL2GL3) img false) target))
    (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR)
    (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
    (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
    (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
    (reify
      GlEnable
      (enable [this]
        (.glActiveTexture gl GL/GL_TEXTURE0)
        (.enable texture gl)
        (.bind texture gl))

      GlDisable
      (disable [this]
        (.disable texture gl))

      IDisposable
      (dispose [this]
        (.destroy texture gl))

      TextureUnit
      (texture-unit-index [this] (int 0))
      (texture-unit-id    [this] (+ GL/GL_TEXTURE0 (texture-unit-index this)))

      TextureBounds
      (texture-width [this] (.getWidth texture))
      (texture-height [this] (.getHeight texture)))))
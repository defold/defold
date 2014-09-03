(ns dynamo.gl.texture
  (:require [dynamo.resource :refer [IDisposable dispose]]
            [dynamo.gl.protocols :refer :all]
            [dynamo.gl.shader :refer [shader-program]])
  (:import [java.awt.image BufferedImage]
           [java.nio IntBuffer]
           [javax.media.opengl GL GL2 GLProfile]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl.util.texture Texture]
           [com.jogamp.opengl.util.texture.awt AWTTextureIO]))

(set! *warn-on-reflection* true)

(defprotocol TextureBounds
  (texture-width [this])
  (texture-height [this]))

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

      TextureBounds
      (texture-width [this] (.getWidth texture))
      (texture-height [this] (.getHeight texture)))))
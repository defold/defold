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

;;; This is where we keep our idea of the GPU's state
;;;
;;; The structure is a map from gl-context -> [ TextureUnit TextureLifecycle ]
(defonce gl-texture-state (atom {}))

(defn texture-unit-id
  "Equivalent to (+ GL_TEXTURE0 texture-unit-index)"
  [index]
  (when index
    (int (+ GL/GL_TEXTURE0 index))))

(defn texture-unit-assign
  [gl unit tex]
  (swap! gl-texture-state assoc-in [gl unit] tex))

(defn texture-unit-unassign
  [gl unit tex]
  (swap! gl-texture-state dissoc gl unit))

(defn texture-unit-forget
  [gl]
  (swap! gl-texture-state dissoc gl))

(defn texture-unit-assignments
  [gl]
  (get @gl-texture-state gl))

(defn texture-unit-assignment
  [gl unit]
  (first (filter #(= unit (key %)) (texture-unit-assignments gl))))

(defn texture-loaded?
  [gl tex]
  (ffirst (filter #(= tex (val %)) (texture-unit-assignments gl))))

(def available? (comp nil? second))

(defn first-available-texture-unit
  [gl]
  (ffirst (filter available? (texture-unit-assignments gl))))

(defn unload-texture-from-unit
  [gl [unit tex]]
  (when tex
    (unbind tex gl)
    (texture-unit-unassign gl unit tex)))

(defn load-texture-in-unit
  [gl unit tex]
  (if (not (available? (texture-unit-assignment gl unit)))
    (unload-texture-from-unit (texture-unit-assignment gl unit)))
  (.glActiveTexture ^GL2 gl (texture-unit-id unit))
  (texture-unit-assign gl unit tex))

(defn texture-unit-index
  [gl tex]
  (texture-loaded? gl tex))

(defn context-local-data
  [gl key]
  (get-in @gl-texture-state [:local gl key]))

(defn set-context-local-data
  [gl key data]
  (swap! gl-texture-state assoc-in [:local gl key] data))

(defn context-local-data-forget
  [gl]
  (swap! gl-texture-state update-in [:local] dissoc gl))

(defn texture-occurrences
  [tex]
  (for [[gl asgns]    (dissoc @gl-texture-state :local)
        [u t :as asgn] asgns
        :when (= t tex)]
    [gl asgn]))

(defn unload-texture
  [tex]
  (doall (map unload-texture-from-unit (texture-occurrences tex))))

(defn unload-all
  [gl]
  (doseq [a (texture-unit-assignments gl)]
    (unload-texture-from-unit gl a))
  (texture-unit-forget gl)
  (context-local-data-forget gl))

(defn initialize
  [gl]
  (let [units (zipmap (map int (range (gl/gl-max-texture-units gl))) (repeat nil))]
    (println "max texture units: " (count units))
    (swap! gl-texture-state assoc gl units)))

(defrecord TextureLifecycle [^BufferedImage img]
  GlBind
  (bind [this gl]
    (when-not (texture-loaded? gl this)
      (let [texture-unit (first-available-texture-unit gl)]
        (load-texture-in-unit gl texture-unit this)
        (let [texture ^Texture (AWTTextureIO/newTexture (GLProfile/getGL2GL3) img true)]
          (.setTexParameteri texture gl GL/GL_TEXTURE_MIN_FILTER GL/GL_LINEAR_MIPMAP_LINEAR)
          (.setTexParameteri texture gl GL/GL_TEXTURE_MAG_FILTER GL/GL_LINEAR)
          (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_S GL2/GL_CLAMP)
          (.setTexParameteri texture gl GL/GL_TEXTURE_WRAP_T GL2/GL_CLAMP)
          (.enable texture gl)
          (.bind texture gl)
          (set-context-local-data gl this texture)))))

  (unbind [this gl]
    (when (texture-loaded? gl this)
      (when-let [texture ^Texture (context-local-data gl this)]
        (.destroy texture gl)
        (set-context-local-data gl this nil))))

  GlEnable
  (enable [this gl]
    (when-let [unit (texture-loaded? gl this)]
      (gl/gl-active-texture ^GL gl (texture-unit-id unit))
      (when-let [texture ^Texture (context-local-data gl this)]
        (.bind texture gl)
        (.enable texture gl))))

  (disable [this gl]
    (when (texture-loaded? gl this)
      (when-let [texture ^Texture (context-local-data gl this)]
        (.disable texture gl))
      (gl/gl-active-texture ^GL gl GL/GL_TEXTURE0)))

  IDisposable
  (dispose [this]
    (unload-texture this)))

(defn image-texture
  [^BufferedImage img]
  (->TextureLifecycle (or img (:contents placeholder-image))))

(defrecord CubemapTexture [^BufferedImage right ^BufferedImage left ^BufferedImage top ^BufferedImage bottom ^BufferedImage front ^BufferedImage back]
  GlBind
  (bind [this gl]
    (when-not (texture-loaded? gl this)
      (let [texture-unit (first-available-texture-unit gl)]
        (load-texture-in-unit gl texture-unit this)
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
          (set-context-local-data gl this texture)))))

  (unbind [this gl]
    (when (texture-loaded? gl this)
      (when-let [texture ^Texture (context-local-data gl this)]
        (.destroy texture gl)
        (set-context-local-data gl this nil))))

  GlEnable
  (enable [this gl]
    (when-let [unit (texture-loaded? gl this)]
      (gl/gl-active-texture ^GL gl (texture-unit-id unit))
      (when-let [texture ^Texture (context-local-data gl this)]
        (.bind texture gl)
        (.enable texture gl))))

  (disable [this gl]
    (when (texture-loaded? gl this)
      (when-let [texture ^Texture (context-local-data gl this)]
        (.disable texture gl))
      (gl/gl-active-texture ^GL gl GL/GL_TEXTURE0)))

  IDisposable
  (dispose [this]
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
  [right left top bottom front back]
  (apply ->CubemapTexture (map #(safe-texture %) [right left top bottom front back])))

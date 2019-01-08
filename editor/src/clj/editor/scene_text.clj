(ns editor.scene-text
  (:require [editor.gl :as gl]
            [editor.scene-cache :as scene-cache])
  (:import [com.jogamp.opengl.util.awt TextRenderer]
           [java.awt Font]
           [com.jogamp.opengl GL2]))

(set! *warn-on-reflection* true)

(defn- request-text-renderer! ^TextRenderer [^GL2 gl]
  (scene-cache/request-object! ::text-renderer ::overlay-text gl [Font/SANS_SERIF Font/PLAIN 9]))

(defn overlay
  ([^GL2 gl ^String text x y]
    (overlay gl text x y 1 1 1 1))
  ([^GL2 gl ^String text x y r g b a]
   (overlay gl text x y r g b a 0.0))
  ([^GL2 gl ^String text x y r g b a rot-z]
    (let [^TextRenderer text-renderer (request-text-renderer! gl)]
      (gl/overlay gl text-renderer text x y r g b a rot-z))))

(defn bounds [^GL2 gl ^String text]
  (let [^TextRenderer text-renderer (request-text-renderer! gl)]
      (let [bounds (.getBounds text-renderer text)]
        [(.getWidth bounds) (.getHeight bounds)])))

(defn- make-text-renderer [^GL2 gl data]
  (let [[font-family font-style font-size] data]
    (gl/text-renderer font-family font-style font-size)))

(defn- destroy-text-renderers [^GL2 gl text-renderers _]
  (doseq [^TextRenderer text-renderer text-renderers]
    (.dispose text-renderer)))

(defn- update-text-renderer [^GL2 gl text-renderer data]
  (destroy-text-renderers gl [text-renderer] nil)
  (make-text-renderer gl data))

(scene-cache/register-object-cache! ::text-renderer make-text-renderer update-text-renderer destroy-text-renderers)

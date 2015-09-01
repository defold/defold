(ns editor.jfx
  (:require [clojure.java.io :as io])
  (:import [javafx.scene.image Image ImageView]
           [javafx.stage FileChooser FileChooser$ExtensionFilter]
           [java.io File]
           [java.util ArrayList]))


; Image cache
(defonce cached-images (atom {}))
(defn- load-image [name]
  (if-let [url (io/resource (str name))]
    (Image. (str url))
    nil))

(defn get-image [name]
  (if-let [image (get @cached-images name)]
    image
    (let [image (load-image name)]
      ((swap! cached-images assoc name image) name))))

(defn get-image-view
  ([name]
    (ImageView. ^Image (get-image name)))
  ([name size]
    (let [iv (ImageView. ^Image (get-image name))]
      (.setFitWidth iv size)
      (.setFitHeight iv size)
      iv)))


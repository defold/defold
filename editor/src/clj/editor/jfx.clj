(ns editor.jfx
  (:require [clojure.java.io :as io])
  (:import  [java.io File]
            [java.util ArrayList]
            [javafx.stage FileChooser FileChooser$ExtensionFilter]
            [javafx.scene.image Image ImageView]))

; ImageView cache
(defonce cached-image-views (atom {}))
(defn- load-image-view [name]
  (if-let [url (io/resource (str name))]
    (ImageView. (Image. (str url)))
    (ImageView.)))

(defn get-image-view [name]
  (if-let [image-view (:name @cached-image-views)]
    image-view
    (let [image-view (load-image-view name)]
      ((swap! cached-image-views assoc name image-view) name))))

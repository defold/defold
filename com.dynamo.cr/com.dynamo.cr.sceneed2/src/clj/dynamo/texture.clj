(ns dynamo.texture
  (:require [dynamo.types :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]))

;; Value
(def TextureImage
  {:path     s/Str
   :contents bytes})

;; Transform produces value
(defnk image-from-resource :- TextureImage
  [this]
  {:path (:resource this)
   :contents (byte-array [1 2 3 4 5])})

;; Behavior
(def Image
  {:properties {:resource (resource)}
   :transforms {:image #'image-from-resource}
   :cached     #{:image}})

(defn animation-frames [])

(def Animation
  {:inputs     {:images (as-schema [Image])}
   :properties {:fps (non-negative-integer :default 30)}
   :transforms {:frames #'animation-frames}})

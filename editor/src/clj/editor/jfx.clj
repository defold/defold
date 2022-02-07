;; Copyright 2020 The Defold Foundation
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.jfx
  (:require [clojure.java.io :as io])
  (:import [javafx.scene.image Image ImageView]))

(set! *warn-on-reflection* true)

;; Image cache
(defonce cached-images (atom {}))

(defn get-image
  ([name]
   (get-image name nil))
  ([name ^Double size]
   (let [image-key    [name size]
         url          (io/resource (str name))
         cached-image (get @cached-images image-key)]
     (cond
       cached-image cached-image
       url          (let [image (if size
                                  (Image. (str url) size size true true)
                                  (Image. (str url)))]
                        (swap! cached-images assoc image-key image)
                        image)))))

(defn get-image-view
  ([name]
   (ImageView. ^Image (get-image name)))
  ([name size]
   (let [iv (ImageView. ^Image (get-image name))]
     (.setFitWidth iv size)
     (.setFitHeight iv size)
     iv)))

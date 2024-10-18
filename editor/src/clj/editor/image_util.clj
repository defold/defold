;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

(ns editor.image-util
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.geom :refer [clamper]]
            [editor.types :as types]
            [editor.pipeline.tex-gen :as tex-gen]
            [service.log :as log])
  (:import [editor.types Rect Image]
           [java.awt Color]
           [java.awt.image BufferedImage]
           [javax.imageio ImageIO]))

(set! *warn-on-reflection* true)

(defn- convert-to-abgr
  [^BufferedImage image]
  (let [type (.getType image)]
    (if (= type BufferedImage/TYPE_4BYTE_ABGR)
      image
      (let [abgr-image (BufferedImage. (.getWidth image) (.getHeight image) BufferedImage/TYPE_4BYTE_ABGR)]
        (doto (.createGraphics abgr-image)
          (.drawImage image 0 0 nil)
          (.dispose))
        abgr-image))))

(defn read-image
  ^BufferedImage [source]
  (with-open [source-stream (io/input-stream source)]
    (convert-to-abgr (ImageIO/read source-stream))))

(defn read-size
  [source]
  (with-open [source-stream (io/input-stream source)
              image-stream (ImageIO/createImageInputStream source-stream)]
    (let [readers (ImageIO/getImageReaders image-stream)]
      (if (.hasNext readers)
        (let [^javax.imageio.ImageReader reader (.next readers)]
          (try
            (.setInput reader image-stream true true)
            {:width (.getWidth reader 0)
             :height (.getHeight reader 0)}
            (finally
              (.dispose reader))))
        (throw (ex-info "No matching ImageReader" {}))))))

(defmacro with-graphics
  [binding & body]
  (let [rsym (gensym)]
    `(let ~(into [] (concat binding [rsym `(do ~@body)]))
       (.dispose ~(first binding))
       ~rsym)))

(s/defn make-color :- java.awt.Color
  "creates a color using rgb values (optional a). Color values between 0 and 1.0"
  ([r :- Float g :- Float b :- Float]
    (java.awt.Color. r g b))
  ([r :- Float g :- Float b :- Float a :- Float]
    (java.awt.Color. r g b a)))

(s/defn make-image :- Image
  [nm :- s/Any contents :- BufferedImage]
  (Image. nm contents (.getWidth contents) (.getHeight contents) 0.5 0.5 :sprite-trim-mode-off))

(s/defn blank-image :- BufferedImage
  ([space :- Rect]
    (blank-image (.width space) (.height space)))
  ([width :- s/Int height :- s/Int]
    (blank-image width height BufferedImage/TYPE_4BYTE_ABGR))
  ([width :- s/Int height :- s/Int t :- s/Int]
    (BufferedImage. width height t)))

(s/defn flood :- BufferedImage
  "Floods the image with the specified color (r g b <a>). Color values between 0 and 1.0."
  [^BufferedImage img :- BufferedImage r :- Float g :- Float b :- Float]
  (let [gfx (.createGraphics img)
        color (make-color r g b)]
    (.setColor gfx color)
    (.fillRect gfx 0 0 (.getWidth img) (.getHeight img))
    (.dispose gfx)
    img))

(defn load-image [src reference]
  (make-image reference (read-image src)))

;; Use "Hollywood Cerise" for the placeholder image color.
(def placeholder-image (make-image "placeholder" (flood (blank-image 64 64) 0.9568 0.0 0.6313)))

(s/defn image-color-components :- long
  [src :- BufferedImage]
  (.. src (getColorModel) (getNumComponents)))

(s/defn image-infer-type :- long
  [src :- BufferedImage]
  (if (not= 0 (.getType src))
    (.getType src)
    (case (image-color-components src)
      4 BufferedImage/TYPE_4BYTE_ABGR
      3 BufferedImage/TYPE_3BYTE_BGR
      1 BufferedImage/TYPE_BYTE_GRAY)))

(s/defn image-type :- g/Int
  [src :- BufferedImage]
  (let [t (.getType src)]
    (if (not= 0 t) t (image-infer-type src))))

(s/defn image-convert-type :- BufferedImage
  [original :- BufferedImage new-type :- g/Int]
  (if (= new-type (image-type original))
    original
    (let [new (blank-image (.getWidth original) (.getHeight original) new-type)]
      (with-graphics [g2d (.createGraphics new)]
        (.drawImage g2d original 0 0 nil))
      new)))

(s/defn image-bounds :- Rect
  [source :- Image]
  (types/rect (.path source) 0 0 (.width source) (.height source)))

(s/defn image-pixels :- ints
  [src :- BufferedImage]
  (let [w      (.getWidth src)
        h      (.getHeight src)
        pixels (int-array (* w h (image-color-components src)))]
    (.. src (getRaster) (getPixels 0 0 w h pixels))
    pixels))

(s/defn image-from-pixels :- BufferedImage
  [^long width :- g/Int ^long height :- g/Int t :- g/Int pixels :- ints]
  (doto (blank-image width height t)
    (.. (getRaster) (setPixels 0 0 width height pixels))))

(defmacro pixel-index [x y step stride]
  `(* ~step (+ ~x (* ~y ~stride))))

(s/defn extrude-borders :- Image
  "Return a new pixel array, larger than the original by `extrusion`
with the orig-pixels centered in it. The source pixels on the edges
will bleed into the surrounding empty space. The pixels in the border
region will be identical to the nearest pixel of the source image."
  [extrusion :- g/Int src :- Image]
  (if-not (< 0 extrusion)
    src
    (let [src-img        (types/contents src)
          src-img        (image-convert-type src-img BufferedImage/TYPE_4BYTE_ABGR)
          orig-width     (.width src)
          orig-height    (.height src)
          new-width      (+ orig-width (* 2 extrusion))
          new-height     (+ orig-height (* 2 extrusion))
          src-pixels     (image-pixels src-img)
          num-components (image-color-components src-img)
          clampx         (clamper 0 (dec orig-width))
          clampy         (clamper 0 (dec orig-height))
          new-pixels     (int-array (* new-width new-height num-components))]
      (doseq [y (range new-height)
              x (range new-width)]
        (let [sx (clampx (- x extrusion))
              sy (clampy (- y extrusion))
              src-idx (pixel-index sx sy num-components orig-width)
              tgt-idx (pixel-index x   y num-components new-width)]
          (doseq [i (range num-components)]
            (aset-int new-pixels (+ i tgt-idx) (aget src-pixels (+ i src-idx))))))
      (make-image (.path src) (image-from-pixels new-width new-height (.getType src-img) new-pixels)))))

(defn- map-by [p coll]
  (zipmap (map p coll) coll))

(s/defn composite :- BufferedImage
  [onto :- BufferedImage placements :- [Rect] sources :- [Image]]
  (let [src-by-path (map-by :path sources)]
    (with-graphics [graphics (.getGraphics onto)]
      (doseq [^Rect rect placements]
        (.drawImage graphics (:contents (get src-by-path (.path rect))) (int (.x rect)) (int (.y rect)) nil)))
    onto))

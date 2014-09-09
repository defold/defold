(ns dynamo.image
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]]
            [dynamo.types :refer :all]
            [dynamo.geom :refer :all]
            [dynamo.node :refer [defnode]]
            [dynamo.file :refer [project-path local-path]]
            [internal.cache :refer [caching]])
  (:import [javax.imageio ImageIO]
           [java.awt.image BufferedImage]
           [dynamo.types Rect Image]))

(defmacro with-graphics
  [binding & body]
  (let [rsym (gensym)]
    `(let ~(into [] (concat binding [rsym `(do ~@body)]))
       (.dispose ~(first binding))
       ~rsym)))

(sm/defn make-image :- Image
  [nm :- s/Any contents :- BufferedImage]
  (Image. nm contents (.getWidth contents) (.getHeight contents)))

(def load-image
  (caching
    (fn [src]
      (if-let [img (ImageIO/read (io/input-stream src))]
        (make-image src img)))))

;; Transform produces value
;; TODO - return placeholder image when the named image is not found
(defnk image-from-resource :- Image
  [this project]
  (load-image (project-path project (:image this))))

;; Behavior
(defnode ImageSource
  (property image (resource))
  (output   image Image :cached image-from-resource))

(sm/defn blank-image :- BufferedImage
  ([space :- Rect]
    (blank-image (.width space) (.height space)))
  ([width :- s/Int height :- s/Int]
    (blank-image width height BufferedImage/TYPE_4BYTE_ABGR))
  ([width :- s/Int height :- s/Int t :- s/Int]
    (BufferedImage. width height t)))

(sm/defn image-color-components :- s/Int
  [src :- BufferedImage]
  (.. src (getColorModel) (getNumComponents)))

(sm/defn image-infer-type :- s/Int
  [src :- BufferedImage]
  (if (not= 0 (.getType src))
    (.getType src)
    (case (image-color-components src)
      4 BufferedImage/TYPE_4BYTE_ABGR
      3 BufferedImage/TYPE_3BYTE_BGR
      1 BufferedImage/TYPE_BYTE_GRAY)))

(sm/defn image-type :- s/Int
  [src :- BufferedImage]
  (let [t (.getType src)]
    (if (not= 0 t) t (image-infer-type src))))

(sm/defn image-convert-type :- BufferedImage
  [original :- BufferedImage new-type :- s/Int]
  (if (= new-type (image-type original))
    original
    (let [new (blank-image (.getWidth original) (.getHeight original) new-type)]
      (with-graphics [g2d (.createGraphics new)]
        (.drawImage g2d original 0 0 nil)))))

(sm/defn image-bounds :- Rect
  [source :- Image]
  (rect (.path source) 0 0 (.width source) (.height source)))

(sm/defn image-pixels :- ints
  [src :- BufferedImage]
  (let [w      (.getWidth src)
        h      (.getHeight src)
        pixels (int-array (* w h (image-color-components src)))]
    (.. src (getRaster) (getPixels 0 0 w h pixels))
    pixels))

(sm/defn image-from-pixels :- BufferedImage
  [width :- s/Int height :- s/Int t :- s/Int pixels :- ints]
  (doto (blank-image width height t)
    (.. (getRaster) (setPixels 0 0 width height pixels))))

(defmacro pixel-index [x y step stride]
  `(* ~step (+ ~x (* ~y ~stride))))

(sm/defn extrude-borders :- Image
  [extrusion :- s/Int src :- Image]
  (if-not (< 0 extrusion)
    src
    (let [src-img        (.contents src)
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

(sm/defn composite :- BufferedImage
  [onto :- BufferedImage placements :- [Rect] sources :- [Image]]
  (let [src-by-path (map-by :path sources)]
    (with-graphics [graphics (.getGraphics onto)]
      (doseq [rect placements]
        (.drawImage graphics (:contents (get src-by-path (.path rect))) (int (.x rect)) (int (.y rect)) nil)))
    onto))

(doseq [[v doc]
       {#'extrude-borders
        "Return a new pixel array, larger than the original by `extrusion`
with the orig-pixels centered in it. The source pixels on the edges
will bleed into the surrounding empty space. The pixels in the border
region will be identical to the nearest pixel of the source image."

        #'image-from-resource
        "Returns `{:path path :contents byte-array}` from an image resource."
        }]
  (alter-meta! v assoc :doc doc))

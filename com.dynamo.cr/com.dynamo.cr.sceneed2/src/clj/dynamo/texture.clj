(ns dynamo.texture
  (:require [clojure.java.io :as io]
            [dynamo.types :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [internal.cache :refer [make-cache caching]]
            [plumbing.core :refer [defnk]])
  (:import [javax.imageio ImageIO]
           [java.awt.image BufferedImage]))

(def ^:dynamic *texture-pack-debug* false)

;; Value
(sm/defrecord Image
  [path     :- s/Str
   contents :- BufferedImage
   width    :- s/Int
   height   :- s/Int])

(sm/defrecord Rect
  [path     :- s/Str
   x        :- s/Int
   y        :- s/Int
   width    :- s/Int
   height   :- s/Int])

(sm/defn make-image :- Image
  [nm :- s/Str contents :- BufferedImage]
  (Image. nm contents (.getWidth contents) (.getHeight contents)))

(def load-image
  (caching
    (fn [src]
      (if-let [img (ImageIO/read (io/input-stream src))]
        (make-image src img)))))

;; Transform produces value
;; TODO - return placeholder image when the named image is not found
(defnk image-from-resource :- Image
  [this]
  (load-image (:resource this)))

;; Behavior
(def ImageSource
  {:properties {:resource (resource)}
   :transforms {:image #'image-from-resource}
   :cached     #{:image}})

(defnk animation-frames [this g])

(def AnimationPlayback (s/enum :PLAYBACK_NONE :PLAYBACK_ONCE_FORWARD :PLAYBACK_ONCE_BACKWARD
                               :PLAYBACK_ONCE_PINGPONG :PLAYBACK_LOOP_FORWARD :PLAYBACK_LOOP_BACKWARD
                               :PLAYBACK_LOOP_PINGPONG))

(sm/defrecord Animation
  [images          :- [Image]
   fps             :- s/Int
   flip-horizontal :- s/Bool
   flip-vertical   :- s/Bool
   playback        :- AnimationPlayback])

(def AnimationBehavior
  {:inputs     {:images (as-schema [ImageSource])}
   :properties {:fps (non-negative-integer :default 30)
                :flip-horizontal {:schema s/Bool}
                :flip-vertical {:schema s/Bool}
                :playback (as-schema AnimationPlayback)}
   :transforms {:frames #'animation-frames}})

(sm/defrecord TextureSet
  [aabb         :- Rect
   packed-image :- Image
   coords       :- [Rect]
   sources      :- [Image]])

(sm/defn blank-image :- BufferedImage
  ([^Rect space :- Rect]
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

(sm/defn image-pixels :- ints
  [src :- BufferedImage]
  (let [w (.getWidth src)
        h (.getHeight src)
        pixels (int-array (* w h (image-color-components src)))]
    (.. src (getRaster) (getPixels 0 0 w h pixels))
    pixels))

(sm/defn image-from-pixels :- BufferedImage
  [width :- s/Int height :- s/Int t :- s/Int pixels :- ints]
  (doto (blank-image width height t)
    (.. (getRaster) (setPixels 0 0 width height pixels))))

(defmacro pixel-index [x y step stride]
  `(* ~step (+ ~x (* ~y ~stride))))

(defn clamper [low high] (fn [x] (min (max x low) high)))

(sm/defn extrude-borders :- Image
  "Return a new pixel array, larger than the original by `extrusion`
   with the orig-pixels centered in it. The source pixels on the edges
   will bleed into the surrounding empty space. The pixels in the border
   region will be identical to the nearest pixel of the source image."
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

(defn area
  [^Rect r]
  (if r
    (* (double (.width r)) (double (.height r)))
    0))

(defn intersect
  ([^Rect r] r)
  ([^Rect r1 ^Rect r2]
    (when (and r1 r2)
      (let [l (max (.x r1) (.x r2))
            t (max (.y r1) (.y r2))
            r (min (+ (.x r1) (.width r1))  (+ (.x r2) (.width r2)))
            b (min (+ (.y r1) (.height r1)) (+ (.y r2) (.height r2)))
            w (- r l)
            h (- b t)]
        (if (and (< 0 w) (< 0 h))
          (Rect. "" l t w h)
          nil))))
  ([^Rect r1 ^Rect r2 & rs]
    (reduce intersect (intersect r1 r2) rs)))

(defn short-side-fit [r1 r2]
  (min (Math/abs (- (.width r1)  (.width r2)))
       (Math/abs (- (.height r1) (.height r2)))))

(defn area-fit
  [^Rect r1 ^Rect r2]
  (- (area r1) (area r2)))

(defn score-rect [free-rect rect]
  (when (and  (>= (:width free-rect) (:width rect))
              (>= (:height free-rect) (:height rect)))
    [(area-fit free-rect rect) free-rect]))

; find the best free-rect into which to place rect
(defn score-rects [free-rects {:keys [width height] :as rect}]
  "Sort the free-rects according to how well rect fits into them.
   A 'good fit' means the least amount of leftover area."
  (reverse (remove nil? (sort-by first (map score-rect free-rects (repeat rect))))))

(defn split-rect
  [^Rect container ^Rect content]
  (let [new-rects (transient [])]
    (if-let [overlap (intersect container content)]
      (do
        ;; bottom slice
        (if (< (.y container) (.y overlap))
          (conj! new-rects (Rect. ""
                                  (.x container)
                                  (.y container)
                                  (.width container)
                                  (- (.y overlap) (.y container)))))

        ;; top slice
        (if (< (+ (.y overlap) (.height overlap)) (+ (.y container) (.height container)))
          (conj! new-rects (Rect. ""
                                  (.x container)
                                  (+ (.y overlap) (.height overlap))
                                  (.width container)
                                  (- (+ (.y container) (.height container))
                                     (+ (.y overlap)   (.height overlap))))))

	      ;; left slice
	      (if (< (.x container) (.x overlap))
	        (conj! new-rects (Rect. ""
	                                (.x container)
	                                (.y overlap)
	                                (- (.x overlap) (.x container))
	                                (.height overlap))))

	      ;; right slice
	      (if (< (+ (.x overlap) (.width overlap)) (+ (.x container) (.width container)))
	        (conj! new-rects (Rect. ""
	                                (+ (.x overlap) (.width overlap))
	                                (.y overlap)
	                                (- (+ (.x container) (.width container))
	                                   (+ (.x overlap)   (.width overlap)))
	                                (.height overlap)))))
      (conj! new-rects container))
    (persistent! new-rects)))

(defn place-in
  [^Rect container ^Rect content]
  (assoc content
         :x (.x container)
         :y (.y container)))

(defn image-bounds [source]
  (Rect. (.path source) 0 0 (.width source) (.height source)))

(def max-width 2048)

(defn- with-top-right-margin
  [margin r]
  (assoc r
         :width (+ margin (:width r))
         :height (+ margin (:height r))))

(sm/defn pack-at-size :- TextureSet
  [margin :- s/Int sources :- [Image] space-available :- Rect]
  (loop [free-rects   [space-available]
         remaining    (reverse (sort-by area (map image-bounds sources)))
         placed       []]
    (if-let [unplaced (first remaining)]
      (let [scored-rects   (score-rects free-rects (with-top-right-margin margin unplaced))
            best-fit       (second (first scored-rects))
            remaining-free (map second (rest scored-rects))]
        (if (nil? best-fit)
          :packing-failed
          (let [newly-placed (place-in best-fit unplaced)]
            (recur (reverse (sort-by area (concat remaining-free (split-rect best-fit (with-top-right-margin margin newly-placed)))))
                   (rest remaining)
                   (conj placed newly-placed)))))
      (TextureSet. space-available
                   nil
                   placed
                   sources))))

(defn doubling
  ([i] (iterate #(bit-shift-left % 1) i)))

(defn stepwise-doubling-sizes
  [start]
  (for [[w h] (partition 2 (rest (mapcat #(repeat 4 %) (doubling start))))]
    (Rect. "" 0 0 w h)))

(defn plausible-sizes
  [sources]
  (let [initial-power-of-two 64
        total-area           (reduce + (map area sources))
        sq                   (Math/sqrt total-area)
        short-side           (first (drop-while #(< sq %) (doubling initial-power-of-two)))]
    (stepwise-doubling-sizes short-side)))

(defn max-rects-pack
  ([sources]
    (max-rects-pack 0 sources))
  ([margin sources]
    (->> sources
      plausible-sizes
      (map #(pack-at-size margin sources %))
      (filter #(not= :packing-failed %))
      first)))

(defn image->rect [i]
  (Rect. (.path i) 0 0 (.width i) (.height i)))

(defn map-by [p coll]
  (zipmap (map p coll) coll))

(defn composite-sources [textureset sources]
  (let [src-by-path (map-by :path sources)
        image       (blank-image (:aabb textureset))
        graphics    (.getGraphics image)]
    (when *texture-pack-debug*
      (.setColor graphics java.awt.Color/RED)
      (.fillRect graphics 0 0 (.width (:aabb textureset)) (.height (:aabb textureset))))
    (doseq [rect (:coords textureset)]
      (when *texture-pack-debug*
        (.setColor graphics java.awt.Color/BLUE)
        (.fillRect graphics (.x rect) (.y rect) (.width rect) (.height rect)))
      (.drawImage graphics (:contents (get src-by-path (.path rect))) (int (.x rect)) (int (.y rect)) nil))
    (.dispose graphics)
    image))

(sm/defn pack-textures :- TextureSet
  [margin :- s/Int extrusion :- s/Int sources :- [Image]]
  (let [sources    (map (partial extrude-borders extrusion) sources)
        textureset (max-rects-pack margin (map image->rect sources))]
    (assoc textureset :packed-image (composite-sources textureset sources))))

(doseq [[v doc]
       {*ns*
        "Schema, behavior, and type information related to textures."

        #'AnimationBehavior
        "*behavior* - Used by [[dynamo.node/defnode]] to define the behavior for an animation resource.

Inputs:

* `:images` - vector of [[Image]]

Properties:

* `:fps` - non-negative integer, default 30

Transforms:

* `:frames` - see [[animation-frames]]"

        #'animation-frames
        "Returns the frames of the animation."

        #'ImageSource
        "*behavior* - Used by [[dynamo.node/defnode]] to define the behavior for an image resource. Cached labels: `:image`.

Properties:

* `:resource` - string path to resource ([[dynamo.types/resource]])

Transforms:

* `:image` - returns `{:path path :contents byte-array}`, see [[image-from-resource]]"

        #'image-from-resource
        "Returns `{:path path :contents byte-array}` from an image resource."
        }]
  (alter-meta! v assoc :doc doc))

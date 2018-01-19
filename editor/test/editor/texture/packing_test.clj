(ns editor.texture.packing-test
  (:require [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [editor.types :as t :refer [rect]]
            [editor.geom :refer :all]
            [editor.image-util :refer :all]
            [editor.texture :refer :all]
            [editor.texture.pack-max-rects :as itp :refer [max-rects-packing]]
            [schema.test :refer [validate-schemas]])
  (:import [editor.types Rect]))

(def rects (gen/fmap (fn [[x y w h]] (rect x y w h))
             (gen/tuple gen/int gen/int gen/s-pos-int gen/s-pos-int)))

(def origin-rects (gen/fmap (fn [[src w h]] (rect src 0 0 w h))
                    (gen/tuple (gen/resize 10 gen/string-alpha-numeric) gen/s-pos-int gen/s-pos-int)))

(def intersecting-rects
  (gen/bind rects
    #(gen/tuple (gen/return %)
       (gen/fmap (fn [[x y w h]] (rect x y w h))
         (gen/tuple gen/int gen/int
           (gen/resize (* 2 (.width %)) gen/s-pos-int)
           (gen/resize (* 2 (.height %)) gen/s-pos-int))))))

(defn total-area
  [non-overlapping-rects]
  (reduce + (map #(if % (area %) 0) non-overlapping-rects)))

(defspec split-rect-area-is-preserved
  (prop/for-all [[container content] intersecting-rects]
    (= (total-area (conj (split-rect container content) (intersect container content)))
       (area container))))

(defn all-pairs
  [xs]
  (for [x xs
        y xs
        :when (not= x y)]
    [x y]))

(defn dimensions= [b1 b2]
  (and
    (= (.width b1) (.width b2))
    (= (.height b1) (.height b2))))

(defn packed-textures-do-not-overlap [tex textures]
  (every? nil? (map #(apply intersect %) (all-pairs (:coords tex)))))

(defn packed-textures-fall-within-the-bounds-of-the-texturemap [tex textures]
  (every? #(dimensions= (intersect (:aabb tex) %) %) (:coords tex)))

(defn texturemap-includes-all-textures [tex textures]
  (= (count textures) (count (:coords tex))))

(defn textures-have-non-negative-origins [tex textures]
  (every? #(and (<= 0 (.x %))
                (<= 0 (.y %))) (:coords tex)))

(defn total-texture-area-fits [tex textures]
  (<= (total-area textures) (area (:aabb tex))))

(defspec texture-packing-invariant
  (prop/for-all [textures (gen/such-that not-empty (gen/resize 20 (gen/vector (gen/resize 128 origin-rects))))]
    (let [tex (max-rects-packing textures)]
      (or (= :packing-failed tex)
        (and (packed-textures-fall-within-the-bounds-of-the-texturemap tex textures)
          (texturemap-includes-all-textures tex textures)
          (textures-have-non-negative-origins tex textures)
          (packed-textures-do-not-overlap tex textures)
          (total-texture-area-fits tex textures))))))

;;; these should fit in 2048x512 or 512x2048.
(def test-rectangles-for-packing
  [(t/map->Rect {:path "/player/images/front_fist_open.png", :x 0, :y 0, :width 86, :height 87})
   (t/map->Rect {:path "/player/images/rear_upper_arm.png", :x 0, :y 0, :width 47, :height 87})
   (t/map->Rect {:path "/player/images/rear_thigh.png", :x 0, :y 0, :width 65, :height 104})
   (t/map->Rect {:path "/player/images/rear_foot.png", :x 0, :y 0, :width 113, :height 60})
   (t/map->Rect {:path "/player/images/head.png", :x 0, :y 0, :width 271, :height 298})
   (t/map->Rect {:path "/player/images/rear_bracer.png", :x 0, :y 0, :width 56, :height 72})
   (t/map->Rect {:path "/player/images/front_foot_bend1.png", :x 0, :y 0, :width 128, :height 70})
   (t/map->Rect {:path "/player/images/front_foot_bend2.png", :x 0, :y 0, :width 108, :height 93})
   (t/map->Rect {:path "/player/images/mouth_grind.png", :x 0, :y 0, :width 93, :height 59})
   (t/map->Rect {:path "/player/images/gun.png", :x 0, :y 0, :width 210, :height 203})
   (t/map->Rect {:path "/player/images/rear_shin.png", :x 0, :y 0, :width 75, :height 178})
   (t/map->Rect {:path "/player/images/front_upper_arm.png", :x 0, :y 0, :width 54, :height 97})
   (t/map->Rect {:path "/player/images/eye_surprised.png", :x 0, :y 0, :width 93, :height 89})
   (t/map->Rect {:path "/player/images/eye_indifferent.png", :x 0, :y 0, :width 93, :height 89})
   (t/map->Rect {:path "/player/images/muzzle.png", :x 0, :y 0, :width 462, :height 400})
   (t/map->Rect {:path "/player/images/front_bracer.png", :x 0, :y 0, :width 58, :height 80})
   (t/map->Rect {:path "/player/images/torso.png", :x 0, :y 0, :width 98, :height 180})
   (t/map->Rect {:path "/player/images/front_foot.png", :x 0, :y 0, :width 126, :height 69})
   (t/map->Rect {:path "/player/images/rear_foot_bend2.png", :x 0, :y 0, :width 103, :height 83})
   (t/map->Rect {:path "/player/images/mouth_oooo.png", :x 0, :y 0, :width 93, :height 59})
   (t/map->Rect {:path "/player/images/rear_foot_bend1.png", :x 0, :y 0, :width 117, :height 66})
   (t/map->Rect {:path "/player/images/neck.png", :x 0, :y 0, :width 36, :height 41})
   (t/map->Rect {:path "/player/images/front_shin.png", :x 0, :y 0, :width 82, :height 184})
   (t/map->Rect {:path "/builtins/graphics/particle_blob.png", :x 0, :y 0, :width 32, :height 32})
   (t/map->Rect {:path "/player/images/front_fist_closed.png", :x 0, :y 0, :width 75, :height 82})
   (t/map->Rect {:path "/player/images/goggles.png", :x 0, :y 0, :width 261, :height 166})
   (t/map->Rect {:path "/player/images/front_thigh.png", :x 0, :y 0, :width 48, :height 112})
   (t/map->Rect {:path "/player/images/mouth_smile.png", :x 0, :y 0, :width 93, :height 59})])


(defn packed-size
  [rects]
  (let [textureset (max-rects-packing 0 rects)]
    ((juxt :width :height) (:aabb textureset))))

(deftest known-size
 (let [packed (packed-size test-rectangles-for-packing)]
   (is (or (= [512 2048] packed) (= [2048 512] packed)))))

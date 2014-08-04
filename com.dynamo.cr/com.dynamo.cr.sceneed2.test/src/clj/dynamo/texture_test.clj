(ns dynamo.texture-test
  (:require [dynamo.texture :refer :all]
            [dynamo.geom :refer :all]
            [dynamo.image :refer :all]
            [dynamo.types :refer [rect]]
            [internal.texture.pack-max-rects :refer [max-rects-packing]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [schema.test :refer [validate-schemas]])
  (:import [dynamo.types Rect]))

(use-fixtures :once validate-schemas)

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
  (reduce + (map area non-overlapping-rects)))

(defspec split-rect-area-is-preserved
  1000
  (prop/for-all [[container content] intersecting-rects]
                (= (total-area (conj (split-rect container content)
                                     (intersect container content)))
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

(defn texturemap-inclues-all-textures [tex textures]
  (= (count textures) (count (:coords tex))))

(defn textures-have-non-negative-origins [tex textures]
  (every? #(and (<= 0 (.x %))
                (<= 0 (.y %))) (:coords tex)))

(defn total-texture-area-fits [tex textures]
  (< (total-area textures) (area (:aabb tex))))

(defspec texture-packing-invariant
 200
 (prop/for-all [textures (gen/such-that not-empty (gen/resize 20 (gen/vector (gen/resize 128 origin-rects))))]
               (let [tex (max-rects-packing textures)]
                 (or (= :packing-failed tex)
                     (and (packed-textures-fall-within-the-bounds-of-the-texturemap tex textures)
                          (texturemap-inclues-all-textures tex textures)
                          (textures-have-non-negative-origins tex textures)
                          (packed-textures-do-not-overlap tex textures)
                          (total-texture-area-fits tex textures))))))


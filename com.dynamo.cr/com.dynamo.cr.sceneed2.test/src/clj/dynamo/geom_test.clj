(ns dynamo.geom-test
  (:require [dynamo.geom :refer :all]
            [dynamo.types :refer :all]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all])
  (:import [com.dynamo.bob.textureset TextureSetGenerator]
           [javax.vecmath Vector3d]))

(def gen-float (gen/fmap (fn [[dec frac]]
                           (float (+ (float dec) (/ (rem (float frac) 10000.0) 10000.0))))
                         (gen/tuple gen/int (gen/resize 10000 gen/int))))

(defspec to-short-uv-works
  (prop/for-all [fuv gen-float]
                (= (TextureSetGenerator/toShortUV fuv) (to-short-uv fuv))))

(def gen-point (gen/fmap (fn [[x y z]] (Vector3d. x y z))
                         (gen/tuple gen/int gen/int gen/int)))

(def gen-aabb (gen/fmap (fn [vs]
                          (let [[[x0 x1] [y0 y1] [z0 z1]] (map sort (partition 2 vs))]
                            (->AABB (Vector3d. x0 y0 z0)
                                    (Vector3d. x1 y1 z1))))
                        (gen/tuple gen/int gen/int gen/int
                                   gen/int gen/int gen/int)))

(defspec aabb-union-reflexive
  (prop/for-all [aabb1 gen-aabb
                 aabb2 gen-aabb]
                (= (aabb-union aabb1 aabb2)
                   (aabb-union aabb2 aabb1))))

(defspec aabb-union-non-degenerate
  (prop/for-all [aabb1 gen-aabb
                 aabb2 gen-aabb]
                (let [u (aabb-union aabb1 aabb2)]
                  (and (<= (.. u min x) (.. u max x))
                       (<= (.. u min y) (.. u max y))
                       (<= (.. u min z) (.. u max z))))))

(defspec aabb-incorporates-points
  (prop/for-all [pointlist (gen/vector gen-point)]
                (let [aabb (reduce aabb-incorporate (null-aabb) pointlist)]
                  (every? #(aabb-contains? aabb %) pointlist))))

(defmacro reduce-field
  [f field coll]
  `(reduce ~f (map #(~field %) ~coll)))

(defspec aabb-incorporates-minimally
  (prop/for-all [pointlist (gen/such-that #(< 0 (count %)) (gen/vector gen-point))]
                (let [aabb (reduce aabb-incorporate (null-aabb) pointlist)
                      max-v (Vector3d. (reduce-field max .x pointlist)
                                       (reduce-field max .y pointlist)
                                       (reduce-field max .z pointlist))
                      min-v (Vector3d. (reduce-field min .x pointlist)
                                       (reduce-field min .y pointlist)
                                       (reduce-field min .z pointlist))]
                  (and
                    (= max-v (.max aabb))
                    (= min-v (.min aabb))))))

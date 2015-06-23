(ns dynamo.geom-test
  (:require [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [editor.types :as types]
            [editor.geom :as geom]
            [schema.test])
  (:import [com.defold.util Geometry]
           [javax.vecmath Point3d]))

(use-fixtures :once schema.test/validate-schemas)

(def gen-float (gen/fmap (fn [[dec frac]]
                           (float (+ (float dec) (/ (rem (float frac) 10000.0) 10000.0))))
                         (gen/tuple gen/int (gen/resize 10000 gen/int))))

(defspec to-short-uv-works
  (prop/for-all [fuv gen-float]
                (= (Geometry/toShortUV fuv) (geom/to-short-uv fuv))))

(deftest to-short-uv-problem-cases
  (testing "Avoid Clojure's off-by-one cases"
    (are [f-uv s-uv] (= s-uv (Geometry/toShortUV f-uv) (geom/to-short-uv f-uv))
             0      0
        1.3785  24804
        6.3785  24799
       -9.456  -29875
      -21.0141   -903)))

(def gen-point (gen/fmap (fn [[x y z]] (Point3d. x y z))
                         (gen/tuple gen/int gen/int gen/int)))

(def gen-aabb (gen/fmap (fn [vs]
                          (let [[[x0 x1] [y0 y1] [z0 z1]] (map sort (partition 2 vs))]
                            (types/->AABB (Point3d. x0 y0 z0)
                                    (Point3d. x1 y1 z1))))
                        (gen/tuple gen/int gen/int gen/int
                                   gen/int gen/int gen/int)))

(defspec aabb-union-reflexive
  (prop/for-all [aabb1 gen-aabb
                 aabb2 gen-aabb]
                (= (geom/aabb-union aabb1 aabb2)
                   (geom/aabb-union aabb2 aabb1))))

(defspec aabb-union-non-degenerate
  (prop/for-all [aabb1 gen-aabb
                 aabb2 gen-aabb]
                (let [u (geom/aabb-union aabb1 aabb2)]
                  (and (<= (.. u min x) (.. u max x))
                       (<= (.. u min y) (.. u max y))
                       (<= (.. u min z) (.. u max z))))))

(deftest aabb-union-null []
  (let [aabb (types/->AABB (Point3d. 0 0 0) (Point3d. 10 10 10))
        null (geom/null-aabb)]
    (is (= aabb (geom/aabb-union aabb null)))
    (is (= aabb (geom/aabb-union null aabb)))
    (is (= aabb (geom/aabb-union aabb aabb)))
    (is (= null (geom/aabb-union null null)))))

(defspec aabb-incorporates-points
  (prop/for-all [pointlist (gen/vector gen-point)]
                (let [aabb (reduce geom/aabb-incorporate (geom/null-aabb) pointlist)]
                  (every? #(geom/aabb-contains? aabb %) pointlist))))

(defmacro reduce-field
  [f field coll]
  `(reduce ~f (map #(~field %) ~coll)))

(defspec aabb-incorporates-minimally
  (prop/for-all [pointlist (gen/such-that #(< 0 (count %)) (gen/vector gen-point))]
                (let [aabb (reduce geom/aabb-incorporate (geom/null-aabb) pointlist)
                      max-v (Point3d. (reduce-field max .x pointlist)
                                       (reduce-field max .y pointlist)
                                       (reduce-field max .z pointlist))
                      min-v (Point3d. (reduce-field min .x pointlist)
                                       (reduce-field min .y pointlist)
                                       (reduce-field min .z pointlist))]
                  (and
                    (= max-v (.max aabb))
                    (= min-v (.min aabb))))))

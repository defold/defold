(ns dynamo.geom-test
  (:require [dynamo.geom :refer :all]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all])
  (:import [com.dynamo.bob.textureset TextureSetGenerator]))

(def gen-float (gen/fmap (fn [[dec frac]]
                           (float (+ (float dec) (/ (rem (float frac) 10000.0) 10000.0))))
                         (gen/tuple gen/int (gen/resize 10000 gen/int))))

(defspec to-short-uv-works
  (prop/for-all [fuv gen-float]
                (= (TextureSetGenerator/toShortUV fuv) (to-short-uv fuv))))

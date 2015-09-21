(ns editor.texture.math-test
  (:require [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [editor.texture.math :refer :all]))

(defn is-power-of-two [x]
  (and (not= 0 x) (= 0 (bit-and x (dec x)))))

(defspec next-power-of-two-works
  100
  (prop/for-all [x gen/pos-int]
    (is-power-of-two (next-power-of-two x))))

(defspec binary-search-yields-power-of-two
  100
  (prop/for-all [i1 gen/pos-int
                 i2 gen/pos-int]
    (let [[i1 i2] (sort [i1 i2])
          steps   (- (nbits i2) (nbits i1))
          dirs    (first (gen/sample (gen/vector gen/boolean steps) 1))
          bs      (binary-search-start i1 i2)
          vals    (map binary-search-current (reductions binary-search-next bs dirs))]
      (every? is-power-of-two (keep identity vals)))))

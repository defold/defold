;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.texture.math-test
  (:require [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [editor.texture.math :as m]))

(defn is-power-of-two [x]
  (and (not= 0 x) (= 0 (bit-and x (dec x)))))

(defspec next-power-of-two-works
  100
  (prop/for-all [x gen/pos-int]
    (is-power-of-two (m/next-power-of-two x))))

(defspec binary-search-yields-power-of-two
  100
  (prop/for-all [i1 gen/pos-int
                 i2 gen/pos-int]
    (let [[i1 i2] (sort [i1 i2])
          steps   (- (m/nbits i2) (m/nbits i1))
          dirs    (first (gen/sample (gen/vector gen/boolean steps) 1))
          bs      (m/binary-search-start i1 i2)
          vals    (map m/binary-search-current (reductions m/binary-search-next bs dirs))]
      (every? is-power-of-two (keep identity vals)))))

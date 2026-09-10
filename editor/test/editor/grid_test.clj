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

(ns editor.grid-test
  (:require [clojure.test :refer :all]
            [editor.grid :as grid]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest grid-axis-line-positions-test
  (is (= [[-1.0 -2.0 0.0]
          [-1.0 2.0 0.0]
          [0.0 -2.0 0.0]
          [0.0 2.0 0.0]
          [1.0 -2.0 0.0]
          [1.0 2.0 0.0]]
         (grid/grid-axis-line-positions 2 0 -1.0 2.0 1.0 1 -2.0 2.0))))

(deftest grid-fog-parameters-test
  (testing "perspective cameras use the legacy linear fog range"
    (let [[fog-start fog-end enabled _]
          (grid/grid-fog-parameters {:type :perspective
                                     :fov-x 90.0
                                     :fov-y 60.0
                                     :z-far 100.0})
          fog-start (double fog-start)
          fog-end (double fog-end)]
      (is (< (Math/abs (- (* 50.0 Math/PI) fog-start)) 0.0001))
      (is (< (Math/abs (- (* 100.0 Math/PI) fog-end)) 0.0001))
      (is (= 1.0 enabled))))

  (testing "non-perspective cameras disable fog"
    (is (= [0.0 1.0 0.0 0.0]
           (vec (grid/grid-fog-parameters {:type :orthographic}))))))

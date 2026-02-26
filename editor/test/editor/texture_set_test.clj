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

(ns editor.texture-set-test
  (:require [clojure.test :refer :all]
            [editor.slice9 :as slice9]
            [editor.texture-set :as texture-set]))

(set! *warn-on-reflection* true)

(def ^:private texture-transform-identity
  [1.0 0.0 0.0
   0.0 1.0 0.0
   0.0 0.0 1.0])

;; Expected transform for tex-coords tl=(0.1,0.2) tr=(0.5,0.2) bl=(0.1,0.8): maps unit square to that quad.
(def ^:private expected-transform-for-atlas-sub-rect
  [0.4 0.0 0.0
   0.0 0.6 0.0
   0.1 0.2 1.0])

(defn- vec= [a b]
  (and (= (count a) (count b))
       (every? true? (map #(<= (Math/abs (- (double %1) (double %2))) 1e-9) a b))))

(deftest vertex-data-texture-transform-quad-test
  (let [out (texture-set/vertex-data nil :size-mode-manual [100.0 100.0] nil :pivot-center)]
    (is (vec= texture-transform-identity (vec (:texture-transform out)))
        "Quad (no animation frame) uses identity texture transform.")))

;; Tex-coords for a sub-region of atlas (not unit square) -> non-identity transform.
(deftest vertex-data-texture-transform-frame-test
  (let [tex-coords [(vector-of :double 0.1 0.2)   ; tl
                    (vector-of :double 0.5 0.2)   ; tr
                    (vector-of :double 0.5 0.8)   ; br
                    (vector-of :double 0.1 0.8)] ; bl
        animation-frame {:tex-coords tex-coords
                         :width 40.0
                         :height 60.0
                         :page-index 0}
        out (texture-set/vertex-data animation-frame :size-mode-auto [40.0 60.0] nil :pivot-center)]
    (is (:texture-transform out))
    (is (= 9 (count (:texture-transform out))))
    (is (vec= expected-transform-for-atlas-sub-rect (vec (:texture-transform out)))
        "Frame texture transform must map unit square to atlas quad (tl,tr,bl).")))

;; Slice-9 path must use frame tex-coords for transform, not identity.
(deftest vertex-data-texture-transform-slice9-test
  (let [tex-coords [(vector-of :double 0.1 0.2)   ; tl
                    (vector-of :double 0.5 0.2)   ; tr
                    (vector-of :double 0.5 0.8)   ; br
                    (vector-of :double 0.1 0.8)] ; bl
        animation-frame {:tex-coords tex-coords
                         :width 40.0
                         :height 60.0
                         :page-index 0}
        slice9-margins [10.0 10.0 10.0 10.0]
        _ (is (slice9/sliced? slice9-margins) "slice9 is active")
        out (texture-set/vertex-data animation-frame :size-mode-manual [80.0 120.0] slice9-margins :pivot-center)]
    (is (:texture-transform out))
    (is (= 9 (count (:texture-transform out))))
    (is (vec= expected-transform-for-atlas-sub-rect (vec (:texture-transform out)))
        "Slice-9 path must produce same texture transform from frame tex-coords as non-slice-9 path.")))

;; Geometry path (trimmed/custom mesh): use-geometries frame still uses tex-coords for texture transform.
(deftest vertex-data-texture-transform-geometry-test
  (let [tex-coords [(vector-of :double 0.1 0.2)   ; tl
                    (vector-of :double 0.5 0.2)   ; tr
                    (vector-of :double 0.5 0.8)   ; br
                    (vector-of :double 0.1 0.8)] ; bl
        ;; Quad as two triangles: normalized positions and matching UVs.
        vertex-coords [(vector-of :double 0.0 0.0)
                       (vector-of :double 1.0 0.0)
                       (vector-of :double 1.0 1.0)
                       (vector-of :double 0.0 1.0)]
        indices [0 1 2 0 2 3]
        animation-frame {:tex-coords tex-coords
                         :use-geometries true
                         :vertex-coords vertex-coords
                         :vertex-tex-coords tex-coords
                         :indices indices
                         :width 40.0
                         :height 60.0
                         :page-index 0}
        out (texture-set/vertex-data animation-frame :size-mode-auto [40.0 60.0] nil :pivot-center)]
    (is (:texture-transform out))
    (is (= 9 (count (:texture-transform out))))
    (is (vec= expected-transform-for-atlas-sub-rect (vec (:texture-transform out)))
        "Geometry path must produce texture transform from frame tex-coords (same as quad path).")))

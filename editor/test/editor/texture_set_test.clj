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
            [editor.texture-set :as texture-set]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(def ^:private texture-transform-identity
  [1.0 0.0 0.0
   0.0 1.0 0.0
   0.0 0.0 1.0])

;; Expected transform for atlas sub-rect with Bob order BL,TL,TR,BR matching those UVs (engine column-major mat3).
(def ^:private expected-transform-for-atlas-sub-rect
  [0.4 0.0 0.0
   0.0 -0.6 0.0
   0.1 0.8 1.0])

(defn- vec= [a b]
  (and (= (count a) (count b))
       (every? true? (map #(<= (Math/abs (- (double %1) (double %2))) 1e-9) a b))))

(deftest vertex-data-texture-transform-quad-test
  (let [out (texture-set/vertex-data nil :size-mode-manual [100.0 100.0] nil :pivot-center)]
    (is (vec= texture-transform-identity (vec (:texture-transform out)))
        "Quad (no animation frame) uses identity texture transform.")))

;; Tex-coords for a sub-region of atlas (not unit square) -> non-identity transform.
(deftest vertex-data-texture-transform-frame-test
  (let [;; Preview UV order: uvnw, uvsw, uvse, uvne (see frame-vertex-data)
        tex-coords [(vector-of :double 0.1 0.2)
                    (vector-of :double 0.1 0.8)
                    (vector-of :double 0.5 0.8)
                    (vector-of :double 0.5 0.2)]
        tex-coords-raw [(vector-of :double 0.1 0.8)
                        (vector-of :double 0.1 0.2)
                        (vector-of :double 0.5 0.2)
                        (vector-of :double 0.5 0.8)]
        animation-frame {:tex-coords tex-coords
                         :tex-coords-raw tex-coords-raw
                         :atlas-rotated false
                         :width 40.0
                         :height 60.0
                         :page-index 0}
        out (texture-set/vertex-data animation-frame :size-mode-auto [40.0 60.0] nil :pivot-center)]
    (is (:texture-transform out))
    (is (= 9 (count (:texture-transform out))))
    (is (vec= expected-transform-for-atlas-sub-rect (vec (:texture-transform out)))
        "Frame texture transform matches engine (unflipped Bob corners).")))

;; Slice-9 path must use frame tex-coords for transform, not identity.
(deftest vertex-data-texture-transform-slice9-test
  (let [tex-coords [(vector-of :double 0.1 0.2)
                    (vector-of :double 0.1 0.8)
                    (vector-of :double 0.5 0.8)
                    (vector-of :double 0.5 0.2)]
        tex-coords-raw [(vector-of :double 0.1 0.8)
                        (vector-of :double 0.1 0.2)
                        (vector-of :double 0.5 0.2)
                        (vector-of :double 0.5 0.8)]
        animation-frame {:tex-coords tex-coords
                         :tex-coords-raw tex-coords-raw
                         :atlas-rotated false
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

;; The opt-in tiled material should have a geometry preview that matches the
;; runtime's twenty-quad Slice-9 grid while retaining atlas-local UVs.
(deftest tiled-slice9-material-preview-test
  (let [tex-coords [[0.125 0.25] [0.125 0.75] [0.625 0.75] [0.625 0.25]]
        frame {:tex-coords tex-coords
               :tex-coords-raw [[0.125 0.75] [0.125 0.25] [0.625 0.25] [0.625 0.75]]
               :flip-horizontal false
               :flip-vertical false
               :width 32.0
               :height 32.0
               :page-index 0}
        size [64.0 48.0]
        margins [2.0 2.0 2.0 2.0]
        original (texture-set/vertex-data frame :size-mode-manual size margins :pivot-center)
        tiled (slice9/tiled-vertex-data frame size margins original)]
    (is (= 54 (count (:position-data original))))
    (is (= 120 (count (:position-data tiled))))
    (is (= 120 (count (:uv-data tiled))))
    (is (= (:line-data original) (:line-data tiled)))
    (is (= (:texture-transform original) (:texture-transform tiled)))
    (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                 (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                (:uv-data tiled))
        "Repeated UVs must remain inside the source atlas rectangle.")
    (is (coll/any? (fn [[x _]] (< (Math/abs (+ x 2.0)) 0.0001))
                   (:position-data tiled))
        "The editor must contain actual intermediate tile seams, not stretched UVs.")))

;; Pathological vertex budgets must fail closed to the pre-existing Stretch
;; preview instead of generating unbounded geometry.
(deftest tiled-slice9-preview-fallback-test
  (let [raw [[0.0 1.0] [0.0 0.0] [1.0 0.0] [1.0 1.0]]
        frame {:width 32.0 :height 32.0 :tex-coords raw :tex-coords-raw raw
               :flip-horizontal false :flip-vertical false}
        margins [2.0 2.0 2.0 2.0]
        base (texture-set/vertex-data frame :size-mode-manual [64.0 48.0] margins :pivot-center)]
    (is (= base (slice9/tiled-vertex-data frame [100000.0 100000.0] margins base)))
    (is (= base (slice9/tiled-vertex-data frame [4.0 4.0] margins base)))))

;; The editor obtains real animation-flip flags from ->anim-data rather than
;; comparing Bob raw UV ordering to display UV ordering. The runtime tiles in
;; all four states, reversing clipped segment positions and sample phases.
(deftest tiled-slice9-authored-flip-preview-test
  (let [raw-tex-coords (java.nio.FloatBuffer/wrap
                         (float-array [0.125 0.75 0.125 0.25 0.625 0.25 0.625 0.75]))
        tex-dims (java.nio.FloatBuffer/wrap (float-array [32.0 32.0]))
        frames [0]
        geometries [{:rotated false :trim-mode :sprite-trim-mode-off}]
        size [64.0 48.0]
        authored-margins [4.0 2.0 8.0 6.0]]
    (doseq [[horizontal vertical] [[0 0] [1 0] [0 1] [1 1]]]
      (let [anim (#'texture-set/->anim-data
                   {:start 0 :end 1 :fps 0 :playback :playback-none
                    :flip-horizontal horizontal :flip-vertical vertical}
                   raw-tex-coords tex-dims [nil] frames frames geometries false)
            frame (first (:frames anim))
            preview-margins (slice9/oriented-margins authored-margins
                              (:flip-horizontal frame) (:flip-vertical frame))
            original (texture-set/vertex-data frame :size-mode-manual size preview-margins :pivot-center)
            tiled (slice9/tiled-vertex-data frame size preview-margins original)]
        (is (= (pos? horizontal) (:flip-horizontal frame)))
        (is (= (pos? vertical) (:flip-vertical frame)))
        (is (= 120 (count (:position-data tiled))) (str "H" horizontal " V" vertical " must tile"))
        (is (= 120 (count (:uv-data tiled))))
        (is (= (:line-data original) (:line-data tiled)))
        (is (= (:texture-transform original) (:texture-transform tiled)))
        (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                     (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                    (:uv-data tiled))
            "Flipped tile UVs must never escape the atlas frame.")
        (let [left-x (if (pos? horizontal) -24.0 -28.0)
              bottom-y (if (pos? vertical) -22.0 -18.0)
              first-u (if (pos? horizontal) 0.375 0.1875)
              first-v (if (pos? vertical) 0.40625 0.65625)]
          (is (coll/any? (fn [[[x y] [u v]]]
                           (and (< (Math/abs (- (double x) left-x)) 1e-6)
                                (< (Math/abs (- (double y) bottom-y)) 1e-6)
                                (< (Math/abs (- (double u) first-u)) 1e-6)
                                (< (Math/abs (- (double v) first-v)) 1e-6)))
                         (mapv vector (:position-data tiled) (:uv-data tiled)))
              "First center tile clipped phase must match native flipped-segment oracle."))))))

;; Actual texture-set frame construction on a 90-degree packed source image,
;; with all four animation flip flags, exercises transposed UV directions.
(deftest tiled-slice9-rotated-authored-flip-test
  (let [raw-tex-coords (java.nio.FloatBuffer/wrap
                         (float-array [0.125 0.75 0.625 0.75 0.625 0.25 0.125 0.25]))
        tex-dims (java.nio.FloatBuffer/wrap (float-array [32.0 32.0]))
        frames [0]
        geometries [{:rotated true :trim-mode :sprite-trim-mode-off}]
        size [64.0 48.0]
        margins [4.0 2.0 8.0 6.0]]
    (doseq [[horizontal vertical] [[0 0] [1 0] [0 1] [1 1]]]
      (let [frame (first (:frames (#'texture-set/->anim-data
                                    {:start 0 :end 1 :fps 0 :playback :playback-none
                                     :flip-horizontal horizontal :flip-vertical vertical}
                                    raw-tex-coords tex-dims [nil] frames frames geometries false)))
            preview-margins (slice9/oriented-margins margins (:flip-horizontal frame) (:flip-vertical frame))
            original (texture-set/vertex-data frame :size-mode-manual size preview-margins :pivot-center)
            tiled (slice9/tiled-vertex-data frame size preview-margins original)]
        (is (true? (:atlas-rotated frame)))
        (is (= 120 (count (:position-data tiled))))
        (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                     (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                    (:uv-data tiled)))
        (is (= (:line-data original) (:line-data tiled)))))))

;; Zero-margin tiling must retain six vertices per tile across authored flips,
;; guarding against the regression that silently stretched unsliced boxes.
(deftest tiled-zero-margin-authored-flip-test
  (let [raw-tex-coords (java.nio.FloatBuffer/wrap
                         (float-array [0.125 0.75 0.125 0.25 0.625 0.25 0.625 0.75]))
        tex-dims (java.nio.FloatBuffer/wrap (float-array [32.0 32.0]))
        frames [0]
        geometries [{:rotated false :trim-mode :sprite-trim-mode-off}]
        size [80.0 48.0]
        margins [0.0 0.0 0.0 0.0]]
    (doseq [[horizontal vertical] [[0 0] [1 0] [0 1] [1 1]]]
      (let [frame (first (:frames (#'texture-set/->anim-data
                                    {:start 0 :end 1 :fps 0 :playback :playback-none
                                     :flip-horizontal horizontal :flip-vertical vertical}
                                    raw-tex-coords tex-dims [nil] frames frames geometries false)))
            original (texture-set/vertex-data frame :size-mode-manual size margins :pivot-center)
            tiled (slice9/tiled-vertex-data frame size margins original)]
        (is (= 6 (count (:position-data original))))
        (is (= 36 (count (:position-data tiled))) "3×2 true zero-margin repeats including flips")
        (is (= (:page-index original) (:page-index tiled)))
        (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                     (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                    (:uv-data tiled)))))))

;; Tiling the editor's 90-degree atlas frame must preserve frame-local UVs,
;; and dragging the size handle must change the repeated vertex count.
(deftest tiled-slice9-rotated-and-resized-preview-test
  (let [frame {:width 32.0 :height 32.0 :atlas-rotated true
               :flip-horizontal false :flip-vertical false
               :tex-coords [[0.125 0.75] [0.625 0.75] [0.625 0.25] [0.125 0.25]]}
        margins [2.0 2.0 2.0 2.0]
        size [64.0 48.0]
        rotated (slice9/tiled-vertex-data frame size margins
                  (texture-set/vertex-data frame :size-mode-manual size margins :pivot-center))
        resized-size [100.0 48.0]
        resized (slice9/tiled-vertex-data frame resized-size margins
                  (texture-set/vertex-data frame :size-mode-manual resized-size margins :pivot-center))]
    (is (= 120 (count (:position-data rotated))))
    (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                 (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                (:uv-data rotated)))
    (is (= 144 (count (:position-data resized))))))

;; The new runtime branch (fa74298) also repeats a normal zero-margin BOX.
;; The editor's unsliced frame quad has a different vertex ordering from the
;; nine-slice mesh, and paged materials must use identical preview geometry.
(deftest tiled-zero-margin-box-preview-test
  (let [frame {:width 32.0 :height 32.0 :page-index 2
               :tex-coords [[0.125 0.25] [0.625 0.25] [0.125 0.75] [0.625 0.75]]
               :flip-horizontal false :flip-vertical false}
        size [64.0 48.0]
        unsliced [0.0 0.0 0.0 0.0]
        original (texture-set/vertex-data frame :size-mode-manual size unsliced :pivot-center)
        tiled (slice9/tiled-vertex-data frame size unsliced original)
        oversized-size [100.0 48.0]
        oversized (slice9/tiled-vertex-data frame oversized-size unsliced
                    (texture-set/vertex-data frame :size-mode-manual oversized-size unsliced :pivot-center))]
    (is (= 6 (count (:position-data original))))
    (is (= 24 (count (:position-data tiled))) "2×2 unclipped center frame repeat")
    (is (= 24 (count (:uv-data tiled))))
    (is (= 48 (count (:position-data oversized))) "4×2 tiles after width edit")
    (is (= (:page-index original) (:page-index tiled)))
    (is (= (:line-data original) (:line-data tiled)))
    (is (= (:texture-transform original) (:texture-transform tiled)))
    (is (every? (fn [[u v]] (and (<= (- 0.125 1e-9) u (+ 0.625 1e-9))
                                 (<= (- 0.25 1e-9) v (+ 0.75 1e-9))))
                (:uv-data tiled)))
    (is (= 24 (count (:position-data
                       (slice9/tiled-vertex-data (assoc frame :flip-horizontal true)
                         size unsliced original)))))
    (is (= original (slice9/tiled-vertex-data (assoc frame :use-geometries true)
                      size unsliced original)))))

;; Geometry path (trimmed/custom mesh): use-geometries frame still uses tex-coords for texture transform.
(deftest vertex-data-texture-transform-geometry-test
  (let [tex-coords [(vector-of :double 0.1 0.2)
                    (vector-of :double 0.1 0.8)
                    (vector-of :double 0.5 0.8)
                    (vector-of :double 0.5 0.2)]
        tex-coords-raw [(vector-of :double 0.1 0.8)
                        (vector-of :double 0.1 0.2)
                        (vector-of :double 0.5 0.2)
                        (vector-of :double 0.5 0.8)]
        ;; Quad as two triangles: normalized positions and matching UVs.
        vertex-coords [(vector-of :double 0.0 0.0)
                       (vector-of :double 1.0 0.0)
                       (vector-of :double 1.0 1.0)
                       (vector-of :double 0.0 1.0)]
        ;; Per-vertex UVs in same order as vertex-coords (NW, NE, SE, SW)
        vertex-tex-coords [(vector-of :double 0.1 0.2)
                           (vector-of :double 0.5 0.2)
                           (vector-of :double 0.5 0.8)
                           (vector-of :double 0.1 0.8)]
        indices [0 1 2 0 2 3]
        animation-frame {:tex-coords tex-coords
                         :tex-coords-raw tex-coords-raw
                         :atlas-rotated false
                         :use-geometries true
                         :vertex-coords vertex-coords
                         :vertex-tex-coords vertex-tex-coords
                         :indices indices
                         :width 40.0
                         :height 60.0
                         :page-index 0}
        out (texture-set/vertex-data animation-frame :size-mode-auto [40.0 60.0] nil :pivot-center)]
    (is (:texture-transform out))
    (is (= 9 (count (:texture-transform out))))
    (is (vec= expected-transform-for-atlas-sub-rect (vec (:texture-transform out)))
        "Geometry path must produce texture transform from frame tex-coords (same as quad path).")))

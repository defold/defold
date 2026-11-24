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

(ns editor.slice9-test
  (:require [clojure.test :refer :all]
            [editor.image :as image]
            [editor.slice9 :as slice9]
            [editor.sprite :as sprite]
            [editor.texture-set :as texture-set]
            [util.coll :as coll])
  (:import [com.google.protobuf ByteString]
           [java.nio ByteBuffer ByteOrder]))

(set! *warn-on-reflection* true)

(def ^:private indices
  [0 1 5 0 5 4 1 2 6 1 6 5 2 3 7 2 7 6
   4 5 9 4 9 8 5 6 10 5 10 9 6 7 11 6 11 10
   8 9 13 8 13 12 9 10 14 9 14 13 10 11 15 10 15 14])

(def ^:private frame
  {:width 128.0
   :height 64.0
   :texture-width 256.0
   :texture-height 128.0
   :tex-coords [[0.125 0.25] [0.125 0.75] [0.625 0.75] [0.625 0.25]]
   :pivot [0.25 -0.25]
   :page-index 2})

(defn- near [expected actual]
  (< (Math/abs (- (double expected) (double actual))) 0.00001))

;; Verifies sprite and GUI pivots use the engine's grid, including fractional
;; sizes and flipped borders.
(deftest shared-position-and-outline-test
  (doseq [[pivot px py] [[:pivot-center 0.5 0.5]
                         [:pivot-n 0.5 1.0] [:pivot-ne 1.0 1.0] [:pivot-e 1.0 0.5]
                         [:pivot-se 1.0 0.0] [:pivot-s 0.5 0.0] [:pivot-sw 0.0 0.0]
                         [:pivot-w 0.0 0.5] [:pivot-nw 0.0 1.0]]
          flip-h [false true]
          flip-v [false true]]
    (let [animation-frame (assoc frame :flip-horizontal flip-h :flip-vertical flip-v)
          {:keys [position-data line-data page-index]}
          (texture-set/vertex-data animation-frame :size-mode-manual [32.5 24.25] [2.0 3.0 4.0 5.0] pivot)
          xs (if flip-h [0.0 4.0 30.5 32.5] [0.0 2.0 28.5 32.5])
          ys (if flip-v [0.0 3.0 19.25 24.25] [0.0 5.0 21.25 24.25])]
      (is (= 2 page-index))
      (is (= 54 (count position-data)))
      (is (= 72 (count line-data)))
      (doseq [[i [x y z]] (eduction (map-indexed vector) position-data)]
        (let [vertex (indices i)]
          (is (near (- (xs (mod vertex 4)) (* 32.5 (+ px 0.25))) x))
          (is (near (- (ys (quot vertex 4)) (* 24.25 (- py 0.25))) y))
          (is (zero? z))))
      (is (= (set position-data) (set line-data)))
      (is (= (mapv position-data [0 1 1 2 2 5 5 0]) (subvec line-data 0 8))))))

;; Verifies UVs use original atlas dimensions and raw corners.
;; Each animation flip must be applied once.
(deftest shared-atlas-uv-test
  (doseq [rotated [false true]
          flip-h [false true]
          flip-v [false true]]
    (let [corners (if rotated
                    [[0.125 0.75] [0.625 0.75] [0.625 0.25] [0.125 0.25]]
                    (:tex-coords frame))
          animation-frame (assoc frame :tex-coords corners :flip-horizontal flip-h :flip-vertical flip-v)
          {:keys [uv-data]} (slice9/vertex-data animation-frame [32.5 24.25] [2.0 3.0 4.0 5.0] :pivot-center)
          us (if rotated [0.125 0.14453125 0.61328125 0.625] [0.125 0.1328125 0.609375 0.625])
          vs (if rotated [0.75 0.734375 0.28125 0.25] [0.25 0.2890625 0.7265625 0.75])]
      (is (= 54 (count uv-data)))
      (doseq [[i [u v]] (eduction (map-indexed vector) uv-data)]
        (let [vertex (indices i)
              x (mod vertex 4)
              y (quot vertex 4)
              x (if flip-h (- 3 x) x)
              y (if flip-v (- 3 y) y)]
          (is (near (us (if rotated y x)) u))
          (is (near (vs (if rotated x y)) v)))))))

;; Verifies legacy standalone-image frames retain frame/box UV sizing without
;; missing-dimension exceptions, including the minimum size for zero dimensions.
(deftest legacy-dimension-fallback-test
  (doseq [[description dimensions size us vs]
          [["Box dimensions when the frame has no dimensions"
            {} [64.0 32.0]
            [0.0 0.03125 0.9375 1.0] [0.0 0.15625 0.90625 1.0]]
           ["Frame dimensions take precedence over box dimensions"
            {:width 128.0 :height 64.0} [64.0 32.0]
            [0.0 0.015625 0.96875 1.0] [0.0 0.078125 0.953125 1.0]]
           ["Each dimension falls back independently"
            {:width 128.0} [64.0 32.0]
            [0.0 0.015625 0.96875 1.0] [0.0 0.15625 0.90625 1.0]]
           ["Zero box dimensions use the legacy minimum"
            {} [0.0 0.0]
            [0.0 2.0 -3.0 1.0] [0.0 5.0 -2.0 1.0]]
           ["Zero frame dimensions use the legacy minimum"
            {:width 0.0 :height 0.0} [64.0 32.0]
            [0.0 2.0 -3.0 1.0] [0.0 5.0 -2.0 1.0]]]
          pivot [:pivot-center :pivot-nw]]
    (testing (str description " with " pivot)
      (let [animation-frame (merge (dissoc (get-in (image/produce-anim-data {:size {:width 64 :height 32}})
                                                   [nil :frames 0])
                                           :width :height :texture-width :texture-height)
                                   dimensions)
            {:keys [position-data uv-data line-data]}
            (texture-set/vertex-data animation-frame :size-mode-manual size [2.0 3.0 4.0 5.0] pivot)]
        (is (= 54 (count position-data) (count uv-data)))
        (is (= 72 (count line-data)))
        (doseq [[i [u v]] (eduction (map-indexed vector) uv-data)]
          (let [vertex (indices i)]
            (is (near (us (mod vertex 4)) u))
            (is (near (vs (quot vertex 4)) v))))
        (doseq [value (eduction cat cat [position-data uv-data line-data])]
          (is (Double/isFinite (double value))))))))

;; Verifies actual ImageNode frames match engine UV orientation with and without
;; slicing, and retain original image dimensions for resized box borders.
(deftest standalone-image-slice-orientation-test
  (let [animation-frame (get-in (image/produce-anim-data {:size {:width 64 :height 32}})
                                [nil :frames 0])]
    (doseq [pivot [:pivot-center :pivot-nw]
            size [[64.0 32.0] [128.0 64.0]]]
      (let [unsliced (texture-set/vertex-data animation-frame :size-mode-manual size [0.0 0.0 0.0 0.0] pivot)
            sliced (texture-set/vertex-data animation-frame :size-mode-manual size [2.0 3.0 4.0 5.0] pivot)
            position->uv (into {} (mapv (fn [position uv] [(subvec position 0 2) uv])
                                        (:position-data sliced)
                                        (:uv-data sliced)))]
        (doseq [[position uv] (mapv vector (:position-data unsliced) (:uv-data unsliced))]
          (is (= (mapv double uv) (position->uv (subvec position 0 2)))))
        (is (near 0.0 (get-in sliced [:uv-data 0 1])))
        (is (near (/ 5.0 32.0) (get-in sliced [:uv-data 2 1])))
        (is (near (- 1.0 (/ 3.0 32.0)) (get-in sliced [:uv-data 20 1])))
        (is (near 1.0 (get-in sliced [:uv-data 38 1])))))))

;; Verifies zero dimensions, collapsed cells and oversized margins retain the
;; native topology without NaNs.
(deftest shared-degenerate-geometry-test
  (doseq [size [[0.0 0.0] [0.001 0.001] [8.0 8.0]]
          margins [[0.0 0.0 0.0 0.0] [12.0 10.0 10.0 12.0]]]
    (let [{:keys [position-data uv-data line-data]} (slice9/vertex-data frame size margins :pivot-center)]
      (is (= 54 (count position-data) (count uv-data)))
      (is (= 72 (count line-data)))
      (doseq [value (eduction cat position-data)]
        (is (Double/isFinite (double value)))))))

(defn- float-bytes [values]
  (let [buffer (doto (ByteBuffer/allocate (* 4 (count values)))
                 (.order ByteOrder/LITTLE_ENDIAN))]
    (doseq [value values]
      (.putFloat buffer (float value)))
    (ByteString/copyFrom (.array buffer))))

;; Verifies canonical atlas UVs apply each flip exactly once in both paths,
;; including rotated entries. Material transforms remain independent of flips.
(deftest animation-frame-shared-input-test
  (doseq [rotated [false true]
          flip-h [0 1]
          flip-v [0 1]]
    (let [corners (if rotated
                    [[0.125 0.75] [0.625 0.75] [0.625 0.25] [0.125 0.25]]
                    (:tex-coords frame))
          texture-set {:width 256
                       :height 128
                       :tex-coords (float-bytes (coll/into-> corners [] cat))
                       :tex-dims (float-bytes [128 64])
                       :frame-indices [0]
                       :page-indices [2]
                       :geometries [{:pivot-x 0.25
                                     :pivot-y -0.25
                                     :rotated rotated
                                     :trim-mode :sprite-trim-mode-off}]
                       :animations [{:id "slice"
                                     :start 0
                                     :end 1
                                     :fps 0
                                     :flip-horizontal flip-h
                                     :flip-vertical flip-v}]}
          decoded (get-in (texture-set/make-anim-data texture-set [nil]) ["slice" :frames 0])
          unsliced (texture-set/vertex-data decoded :size-mode-manual [32.5 24.25] nil :pivot-center)
          sliced (texture-set/vertex-data decoded :size-mode-manual [32.5 24.25] [2.0 3.0 4.0 5.0] :pivot-center)
          position->uv (into {} (mapv (fn [position uv] [(subvec position 0 2) uv])
                                      (:position-data sliced)
                                      (:uv-data sliced)))]
      (is (= 256 (:texture-width decoded)))
      (is (= 128 (:texture-height decoded)))
      (is (= (pos? flip-h) (:flip-horizontal decoded)))
      (is (= (pos? flip-v) (:flip-vertical decoded)))
      (is (= corners (:tex-coords decoded)))
      (is (= [0.5 0.0 0.0 0.0 0.5 0.0 0.125 0.25 1.0]
             (:texture-transform unsliced)
             (:texture-transform sliced)))
      (doseq [[[x y] uv position] (mapv vector
                                        [[0 0] [1 0] [0 1] [1 0] [1 1] [0 1]]
                                        (:uv-data unsliced)
                                        (:position-data unsliced))]
        (let [x (if (pos? flip-h) (- 1 x) x)
              y (if (pos? flip-v) (- 1 y) y)]
          (is (= (if rotated
                   [(+ 0.125 (* 0.5 y)) (- 0.75 (* 0.5 x))]
                   [(+ 0.125 (* 0.5 x)) (+ 0.25 (* 0.5 y))])
                 uv))
          (is (= uv (position->uv (subvec position 0 2)))))))))

;; Verifies trimmed meshes retain their UV-to-position association after flips,
;; without applying quad UV flips a second time to their per-vertex UV data.
(deftest trimmed-animation-flips-test
  (doseq [flip-h [0 1]
          flip-v [0 1]]
    (let [texture-set {:width 256
                       :height 128
                       :tex-coords (float-bytes (coll/into-> (:tex-coords frame) [] cat))
                       :tex-dims (float-bytes [128 64])
                       :frame-indices [0]
                       :page-indices [0]
                       :use-geometries 1
                       :geometries [{:width 128
                                     :height 64
                                     :trim-mode :sprite-trim-mode-4
                                     :vertices [-0.5 -0.25 0.25 -0.25 0.0 0.5]
                                     :uvs [0.125 0.25 0.625 0.25 0.125 0.75]
                                     :indices [0 1 2]}]
                       :animations [{:id "trimmed"
                                     :start 0
                                     :end 1
                                     :fps 0
                                     :flip-horizontal flip-h
                                     :flip-vertical flip-v}]}
          decoded (get-in (texture-set/make-anim-data texture-set [nil]) ["trimmed" :frames 0])
          rendered (texture-set/vertex-data decoded :size-mode-auto [128.0 64.0] nil :pivot-center)
          uv->position (zipmap (:uv-data rendered) (:position-data rendered))]
      (is (= (:tex-coords frame) (:tex-coords decoded)))
      (is (= [0.5 0.0 0.0 0.0 0.5 0.0 0.125 0.25 1.0] (:texture-transform rendered)))
      (doseq [[uv [x y]] [[[0.125 0.25] [-64.0 -16.0]]
                          [[0.625 0.25] [32.0 -16.0]]
                          [[0.125 0.75] [0.0 32.0]]]]
        (is (= [(if (pos? flip-h) (- x) x)
                (if (pos? flip-v) (- y) y)
                0.0 1.0]
               (uv->position uv)))))))

;; Verifies outline buffers accommodate all nine native cells even when only one
;; margin is nonzero.
(deftest sprite-outline-buffer-test
  (let [margins [2.0 0.0 0.0 0.0]
        scene (sprite/produce-scene {:_node-id 1
                                     :aabb nil
                                     :material-shader nil
                                     :scene-infos [{:animation {:frames [frame]}}]
                                     :size-mode :size-mode-manual
                                     :size [32.5 24.25]
                                     :slice9 margins
                                     :blend-mode :blend-mode-alpha
                                     :material-attribute-infos nil
                                     :vertex-attribute-bytes nil})
        quad-count (get-in scene [:children 0 :renderable :user-data :quad-count])
        {:keys [line-data]} (texture-set/vertex-data frame :size-mode-manual [32.5 24.25] margins :pivot-center)]
    (is (= 9 quad-count))
    (is (= (* quad-count 8) (count line-data)))))

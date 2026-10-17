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

(ns editor.slice9
  (:require [editor.geom :as geom]
            [util.coll :as coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- box-corner-coords->vertices2 [[x0 y0 x1 y1]]
  [[x0 y0] [x0 y1] [x1 y1] [x1 y0]])

(defn- rotated-box-corner-coords->vertices2 [[x0 y0 x1 y1]]
  [[x0 y0] [x1 y0] [x1 y1] [x0 y1]])

(defn- box-corner-coords->vertices3 [[x0 y0 x1 y1]]
  [[x0 y0 0.0] [x0 y1 0.0] [x1 y1 0.0] [x1 y0 0.0]])

(defn- ranges->box-corner-coords [x-ranges y-ranges]
  (for [[x0 x1] x-ranges
        [y0 y1] y-ranges]
    [x0 y0 x1 y1]))

(defn- ranges->rotated-box-corner-coords [x-ranges y-ranges]
  (for [[y0 y1] y-ranges
        [x0 x1] x-ranges]
    [x0 y0 x1 y1]))

(defn- steps->ranges [v]
  (partition 2 1 v))

(defn- pivot-offset [pivot size]
  (let [h-align (geom/gui-pivot->h-align pivot)
        v-align (geom/gui-pivot->v-align pivot)
        xs (case h-align
             :right -1.0
             :center -0.5
             :left 0.0)
        ys (case v-align
             :top -1.0
             :middle -0.5
             :bottom 0.0)]
    (mapv * size [xs ys 1])))

(def ^:private box-triangles-vertex-order [0 1 3 3 1 2])

(defn sliced? [slice9]
  (some? (some pos? slice9)))

(defn box->triangle-vertices [box]
  ;; box vertices are in order BL TL TR BR
  ;;
  ;; turns:
  ;; 1---2
  ;; |   |
  ;; 0---3
  ;;
  ;; into:
  ;; 1     1-2
  ;; |\  +  \|
  ;; 0-3     3
  ;;
  ;; normal uv box vertices are in order BL TL TR BR
  ;;
  ;; turns:
  ;; 1---2
  ;; |   |
  ;; 0---3
  ;;
  ;; into:
  ;; 1     1-2
  ;; |\  +  \|
  ;; 0-3     3
  ;;
  ;; _rotated_ uv box vertices are in order TL TR BR BL
  ;;
  ;; turns:
  ;;
  ;; 0---1
  ;; |   |
  ;; 3---2
  ;;
  ;; into:
  ;; 0-1     1
  ;; |/  +  /|
  ;; 3     3-2
  (map box box-triangles-vertex-order))

(defn vertex-data
  [{:keys [width height tex-coords] :as _animation-frame} size slice9 pivot]
  (let [[^double box-width ^double box-height] size
        texture-width (max 1.0 (double (or width box-width)))
        texture-height (max 1.0 (double (or height box-height)))
        ;; Sample tex-coords if anim from tile source:
        ;;
        ;;  no flip:  [[0.0 0.140625] [0.0 1.0] [0.5566406 1.0] [0.5566406 0.140625]]   TL BL BR TR     T-B-B-T L-L-R-R
        ;;   flip-h:  [[0.5566406 0.140625] [0.5566406 1.0] [0.0 1.0] [0.0 0.140625]]   "TR BR BL TL"   T-B-B-T R-R-L-L
        ;;   flip-v:  [[0.0 1.0] [0.0 0.140625] [0.5566406 0.140625] [0.5566406 1.0]]   "BL TL TR BR"   B-T-T-B L-L-R-R
        ;; flip-h+v:  [[0.5566406 1.0] [0.5566406 0.140625] [0.0 0.140625] [0.0 1.0]]   "BR TR TL BL"   B-T-T-B R-R-L-L
        ;;
        ;; Sample tex-coords from rotated (90 CW) image in atlas:
        ;;           [[0.0 1.0] [0.8691406 1.0] [0.8691406 0.375] [0.0 0.375]]          TL TR BR BL
        ;;
        ;; In comp_gui.cpp RenderBoxNodes we determine whether the texture is rotated like this:
        uv-rotated? (and tex-coords
                         (not= (get-in tex-coords [0 0])
                               (get-in tex-coords [1 0]))
                         (not= (get-in tex-coords [1 1])
                               (get-in tex-coords [2 1])))
        [^double u0 ^double v0] (get tex-coords 0 [0.0 0.0])
        [^double u1 ^double v1] (get tex-coords 2 [1.0 1.0])
        u-delta (- u1 u0)
        v-delta (- v1 v0)
        uv-boxes (if-not uv-rotated?
                   ;;    ^
                   ;;    |
                   ;;
                   ;;   v1   ___________
                   ;;       |           |
                   ;;       |           |
                   ;;   v1- |   -   X   |
                   ;;       |           |
                   ;;       | *       * |
                   ;;       |  *     *  |
                   ;;   v0+ |    ***    |
                   ;;       |           |
                   ;;   v0   -----------
                   ;;       u0 u0+ u1- u1 -->
                   ;;
                   ;; uv-box vertex order is:
                   ;;
                   ;; 1---2
                   ;; |   |
                   ;; |   |
                   ;; 0---3
                   ;;
                   ;; Slice 9 uv-box order:
                   ;;
                   ;;  ___________
                   ;; |   |   |   |
                   ;; | 3 | 6 | 9 |
                   ;; |---|---|---|
                   ;; | 2 | 5 | 8 |
                   ;; |---|---|---|
                   ;; | 1 | 4 | 7 |
                   ;; |   |   |   |
                   ;;  -----------
                   (let [u-steps [u0
                                  (+ u0 (* u-delta (/ ^double (get slice9 0) texture-width)))
                                  (- u1 (* u-delta (/ ^double (get slice9 2) texture-width)))
                                  u1]
                         v-steps [v0
                                  (+ v0 (* v-delta (/ ^double (get slice9 3) texture-height)))
                                  (- v1 (* v-delta (/ ^double (get slice9 1) texture-height)))
                                  v1]
                         uv-box-coords (ranges->box-corner-coords (steps->ranges u-steps) (steps->ranges v-steps))]
                     (map box-corner-coords->vertices2 uv-box-coords))
                   ;;   ^
                   ;;   |
                   ;;
                   ;;  v0   __________________
                   ;;      |                  |
                   ;;  v0+ |     *            |
                   ;;      |    *       -     |
                   ;;      |   *              |
                   ;;      |    *       X     |
                   ;;  v1- |     *            |
                   ;;      |                  |
                   ;;  v1   ------------------
                   ;;      u0  u0+      u1-  u1 -->
                   ;;
                   ;;  uv-box vertex order (handled by rotated-box-corner-coords->vertices2) is:
                   ;;
                   ;;  0---1
                   ;;  |   |
                   ;;  |   |
                   ;;  3---2
                   ;;
                   ;; Slice 9 uv-box order (ranges->rotated-box-corner-coords):
                   ;;
                   ;;  ___________
                   ;; |   |   |   |
                   ;; | 1 | 2 | 3 |
                   ;; |---|---|---|
                   ;; | 4 | 5 | 6 |
                   ;; |---|---|---|
                   ;; | 7 | 8 | 9 |
                   ;; |   |   |   |
                   ;;  -----------
                   (let [u-steps [u0
                                  (+ u0 (* u-delta (/ ^double (get slice9 3) texture-height)))
                                  (- u1 (* u-delta (/ ^double (get slice9 1) texture-height)))
                                  u1]
                         v-steps [v0
                                  (+ v0 (* v-delta (/ ^double (get slice9 0) texture-width)))
                                  (- v1 (* v-delta (/ ^double (get slice9 2) texture-width)))
                                  v1]
                         uv-box-coords (ranges->rotated-box-corner-coords (steps->ranges u-steps) (steps->ranges v-steps))]
                     (map rotated-box-corner-coords->vertices2 uv-box-coords)))
        x-steps [0.0 ^double (get slice9 0) (- box-width ^double (get slice9 2)) box-width]
        y-steps [0.0 ^double (get slice9 3) (- box-height ^double (get slice9 1)) box-height]
        xy-box-coords (ranges->box-corner-coords (steps->ranges x-steps) (steps->ranges y-steps))
        empty-xy-box-coords+uv-boxes (mapv pair xy-box-coords uv-boxes)

        non-empty-xy-box-coords+uv-boxes
        (or (coll/not-empty
              (filterv
                (fn [[[x0 y0 x1 y1] _uv-box]]
                  (and (not= x0 x1) (not= y0 y1)))
                empty-xy-box-coords+uv-boxes))
            ;; We need *some* geometry, or it will eventually result in invalid
            ;; buffers that will likely crash the GL driver. When all 9 boxes
            ;; are empty, use the center slice. This can happen with a manual
            ;; size of zero.
            [(empty-xy-box-coords+uv-boxes 4)])

        non-empty-xy-boxes (mapv (comp (partial geom/transl (pivot-offset pivot size))
                                       box-corner-coords->vertices3
                                       first)
                                 non-empty-xy-box-coords+uv-boxes)
        position-data (into []
                            (mapcat box->triangle-vertices)
                            non-empty-xy-boxes)
        uv-data (into []
                      (comp (map second)
                            (mapcat box->triangle-vertices))
                      non-empty-xy-box-coords+uv-boxes)
        line-data (into []
                        (mapcat (fn [box-vertices]
                                  (interleave box-vertices (drop 1 (cycle box-vertices)))))
                        non-empty-xy-boxes)]
    {:position-data position-data
     :uv-data uv-data
     :line-data line-data}))

;; Subdivide the same atlas-local Slice-9 cells used by the engine's opt-in
;; slice9_tiled material. Preserve the original editor outline and existing
;; UV transform; only the textured triangles need to be repeated.
(defn- interpolate-quad [bl tl tr br x y]
  (let [x (double x)
        y (double y)]
    (mapv (fn [^double a ^double b ^double c ^double d]
            (+ (* (- 1.0 y) (+ (* (- 1.0 x) a) (* x d)))
               (* y (+ (* (- 1.0 x) b) (* x c)))))
          bl tl tr br)))

(defn- segment [index count length period flipped]
  (let [index (long index)
        count (long count)
        length (double length)
        period (double period)]
    (if flipped
      (let [first-length (- length (* (dec count) period))
            start (if (zero? index) 0.0 (+ first-length (* (dec index) period)))
            end (if (zero? index) first-length (min length (+ first-length (* index period))))]
        [(/ start length) (/ end length) (if (zero? index) (/ first-length period) 1.0) 0.0])
      (let [start (* index period)
            end (min length (+ start period))]
        [(/ start length) (/ end length) 0.0 (/ (- end start) period)]))))

(defn oriented-margins
  "Arrange authored Slice-9 border thicknesses in displayed flipped directions."
  [[left top right bottom] flip-horizontal flip-vertical]
  [(if flip-horizontal right left)
   (if flip-vertical bottom top)
   (if flip-horizontal left right)
   (if flip-vertical top bottom)])

;; Mirror GetGuiRepeatAxis and GetGuiRepeatTileSegment in comp_gui.cpp. This
;; keeps the artist's zoom/phase authoring preview aligned with the runtime
;; allocator and its clipped first/last tiles, without wrapping atlas UVs.
(defn- repeat-axis [destination source zoom offset flipped]
  (let [destination (double destination)
        source (double source)
        zoom (double zoom)
        offset (double offset)
        step (* source zoom)]
    (when (and (> destination 0.001)
               (> source 0.001)
               (<= 0.01 zoom 256.0)
               (<= (Math/abs offset) 1000000.0)
               (> step 0.001)
               (<= step 100000000.0))
      (let [phase (mod (if flipped (+ destination offset) offset) step)
            first-span (if flipped
                         (if (> phase 0.00001) phase step)
                         (if (> phase 0.00001) (- step phase) step))
            ;; Match GetGuiRepeatAxis: sub-half-ULP remainder cannot form a
            ;; visible float vertex and must not trip the 2048-quad budget.
            tolerance (* 0.5 (Math/ulp (float 1.0)) destination)
            count (+ 1.0 (Math/ceil (/ (max 0.0 (- destination first-span tolerance)) step)))]
        (when (<= 1.0 count 2048.0)
          {:count (long count)
           :step step
           :first-span first-span})))))

(defn- repeat-segment [index destination step first-span flipped]
  (let [index (long index)
        destination (double destination)
        step (double step)
        first-span (double first-span)
        start (if (zero? index) 0.0 (+ first-span (* (dec index) step)))
        end (min destination (if (zero? index) first-span (+ first-span (* index step))))
        fraction (/ (- end start) step)
        u0 (if flipped (if (zero? index) (/ first-span step) 1.0)
               (if (zero? index) (- 1.0 (/ first-span step)) 0.0))]
    [(/ start destination) (/ end destination) u0 (if flipped (- u0 fraction) (+ u0 fraction))]))

(defn tiled-vertex-data
  "Preview opt-in box repeat without repeating the entire atlas.
  Retains legacy stretch for invalid dimensions and over-budget node sizes."
  [{:keys [width height tex-coords flip-horizontal flip-vertical use-geometries repeat-zoom repeat-offset]} size slice9
   {:keys [position-data uv-data] :as base}]
  (let [[^double node-width ^double node-height] size
        [^double left ^double top ^double right ^double bottom] slice9
        no-margins (not (sliced? slice9))
        source-x (- (double (or width 0)) left right)
        source-y (- (double (or height 0)) top bottom)
        center-x (- node-width left right)
        center-y (- node-height top bottom)
        [zoom-x zoom-y] (or repeat-zoom [1.0 1.0])
        [offset-x offset-y] (or repeat-offset [0.0 0.0])
        x-axis (repeat-axis center-x source-x zoom-x offset-x flip-horizontal)
        y-axis (repeat-axis center-y source-y zoom-y offset-y flip-vertical)
        x-count (or (:count x-axis) 0)
        y-count (or (:count y-axis) 0)
        animated-phase (or (not= 1.0 (double zoom-x))
                           (not= 1.0 (double zoom-y))
                           (not= 0.0 (double offset-x))
                           (not= 0.0 (double offset-y)))]
    (if (or (nil? tex-coords)
            (and no-margins (or use-geometries (not= 6 (count position-data))))
            (nil? x-axis)
            (nil? y-axis)
            (not (> source-x 0.001))
            (not (> source-y 0.001))
            (not (> center-x 0.001))
            (not (> center-y 0.001))
            (not (<= 1.0 x-count (if no-margins 2048.0 2046.0)))
            (not (<= 1.0 y-count (if no-margins 2048.0 2046.0)))
            (> (* (+ x-count (if no-margins 0.0 2.0))
                  (+ y-count (if no-margins 0.0 2.0))) 2048.0))
      base
      (let [min-x (double (transduce (map first) min Double/POSITIVE_INFINITY position-data))
            min-y (double (transduce (map second) min Double/POSITIVE_INFINITY position-data))
            center-x0 (+ min-x left)
            center-y0 (+ min-y bottom)
            triangle-count (quot (count position-data) 6)
            [tiled-positions tiled-uvs]
            (reduce
              (fn [[out-positions out-uvs] ^long quad-index]
                (let [offset (* quad-index 6)
                      ;; The six-vertex frame quad order is NW NE SW NE SE SW;
                      ;; the nine-slice triangle order is BL TL BR BR TL TR.
                      indices (if no-margins [0 2 1 4] [0 1 2 5])
                      [bl tl br tr] (mapv (fn [^long vertex-index] (nth position-data (+ offset vertex-index))) indices)
                      [uv-bl uv-tl uv-br uv-tr] (mapv (fn [^long vertex-index] (nth uv-data (+ offset vertex-index))) indices)
                      tile-x (and (< (Math/abs (- (double (first bl)) center-x0)) 0.0001)
                                  (< (Math/abs (- (double (first br)) (+ center-x0 center-x))) 0.0001))
                      tile-y (and (< (Math/abs (- (double (second bl)) center-y0)) 0.0001)
                                  (< (Math/abs (- (double (second tl)) (+ center-y0 center-y))) 0.0001))
                      x-steps (long (if tile-x x-count 1))
                      y-steps (long (if tile-y y-count 1))]
                  (reduce
                    (fn [out ^long yi]
                      (reduce
                        (fn [[positions uvs] ^long xi]
                          (let [[px0 px1 ux0 ux1] (if tile-x
                                                    (if animated-phase
                                                      (repeat-segment xi center-x (:step x-axis) (:first-span x-axis) flip-horizontal)
                                                      (segment xi x-steps center-x source-x flip-horizontal))
                                                    [0.0 1.0 0.0 1.0])
                                [py0 py1 uy0 uy1] (if tile-y
                                                    (if animated-phase
                                                      (repeat-segment yi center-y (:step y-axis) (:first-span y-axis) flip-vertical)
                                                      (segment yi y-steps center-y source-y flip-vertical))
                                                    [0.0 1.0 0.0 1.0])
                                quad-position (mapv (fn [[x y]] (interpolate-quad bl tl tr br x y))
                                                    [[px0 py0] [px0 py1] [px1 py1] [px1 py0]])
                                quad-uv (mapv (fn [[u v]] (interpolate-quad uv-bl uv-tl uv-tr uv-br
                                                                 (if (and tile-x flip-horizontal) (- 1.0 u) u)
                                                                 (if (and tile-y flip-vertical) (- 1.0 v) v)))
                                              [[ux0 uy0] [ux0 uy1] [ux1 uy1] [ux1 uy0]])]
                            [(into positions (map quad-position) box-triangles-vertex-order)
                             (into uvs (map quad-uv) box-triangles-vertex-order)]))
                        out
                        (range x-steps)))
                    [out-positions out-uvs]
                    (range y-steps))))
              [[] []]
              (range triangle-count))]
        (assoc base :position-data tiled-positions :uv-data tiled-uvs)))))

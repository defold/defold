;; Copyright 2020-2022 The Defold Foundation
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
  (:require [editor.geom :as geom]))

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

(defn- pivot->h-align [pivot]
  (case pivot
    (:pivot-e :pivot-ne :pivot-se) :right
    (:pivot-center :pivot-n :pivot-s) :center
    (:pivot-w :pivot-nw :pivot-sw) :left))

(defn- pivot->v-align [pivot]
  (case pivot
    (:pivot-ne :pivot-n :pivot-nw) :top
    (:pivot-e :pivot-center :pivot-w) :middle
    (:pivot-se :pivot-s :pivot-sw) :bottom))

(defn- pivot-offset [pivot size]
  (let [h-align (pivot->h-align pivot)
        v-align (pivot->v-align pivot)
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
  [{:keys [width height tex-coords] :as _frame} size slice9 pivot]
  (let [^double texture-width (or width 1.0)
        ^double texture-height (or height 1.0)
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
        [^double box-width ^double box-height _] size
        x-steps [0.0 ^double (get slice9 0) (- box-width ^double (get slice9 2)) box-width]
        y-steps [0.0 ^double (get slice9 3) (- box-height ^double (get slice9 1)) box-height]
        xy-box-coords (ranges->box-corner-coords (steps->ranges x-steps) (steps->ranges y-steps))
        non-empty-xy-box-coords+uv-boxes (into []
                                               (filter (fn [[[x0 y0 x1 y1] _uv-box]]
                                                         (and (not= x0 x1) (not= y0 y1))))
                                               (map vector xy-box-coords uv-boxes))
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

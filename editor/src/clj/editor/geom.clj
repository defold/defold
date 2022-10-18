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

(ns editor.geom
  (:require [schema.core :as s]
            [editor.types :as types]
            [editor.math :as math]
            [internal.util :as util])
  (:import [com.defold.util Geometry]
           [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]
           [editor.types AABB Frustum Rect]
           [javax.vecmath Matrix3d Matrix4d Point2d Point3d Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(defn clamper [low high] (fn [x] (min (max x low) high)))

(defn lift-f1 [op] (fn [c xs] (into (empty xs) (for [x xs] (op c x)))))

(def *& (lift-f1 *))
(def +& (lift-f1 +))
(def -& (lift-f1 -))

; -------------------------------------
; 2D geometry
; -------------------------------------
(s/defn area :- double
  [r :- Rect]
  (if r
    (* (double (.width r)) (double (.height r)))
    0))

(s/defn intersect :- (s/maybe Rect)
  ([r :- Rect] r)
  ([r1 :- Rect r2 :- Rect]
    (when (and r1 r2)
      (let [l (max (.x r1) (.x r2))
            t (max (.y r1) (.y r2))
            r (min (+ (.x r1) (.width r1))  (+ (.x r2) (.width r2)))
            b (min (+ (.y r1) (.height r1)) (+ (.y r2) (.height r2)))
            w (- r l)
            h (- b t)]
        (if (and (< 0 w) (< 0 h))
          (types/rect l t w h)
          nil))))
  ([r1 :- Rect r2 :- Rect & rs :- [Rect]]
   (reduce intersect (intersect r1 r2) rs)))

(defn rect-empty? [^Rect r]
  (or (zero? (.width r))
      (zero? (.height r))))

(defn intersect? [^Rect r ^Rect r2]
  (let [no-intersection (or (< (+ (.x r) (.width r)) (.x r2))
                            (< (+ (.x r2) (.width r2)) (.x r))
                            (< (+ (.y r) (.height r)) (.y r2))
                            (< (+ (.y r2) (.height r2)) (.y r)))]
    (not no-intersection)))

(s/defn split-rect-| :- [Rect]
  "Splits the rectangle such that the side slices extend to the top and bottom"
  [^Rect container :- Rect ^Rect content :- Rect]
  (let [new-rects     (transient [])
        overlap ^Rect (intersect container content)]
    (if overlap
      (do
        ;; left slice
        (if (< (.x container) (.x overlap))
          (conj! new-rects (types/rect (.x container)
                                 (.y container)
                                 (- (.x overlap) (.x container))
                                 (.height container))))

        ;; right slice
        (if (< (+ (.x overlap) (.width overlap)) (+ (.x container) (.width container)))
          (conj! new-rects (types/rect ""
                                (+ (.x overlap) (.width overlap))
                                (.y container)
                                (- (+ (.x container) (.width container))
                                    (+ (.x overlap)   (.width overlap)))
                                (.height container))))
        ;; bottom slice
        (if (< (.y container) (.y overlap))
          (conj! new-rects (types/rect ""
                                 (.x overlap)
                                 (.y container)
                                 (.width overlap)
                                 (- (.y overlap) (.y container)))))

        ;; top slice
        (if (< (+ (.y overlap) (.height overlap)) (+ (.y container) (.height container)))
          (conj! new-rects (types/rect ""
                                 (.x overlap)
                                 (+ (.y overlap) (.height overlap))
                                 (.width overlap)
                                 (- (+ (.y container) (.height container))
                                    (+ (.y overlap)   (.height overlap)))))))
      (conj! new-rects container))
    (persistent! new-rects)))

(s/defn split-rect-= :- [Rect]
  "Splits the rectangle such that the top and bottom slices extend to the sides"
  [^Rect container :- Rect ^Rect content :- Rect]
  (let [new-rects     (transient [])
        overlap ^Rect (intersect container content)]
    (if overlap
      (do
         ;; bottom slice
         (if (< (.y container) (.y overlap))
           (conj! new-rects (types/rect ""
                                  (.x container)
                                  (.y container)
                                  (.width container)
                                  (- (.y overlap) (.y container)))))

         ;; top slice
         (if (< (+ (.y overlap) (.height overlap)) (+ (.y container) (.height container)))
           (conj! new-rects (types/rect ""
                                  (.x container)
                                  (+ (.y overlap) (.height overlap))
                                  (.width container)
                                  (- (+ (.y container) (.height container))
                                    (+ (.y overlap) (.height overlap))))))

         ;; left slice
         (if (< (.x container) (.x overlap))
           (conj! new-rects (types/rect ""
                                  (.x container)
                                  (.y overlap)
                                  (- (.x overlap) (.x container))
                                  (.height overlap))))

         ;; right slice
         (if (< (+ (.x overlap) (.width overlap)) (+ (.x container) (.width container)))
           (conj! new-rects (types/rect ""
                                 (+ (.x overlap) (.width overlap))
                                 (.y overlap)
                                 (- (+ (.x container) (.width container))
                                     (+ (.x overlap)   (.width overlap)))
                                 (.height overlap)))))
      (conj! new-rects container))
    (persistent! new-rects)))

(defn- largest-area
  [rs]
  (reduce max 0 (map area rs)))

(s/defn split-rect :- [Rect]
  "Splits the rectangle with an attempt to minimize fragmentation"
  [^Rect container :- Rect ^Rect content :- Rect]
  (let [horizontal (split-rect-= container content)
        vertical   (split-rect-| container content)]
    (if (> (largest-area horizontal) (largest-area vertical))
      horizontal
      vertical)))

; This is off-by-one in many cases, due to Clojure's preference to promote things into Double and Long.
;
;(defn to-short-uv
;  "Return a fixed-point integer representation of the fractional part of the given fuv."
;  [^Float fuv]
;  (.shortValue
;    (bit-and
;      (int
;        (* (float fuv) (.floatValue 65535.0)))
;      0xffff)))

(defn to-short-uv
  [^Float fuv]
  (Geometry/toShortUV fuv))

; -------------------------------------
; Transformations
; -------------------------------------

(s/defn world-space [node :- {:world-transform Matrix4d s/Any s/Any} point :- Point3d]
  (let [p             (Point3d. point)
        tfm ^Matrix4d (:world-transform node)]
    (.transform tfm p)
    p))

(def ^Quat4d NoRotation (Quat4d. 0.0 0.0 0.0 1.0))
(def ^Matrix3d Identity3d (doto (Matrix3d.) (.setIdentity)))
(def ^Matrix4d Identity4d (doto (Matrix4d.) (.setIdentity)))

; -------------------------------------
; Matrix sloshing
; -------------------------------------
(defprotocol AsArray
  (^doubles as-array [this]))

(defprotocol Invertible
  (invert [this]))

(extend-type Matrix4d
  AsArray
  (as-array [this]
    (double-array [(.m00 this) (.m10 this) (.m20 this) (.m30 this)
                   (.m01 this) (.m11 this) (.m21 this) (.m31 this)
                   (.m02 this) (.m12 this) (.m22 this) (.m32 this)
                   (.m03 this) (.m13 this) (.m23 this) (.m33 this)]))

  Invertible
  (invert [this]
    (doto (Matrix4d. this)
      .invert)))

(extend-type Vector3d
  AsArray
  (as-array [v]
    (let [vals (double-array 3)]
      (.get v vals)
      vals)))

(extend-type Point3d
  AsArray
  (as-array [v]
    (let [vals (double-array 3)]
      (.get v vals)
      vals)))

(extend-type Vector4d
  AsArray
  (as-array [v]
    (let [vals (double-array 4)]
      (.get v vals)
      vals)))

(s/defn ident :- Matrix4d
  []
  (doto (Matrix4d.)
    (.setIdentity)))

; -------------------------------------
; 3D geometry
; -------------------------------------

(defrecord DoubleRange [^double min ^double max])

(defn- ranges-overlap? [^DoubleRange a ^DoubleRange b]
  (and (<= (.min b) (.max a))
       (<= (.min a) (.max b))))

(defn- project-points-on-axis
  ^DoubleRange [points normalized-axis]
  (loop [points points
         minimum Double/POSITIVE_INFINITY
         maximum Double/NEGATIVE_INFINITY]
    (if-some [point (first points)]
      (let [dot-product (math/dot point normalized-axis)]
        (recur (next points)
               (min minimum dot-product)
               (max maximum dot-product)))
      (->DoubleRange minimum maximum))))

(defn- overlap-when-projected-on-axis? [a-points b-points normalized-axis]
  (ranges-overlap? (project-points-on-axis a-points normalized-axis)
                   (project-points-on-axis b-points normalized-axis)))

(defn sat-intersection?
  "Returns true if two convex geometries intersect based on the Separating Axis
  Theorem. Both objects must implement the SATIntersection protocol."
  [a b]
  (let [overlap-when-projected-on-axis?
        (partial overlap-when-projected-on-axis?
                 (types/points a)
                 (types/points b))]
    (and
      (every? overlap-when-projected-on-axis?
              (types/unique-face-normals a))
      (every? overlap-when-projected-on-axis?
              (types/unique-face-normals b))
      (every? overlap-when-projected-on-axis?
              (for [a-edge-normal (types/unique-edge-normals a)
                    b-edge-normal (types/unique-edge-normals b)]
                (math/cross a-edge-normal b-edge-normal))))))

(def ^AABB null-aabb (types/->AABB (Point3d. Integer/MAX_VALUE Integer/MAX_VALUE Integer/MAX_VALUE)
                                   (Point3d. Integer/MIN_VALUE Integer/MIN_VALUE Integer/MIN_VALUE)))

(defn null-aabb? [^AABB aabb]
  (= aabb null-aabb))

(defn make-aabb [^Point3d a ^Point3d b]
  (let [minx (Math/min (.x a) (.x b))
        miny (Math/min (.y a) (.y b))
        minz (Math/min (.z a) (.z b))
        maxx (Math/max (.x a) (.x b))
        maxy (Math/max (.y a) (.y b))
        maxz (Math/max (.z a) (.z b))]
    (types/->AABB (Point3d. minx miny minz)
                  (Point3d. maxx maxy maxz))))

(defn- coords->Point3d [coords]
  (Point3d. (double-array (if (= (count coords) 3) coords (conj coords 0)))))

(defn coords->aabb [min-coords max-coords]
  (make-aabb (coords->Point3d min-coords)
             (coords->Point3d max-coords)))

(s/defn aabb-incorporate :- AABB
  ([^AABB aabb :- AABB
    ^Point3d p :- Point3d]
    (aabb-incorporate aabb (.x p) (.y p) (.z p)))
  ([^AABB aabb :- AABB
    ^double x :- s/Num
    ^double y :- s/Num
    ^double z :- s/Num]
    (let [minx (Math/min (-> aabb types/min-p .x) x)
          miny (Math/min (-> aabb types/min-p .y) y)
          minz (Math/min (-> aabb types/min-p .z) z)
          maxx (Math/max (-> aabb types/max-p .x) x)
          maxy (Math/max (-> aabb types/max-p .y) y)
          maxz (Math/max (-> aabb types/max-p .z) z)]
      (types/->AABB (Point3d. minx miny minz)
                    (Point3d. maxx maxy maxz)))))

(s/defn aabb-union :- AABB
  ([aabb1 :- AABB] aabb1)
  ([aabb1 :- AABB aabb2 :- AABB]
   (cond
     (null-aabb? aabb1) aabb2
     (null-aabb? aabb2) aabb1
     :else (-> aabb1
               (aabb-incorporate (types/min-p aabb2))
               (aabb-incorporate (types/max-p aabb2)))))
  ([aabb1 :- AABB aabb2 :- AABB & aabbs :- [AABB]]
   (aabb-union (aabb-union aabb1 aabb2) aabbs)))

(s/defn aabb-contains?
  [^AABB aabb :- AABB ^Point3d p :- Point3d]
  (and
    (>= (-> aabb types/max-p .x) (.x p) (-> aabb types/min-p .x))
    (>= (-> aabb types/max-p .y) (.y p) (-> aabb types/min-p .y))
    (>= (-> aabb types/max-p .z) (.z p) (-> aabb types/min-p .z))))

(s/defn aabb-extent :- Point3d
  [aabb :- AABB]
  (let [v (Point3d. (types/max-p aabb))]
    (.sub v (types/min-p aabb))
    v))

(s/defn aabb-center :- Point3d
  [^AABB aabb :- AABB]
  (Point3d. (/ (+ (-> aabb types/min-p .x) (-> aabb types/max-p .x)) 2.0)
            (/ (+ (-> aabb types/min-p .y) (-> aabb types/max-p .y)) 2.0)
            (/ (+ (-> aabb types/min-p .z) (-> aabb types/max-p .z)) 2.0)))

(s/defn rect->aabb :- AABB
  [^Rect bounds :- Rect]
  (assert bounds "rect->aabb require boundaries")
  (let [x0 (.x bounds)
        y0 (.y bounds)
        x1 (+ x0 (.width bounds))
        y1 (+ y0 (.height bounds))]
    (make-aabb (Point3d. x0 y0 0)
               (Point3d. x1 y1 0))))

(s/defn centered-rect->aabb :- AABB
  [^Rect centered-rect :- Rect]
  (let [x1 (.x centered-rect)
        y1 (.y centered-rect)
        x2 (+ x1 (* 0.5 (.width centered-rect)))
        y2 (+ y1 (* 0.5 (.height centered-rect)))
        x1 (- x1 (* 0.5 (.width centered-rect)))
        y1 (- y1 (* 0.5 (.height centered-rect)))]
    (make-aabb (Point3d. x1 y1 0)
               (Point3d. x2 y2 0))))

(def empty-bounding-box (coords->aabb [0 0 0] [0 0 0]))

(defn empty-aabb? [^AABB aabb]
  (let [min-p ^Point3d (.min aabb)
        max-p ^Point3d (.max aabb)]
    (and (= (.x min-p) (.x max-p))
         (= (.y min-p) (.y max-p))
         (= (.z min-p) (.z max-p)))))

;; From Graphics Gems:
;; https://github.com/erich666/GraphicsGems/blob/master/gems/TransBox.c
(defn aabb-transform [^AABB aabb ^Matrix4d transform]
  (if (null-aabb? aabb)
    aabb
    (let [min-p  (as-array (.min aabb))
          max-p  (as-array (.max aabb))
          min-p' (double-array [(.m03 transform) (.m13 transform) (.m23 transform)])
          max-p' (double-array [(.m03 transform) (.m13 transform) (.m23 transform)])]
      (loop [i 0, j 0]
        (when (< i 3)
          (if (< j 3)
            (let [av (* (.getElement transform i j) (aget min-p j))
                  bv (* (.getElement transform i j) (aget max-p j))]
              (if (< av bv)
                (do
                  (aset ^doubles min-p' i (+ (aget min-p' i) av))
                  (aset ^doubles max-p' i (+ (aget max-p' i) bv)))
                (do
                  (aset ^doubles min-p' i (+ (aget min-p' i) bv))
                  (aset ^doubles max-p' i (+ (aget max-p' i) av))))
              (recur i (inc j)))
            (recur (inc i) 0))))
      (types/->AABB (Point3d. min-p') (Point3d. max-p')))))

(defn aabb->corners [^AABB aabb]
  (types/points aabb))

(defn aabb-frustum-test-coarse [^AABB aabb ^Frustum frustum]
  (let [frustum-planes (vals (.planes frustum))
        box-corners (aabb->corners aabb)]
    (reduce (fn [coarse-test-result frustum-plane]
              (let [num-box-corners-in-front-of-frustum-plane
                    (util/count-where #(math/in-front-of-plane? frustum-plane %)
                                      box-corners)]
                (case num-box-corners-in-front-of-frustum-plane
                  8 (reduced :fully-outside-frustum)
                  0 coarse-test-result
                  :partially-outside-frustum)))
            :fully-inside-frustum
            frustum-planes)))

(defn aabb-inside-frustum? [^AABB aabb ^Frustum frustum]
  (case (aabb-frustum-test-coarse aabb frustum)
    :fully-inside-frustum true
    :fully-outside-frustum false
    :partially-outside-frustum (sat-intersection? aabb frustum)))

(s/defn corners->frustum :- Frustum
  [near-tl :- Point3d
   near-tr :- Point3d
   near-bl :- Point3d
   near-br :- Point3d
   far-tl :- Point3d
   far-tr :- Point3d
   far-bl :- Point3d
   far-br :- Point3d]
  (let [near (math/plane-from-points near-tl near-bl near-br)
        far (math/plane-from-points far-tl far-tr far-br)
        top (math/plane-from-points near-tl near-tr far-tr)
        right (math/plane-from-points near-tr near-br far-br)
        bottom (math/plane-from-points near-br near-bl far-bl)
        left (math/plane-from-points near-bl near-tl far-tl)
        corners (types/->FrustumCorners near-tl near-bl near-br near-tr far-tl far-bl far-br far-tr)
        planes (types/->FrustumPlanes near far top right bottom left)
        orthographic? (> math/epsilon-sq
                         (Math/abs (- (.distanceSquared near-tl near-tr)
                                      (.distanceSquared far-tl far-tr))))
        unique-face-normals (cond-> [(math/plane-normal near)
                                     (math/plane-normal top)
                                     (math/plane-normal right)]
                                    (not orthographic?) (into [(math/plane-normal bottom)
                                                               (math/plane-normal left)]))
        unique-edge-normals (if orthographic?
                              unique-face-normals
                              [(math/edge-normal near-tl near-tr)
                               (math/edge-normal near-bl near-tl)
                               (math/edge-normal near-tl far-tl)
                               (math/edge-normal near-tr far-tr)
                               (math/edge-normal near-bl far-bl)
                               (math/edge-normal near-br far-br)])]
    (types/->Frustum corners planes unique-edge-normals unique-face-normals)))

; -------------------------------------
; Primitive shapes as vertex arrays
; -------------------------------------


(s/defn unit-sphere-pos-nrm [lats longs]
  (for [lat-i (range lats)
       long-i (range longs)]
   (let [lat-angle   (fn [rate] (* Math/PI rate))
         long-angle  (fn [rate] (* (* 2 Math/PI) rate))
         make-vertex (fn [lat-a long-a]
                       (let [y      (Math/cos lat-a)
                             radius (Math/sin lat-a)
                             x      (* radius (Math/cos long-a))
                             z      (* radius (Math/sin long-a))]
                         [x y z x y z]))
         vertices (for [lat-idx  [lat-i (inc lat-i)]
                        long-idx [long-i (inc long-i)]]
                    (make-vertex (lat-angle (/ lat-idx lats)) (long-angle (/ long-idx longs))))]
                (map #(nth vertices %) [0 1 2 1 3 2]))))

; Procedural geometry

(defn transl
  ([delta]
    (map (partial apply transl delta)))
  ([delta ps]
    (let [res (mapv (fn [p] (mapv + delta p)) ps)]
      res)))

(defn scale
  ([factor]
    (map (partial apply scale factor)))
  ([factor ps]
    (mapv (fn [p] (mapv * factor p)) ps)))

(defn rotate
  ([euler]
    (map (partial apply rotate euler)))
  ([euler ps]
    (let [q (math/euler->quat euler)
          tmp-v (Vector3d.)]
      (let [res (mapv (fn [[^double x ^double y ^double z]]
                    (.set tmp-v x y z)
                    (let [v (math/rotate q tmp-v)]
                      [(.x v) (.y v) (.z v)])) ps)]
        res))))

(defn transf-p
  [^Matrix4d m4d ps]
  (let [p (Point3d.)]
    (let [res (mapv (fn [[^double x ^double y ^double z]]
                      (.set p x y z)
                      (.transform m4d p)
                      [(.x p) (.y p) (.z p)]) ps)]
      res)))

(defn transf-p4
  [^Matrix4d m4d ps]
  (let [p (Point3d.)]
    (let [res (mapv (fn [[^double x ^double y ^double z]]
                      (.set p x y z)
                      (.transform m4d p)
                      [(.x p) (.y p) (.z p) 1.0])
                    ps)]
      res)))

(defn chain [n f ps]
  (loop [i n
         v ps
         result ps]
    (if (> i 0)
      (let [v' (f v)]
        (recur (dec i) v' (into result v')))
      result)))

(def empty-geom [])
(def origin-geom [[0 0 0]])

(defn circling [segments ps]
  (let [angle (/ 360 segments)]
    (chain (dec segments) (partial rotate [0 0 angle]) ps)))

(defn identity-uv-trans
  []
  (TextureSetGenerator$UVTransform.))

(defn uv-trans [^TextureSetGenerator$UVTransform uv-trans ps]
  (if uv-trans
    (let [p (Point2d.)]
     (mapv (fn [[^double x ^double y]]
             (.set p x y)
             (.apply uv-trans p)
             [(.x p) (.y p)]) ps))
    ps))

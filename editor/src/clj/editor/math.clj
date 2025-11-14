;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.math
  (:require [editor.util :as eutil]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [java.lang Math]
           [java.math RoundingMode]
           [javax.vecmath Matrix3d Matrix3f Matrix4d Matrix4f Point3d Quat4d SingularMatrixException Tuple2d Tuple3d Tuple4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:const epsilon 0.000001)
(def ^:const epsilon-sq (* epsilon epsilon))

(def ^:const pi Math/PI)
(def ^:const half-pi (* 0.5 pi))
(def ^:const two-pi (* 2.0 pi))
(def ^:const recip-180 (/ 1.0 180.0))
(def ^:const recip-pi (/ 1.0 Math/PI))

(def ^:const precision-general 0.000001)
(def ^:const precision-coarse 0.001)

(def ^Vector3d zero-v3 (Vector3d. 0.0 0.0 0.0))
(def ^Vector3d one-v3 (Vector3d. 1.0 1.0 1.0))
(def ^Quat4d identity-quat (Quat4d. 0.0 0.0 0.0 1.0))
(def ^Matrix4d identity-mat4 (doto (Matrix4d.) (.setIdentity)))

(definline float32? [value]
  `(instance? Float ~value))

(defn deg->rad
  ^double [^double deg]
  (* deg Math/PI recip-180))

(defn rad->deg
  ^double [^double rad]
  (* rad 180.0 recip-pi))

(defn median [numbers]
  (let [sorted-numbers (sort numbers)
        count (count sorted-numbers)]
    (if (zero? count)
      (throw (ex-info "Cannot calculate the median of an empty sequence." {:numbers numbers}))
      (let [middle-index (quot count 2)
            middle-number (nth sorted-numbers middle-index)]
        (if (odd? count)
          middle-number
          (-> middle-number
              (+ (nth sorted-numbers (dec middle-index)))
              (/ 2.0)))))))

(defn round-with-precision
  "Slow but precise rounding to a specified precision. Use with UI elements that
  produce doubles, not for performance-sensitive code. The precision is expected
  to be a double in the format 0.01, which would round 1.666666 to 1.67."
  ([^double n ^double precision]
   (round-with-precision n precision RoundingMode/HALF_UP))
  ([^double n ^double precision ^RoundingMode rounding-mode]
   (.doubleValue (.setScale (BigDecimal. n)
                            (int (- (Math/log10 precision)))
                            rounding-mode))))

(defn zip-clj-v3
  "Takes a three-element Clojure vector and returns a new three-element Clojure
  vector whose components are the result of applying component-fn to both
  components of a and b. Supports a Clojure vector, a javax.vecmath.Tuple3d, or
  a number for the second argument. If the second argument is a number, it will
  be combined with every component from the first vector."
  [a b component-fn]
  {:pre [(vector? a)
         (= 3 (count a))]}
  (cond
    (instance? Tuple3d b)
    (-> (coll/empty-with-meta a)
        (conj (component-fn (a 0) (.getX ^Tuple3d b)))
        (conj (component-fn (a 1) (.getY ^Tuple3d b)))
        (conj (component-fn (a 2) (.getZ ^Tuple3d b))))

    (vector? b)
    (-> (coll/empty-with-meta a)
        (conj (component-fn (a 0) (b 0)))
        (conj (component-fn (a 1) (b 1)))
        (conj (component-fn (a 2) (b 2))))

    (number? b)
    (-> (coll/empty-with-meta a)
        (conj (component-fn (a 0) b))
        (conj (component-fn (a 1) b))
        (conj (component-fn (a 2) b)))

    :else
    (throw (ex-info "Second argument must be a number or a vector type."
                    {:b b
                     :type (type b)}))))

(defn project [^Vector3d from ^Vector3d onto] ^Double
  (let [onto-dot (.dot onto onto)]
    (assert (> (.dot onto onto) 0.0))
    (/ (.dot from onto) onto-dot)))

(defn project-lines [^Point3d from-pos ^Vector3d from-dir ^Point3d onto-pos ^Vector3d onto-dir] ^Point3d
  (let [w (doto (Vector3d.) (.sub onto-pos from-pos))
        a (.dot onto-dir onto-dir) ;; always >= 0
        b (.dot onto-dir from-dir)
        c (.dot from-dir from-dir) ;; always >= 0
        d (.dot onto-dir w)
        e (.dot from-dir w)
        D (- (* a c) (* b b)) ;; always >= 0
        sc (if (< D 0.000001) 0.0 (/ (- (* b e) (* c d)) D))]
    (doto (Vector3d. onto-dir) (.scaleAdd sc onto-pos))))

; Implementation based on:
; http://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
; In case the line is parallel with the plane, nil is returned.
(defn line-plane-intersection
  "Returns the position at which the line intersects the plane. If the line and plane are parallel, nil is returned."
  [^Point3d line-pos ^Vector3d line-dir ^Point3d plane-pos ^Vector3d plane-normal]
  (let [norm-dir-dot (.dot line-dir plane-normal)]
    (when (> (Math/abs norm-dir-dot) epsilon-sq)
      (let [diff (doto (Vector3d.) (.sub line-pos plane-pos))
            t (- (/ (.dot diff plane-normal) norm-dir-dot))]
        (doto (Point3d. line-dir) (.scaleAdd t line-pos))))))

(defn project-line-circle [^Point3d line-pos ^Vector3d line-dir ^Point3d circle-pos ^Vector3d circle-axis ^Double radius] ^Point3d
  (if-let [intersection ^Point3d (line-plane-intersection line-pos line-dir circle-pos circle-axis)]
    (let [dist-sq (.distanceSquared intersection circle-pos)]
      (when (> dist-sq epsilon-sq)
        (Point3d. (doto (Vector3d. intersection) (.sub circle-pos) (.normalize) (.scaleAdd radius circle-pos)))))
    (let [closest ^Point3d (project-lines circle-pos circle-axis line-pos line-dir)
          plane-dist (project (doto (Vector3d. closest) (.sub circle-pos)) circle-axis)
          closest (doto (Point3d. circle-axis) (.scaleAdd ^Double (- plane-dist) closest))
          radius-sq (* radius radius)
          dist-sq (.distanceSquared closest circle-pos)]
      (if (< dist-sq radius-sq)
        (doto (Point3d. line-dir) (.scaleAdd (- (Math/sqrt (- radius-sq dist-sq))) closest))
        (Point3d. (doto (Vector3d. closest) (.sub circle-pos) (.normalize) (.scaleAdd (Math/sqrt radius-sq) circle-pos)))))))

(defn euler->quat
  ^Quat4d [euler]
  ; Implementation based on:
  ; http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
  ; Rotation sequence: 231 (YZX)
  (let [[x y z] euler]
    (if (= 0.0 (double x) (double y))
      (let [ha (* 0.5 (deg->rad z))
            s (Math/sin ha)
            c (Math/cos ha)]
        (Quat4d. 0 0 s c))
      (let [[t1 t2 t3] (map deg->rad (map #(nth euler %) [1 2 0]))
            c1 (Math/cos (* t1 0.5))
            s1 (Math/sin (* t1 0.5))
            c2 (Math/cos (* t2 0.5))
            s2 (Math/sin (* t2 0.5))
            c3 (Math/cos (* t3 0.5))
            s3 (Math/sin (* t3 0.5))
            c1_c2 (* c1 c2)
            s2_s3 (* s2 s3)
            q (Quat4d. (+ (* s1 s2 c3) (* s3 c1_c2))
                       (+ (* s1 c2 c3) (* s2_s3 c1))
                       (+ (- (* s1 s3 c2)) (* s2 c1 c3))
                       (+ (- (* s1 s2_s3)) (* c1_c2 c3)))]
        (.normalize q)
        q))))

(defn euler-z->quat [angle]
  (let [ha (* (deg->rad angle) 0.5)
        s (Math/sin ha)
        c (Math/cos ha)]
    (Quat4d. 0.0 0.0 s c)))

(defn quat-components->euler [^double x ^double y ^double z ^double w]
  (if (= 0.0 x y)
    (let [ha (Math/atan2 z w)]
      [0.0 0.0 (rad->deg (* 2.0 ha))])
    (let [test (+ (* x y) (* z w))]
      (cond
        (or (> test 0.499) (< test -0.499)) ; singularity at north pole
        (let [sign (Math/signum test)
              heading (* sign 2.0 (Math/atan2 x w))
              attitude (* sign Math/PI 0.5)
              bank 0.0]
          [(rad->deg bank) (rad->deg heading) (rad->deg attitude)])

        :default
        (let [sqx (* x x)
              sqy (* y y)
              sqz (* z z)
              heading (Math/atan2 (- (* 2.0 y w) (* 2.0 x z)) (- 1.0 (* 2.0 sqy) (* 2.0 sqz)))
              attitude (Math/asin (* 2.0 test))
              bank (Math/atan2 (- (* 2.0 x w) (* 2.0 y z)) (- 1.0 (* 2.0 sqx) (* 2.0 sqz)))]
          [(rad->deg bank) (rad->deg heading) (rad->deg attitude)])))))

(defn quat->euler [^Quat4d quat]
  (quat-components->euler (.getX quat) (.getY quat) (.getZ quat) (.getW quat)))

(defn clj-quat->euler [[^double x ^double y ^double z ^double w]]
  (quat-components->euler x y z w))

(defn float-array-distance-sq
  ^double [^floats a ^floats b]
  (let [ax (aget a 0)
        ay (aget a 1)
        az (aget a 2)
        bx (aget b 0)
        by (aget b 1)
        bz (aget b 2)
        dx (- bx ax)
        dy (- by ay)
        dz (- bz az)]
    (+ (* dx dx)
       (+ (* dy dy)
          (* dz dz)))))

(defn float-array-distance
  ^double [^floats a ^floats b]
  (Math/sqrt (float-array-distance-sq a b)))

(defn offset-scaled
  ^Tuple3d [^Tuple3d original ^Tuple3d offset ^double scale-factor]
  (doto ^Tuple3d (.clone original) ; Overwritten - we just want an instance.
    (.scaleAdd scale-factor offset original)))

(defn rotate
  ^Vector3d [^Quat4d rotation ^Vector3d v]
  (let [q (doto (Quat4d.) (.set (Vector4d. v)))
        _ (.mul q rotation q)
        _ (.mulInverse q rotation)]
    (Vector3d. (.getX q) (.getY q) (.getZ q))))

(defn transform
  ^Vector3d [^Matrix4d mat ^Vector3d v]
  (let [v' (Point3d. v)]
    (.transform mat v')
    (Vector3d. v')))

(defn transform-vector
  ^Vector3d [^Matrix4d mat ^Vector3d v]
  (let [v' (Vector3d. v)]
    (.transform mat v')
    v'))

(defn transform-vector-v4
  ^Vector4d [^Matrix4d mat ^Vector4d v]
  (let [v' (Vector4d. v)]
    (.transform mat v')
    v'))

(defn round-vector
  ^Vector3d [^Vector3d v]
  (Vector3d. (Math/round (.getX v))
             (Math/round (.getY v))
             (Math/round (.getZ v))))

(defn scale-vector
  ^Vector3d [^Vector3d v ^double n]
  (doto (Vector3d. v)
    (.scale n)))

(defn add-vector
  ^Vector3d [^Tuple3d v1 ^Tuple3d v2]
  (doto (Vector3d. v1)
    (.add v2)))

(defn subtract-vector
  ^Vector3d [^Tuple3d v1 ^Tuple3d v2]
  (doto (Vector3d. v1)
    (.sub v2)))

(defn multiply-vector
  ^Vector3d [^Vector3d v1 ^Vector3d v2]
  (Vector3d. (* (.-x v1) (.-x v2))
             (* (.-y v1) (.-y v2))
             (* (.-z v1) (.-z v2))))

(defn cross
  ^Vector3d [^Vector3d u ^Vector3d v]
  (doto (Vector3d.)
    (.cross u v)))

(defn dot
  "Flexible dot product that works with any three-element tuples."
  ^double [^Tuple3d a ^Tuple3d b]
  (+ (* (.x a) (.x b))
     (* (.y a) (.y b))
     (* (.z a) (.z b))))

(defn dot-v4-point
  "Dot product between a four-element vector and a point."
  ^double [^Tuple4d v ^Tuple3d p]
  (+ (* (.x v) (.x p))
     (* (.y v) (.y p))
     (* (.z v) (.z p))
     (.w v)))

(defn min-point
  ^Point3d [^Point3d a ^Point3d b]
  (Point3d. (Double/min (.x a) (.x b))
            (Double/min (.y a) (.y b))
            (Double/min (.z a) (.z b))))

(defn max-point
  ^Point3d [^Point3d a ^Point3d b]
  (Point3d. (Double/max (.x a) (.x b))
            (Double/max (.y a) (.y b))
            (Double/max (.z a) (.z b))))

(defn plane-from-points
  "Construct an infinite plane from three points in space. The plane normal will
  point towards the observer when the points are supplied in counter-clockwise
  order. Returns a 4d vector where the x, y, z coordinates are the plane normal
  and w is the distance from the origin in the opposite direction of the normal."
  ^Vector4d [^Tuple3d a ^Tuple3d b ^Tuple3d c]
  (let [ab (subtract-vector b a)
        ac (subtract-vector c a)
        n (cross ab ac)
        len (.length n)]
    (when (zero? len)
      (throw (ArithmeticException. "A plane cannot be found since all three points are on a line.")))
    (.scale n (/ 1.0 len))
    (Vector4d. (.x n) (.y n) (.z n) (- (dot n a)))))

(def in-front-of-plane? (comp pos? dot-v4-point))

(defn plane-normal
  ^Vector3d [^Vector4d plane]
  (Vector3d. (.x plane) (.y plane) (.z plane)))

(defn edge-normal
  ^Vector3d [^Tuple3d from ^Tuple3d to]
  (doto (Vector3d.)
    (.sub to from)
    (.normalize)))

(defn translation
  ^Vector3d [^Matrix4d m]
  (let [ret (Vector3d.)]
    (.get m ret)
    ret))

(defn rotation
  ^Quat4d [^Matrix4d m]
  (let [ret (Quat4d.)]
    (.get m ret)
    ret))

(defn scale
  ^Vector3d [^Matrix4d m]
  (let [unrotated (doto (Matrix4d. m)
                    (.setRotation (Quat4d. 0.0 0.0 0.0 1.0)))]
    (Vector3d. (.-m00 unrotated) (.-m11 unrotated) (.-m22 unrotated))))

(defn inv-transform
  ([^Point3d position ^Quat4d rotation ^Point3d p]
   (let [q (doto (Quat4d. rotation) (.conjugate))
         p1 (doto (Point3d. p) (.sub position))]
     (rotate q p1)))
  ([^Quat4d rotation ^Quat4d q]
   (let [q1 (doto (Quat4d. rotation) (.conjugate))]
     (.mul q1 q)
     q1)))

(defn from-to->quat [^Vector3d unit-from ^Vector3d unit-to]
  (let [dot (.dot unit-from unit-to)]
    (let [cos-half (Math/sqrt (* 2.0 (+ 1.0 dot)))
          recip-cos-half (/ 1.0 cos-half)
          axis (doto (Vector3d.) (.cross unit-from unit-to) (.scale recip-cos-half))]
      (doto (Quat4d. (.x axis) (.y axis) (.z axis) (* 0.5 cos-half))))))

(defn unit-axis [^long dimension-index] ^Vector3d
  (case dimension-index
    0 (Vector3d. 1 0 0)
    1 (Vector3d. 0 1 0)
    2 (Vector3d. 0 0 1)))

(defn ->mat4
  ^Matrix4d []
  (doto (Matrix4d.) (.setIdentity)))

(defn ->mat4-uniform
  ^Matrix4d [^Vector3d position ^Quat4d rotation ^double scale]
  (Matrix4d. rotation position scale))

(defn ->mat4-non-uniform
  ^Matrix4d [^Vector3d position ^Quat4d rotation ^Vector3d scale]
  (let [s (doto (Matrix3d.)
            (.setElement 0 0 (.x scale))
            (.setElement 1 1 (.y scale))
            (.setElement 2 2 (.z scale)))
        rs (doto (Matrix3d.)
             (.set rotation)
             (.mul s))]
    (doto (Matrix4d.)
      (.setRotationScale rs)
      (.setTranslation position)
      (.setElement 3 3 1.0))))

(defn ->mat4-scale
  (^Matrix4d [scale]
   (cond
     (vector? scale)
     (->mat4-scale (scale 0) (scale 1) (scale 2))

     (instance? Vector3d scale)
     (->mat4-scale (.x ^Vector3d scale) (.y ^Vector3d scale) (.z ^Vector3d scale))

     (number? scale)
     (->mat4-scale scale scale scale)

     :else
     (throw (ex-info (str "Invalid argument:" scale)
                     {:argument scale}))))
  (^Matrix4d [^double x-scale ^double y-scale ^double z-scale]
   (doto (Matrix4d.)
     (.setElement 0 0 x-scale)
     (.setElement 1 1 y-scale)
     (.setElement 2 2 z-scale)
     (.setElement 3 3 1.0))))

(defn split-mat4 [^Matrix4d mat ^Tuple3d out-position ^Quat4d out-rotation ^Vector3d out-scale]
  (let [tmp (Vector4d.)
        _ (.getColumn mat 3 tmp)
        _ (.set out-position (.getX tmp) (.getY tmp) (.getZ tmp))
        tmp (Vector3d.)
        mat3 (Matrix3d.)
        _ (.getRotationScale mat mat3)
        scale (double-array 3)]
    (doseq [^long col (range 3)]
      (.getColumn mat3 col tmp)
      (let [s (.length tmp)
            ^Vector3d axis (if (> s epsilon)
                             (doto tmp (.scale (/ 1.0 s)))
                             (unit-axis col))]
        (aset scale col s)
        (.setColumn mat3 col axis))
      (.set out-rotation mat3)
      (.set out-scale scale))))

(defn inverse
  "Calculate the inverse of a matrix."
  ^Matrix4d [^Matrix4d mat]
  (doto (Matrix4d.)
    (.invert mat)))

(defn affine-inverse
  "Efficiently calculate the inverse of an affine matrix.
  Warning: You cannot use this with scaled matrices."
  ^Matrix4d [^Matrix4d mat]
  (let [t (Vector3d.)
        rs (Matrix3d.)]
    (.get mat t)
    (.getRotationScale mat rs)
    (.transpose rs)
    (.negate t)
    (.transform rs t)
    (doto (Matrix4d.) (.set t) (.setRotationScale rs))))

(defprotocol VecmathConverter
  (clj->vecmath [this v])
  (vecmath->clj [this])
  (vecmath-into-clj [this dest]))

(extend-protocol VecmathConverter
  Tuple2d
  (clj->vecmath [this v] (.set this (nth v 0) (nth v 1)))
  (vecmath->clj [this] [(.getX this) (.getY this)])
  (vecmath-into-clj [this dest] (-> dest (conj (.getX this)) (conj (.getY this))))
  Tuple3d
  (clj->vecmath [this v] (.set this (nth v 0) (nth v 1) (nth v 2)))
  (vecmath->clj [this] [(.getX this) (.getY this) (.getZ this)])
  (vecmath-into-clj [this dest] (-> dest (conj (.getX this)) (conj (.getY this)) (conj (.getZ this))))
  Tuple4d
  (clj->vecmath [this v] (.set this (nth v 0) (nth v 1) (nth v 2) (nth v 3)))
  (vecmath->clj [this] [(.getX this) (.getY this) (.getZ this) (.getW this)])
  (vecmath-into-clj [this dest] (-> dest (conj (.getX this)) (conj (.getY this)) (conj (.getZ this)) (conj (.getW this))))
  Matrix4d
  (clj->vecmath [this v] (.set this (double-array v)))
  (vecmath->clj [this] [(.m00 this) (.m01 this) (.m02 this) (.m03 this)
                        (.m10 this) (.m11 this) (.m12 this) (.m13 this)
                        (.m20 this) (.m21 this) (.m22 this) (.m23 this)
                        (.m30 this) (.m31 this) (.m32 this) (.m33 this)])
  (vecmath-into-clj [this dest]
    (if (coll/supports-transient? dest)
      (-> (transient dest)
          (conj! (.m00 this)) (conj! (.m01 this)) (conj! (.m02 this)) (conj! (.m03 this))
          (conj! (.m10 this)) (conj! (.m11 this)) (conj! (.m12 this)) (conj! (.m13 this))
          (conj! (.m20 this)) (conj! (.m21 this)) (conj! (.m22 this)) (conj! (.m23 this))
          (conj! (.m30 this)) (conj! (.m31 this)) (conj! (.m32 this)) (conj! (.m33 this))
          (persistent!)
          (with-meta (meta dest)))
      (-> dest
          (conj (.m00 this)) (conj (.m01 this)) (conj (.m02 this)) (conj (.m03 this))
          (conj (.m10 this)) (conj (.m11 this)) (conj (.m12 this)) (conj (.m13 this))
          (conj (.m20 this)) (conj (.m21 this)) (conj (.m22 this)) (conj (.m23 this))
          (conj (.m30 this)) (conj (.m31 this)) (conj (.m32 this)) (conj (.m33 this))))))

(defn clj->mat4
  ^Matrix4d [position rotation scale]
  (if (and (nil? position)
           (nil? rotation)
           (nil? scale))
    identity-mat4
    (let [position-v3 (if (nil? position)
                        zero-v3
                        (doto (Vector3d.) (clj->vecmath position)))
          rotation-q4 (if (nil? rotation)
                        identity-quat
                        (doto (Quat4d.) (clj->vecmath rotation)))]
      (cond
        (nil? scale)
        (->mat4-uniform position-v3 rotation-q4 1.0)

        (number? scale)
        (->mat4-uniform position-v3 rotation-q4 (double scale))

        :else
        (let [scale-v3 (doto (Vector3d.) (clj->vecmath scale))]
          (->mat4-non-uniform position-v3 rotation-q4 scale-v3))))))

(defmacro hermite [y0 y1 t0 t1 t]
  `(let [t# ~t
         t2# (* t# t#)
         t3# (* t2# t#)]
     (+ (* (+ (* 2.0 t3#) (* -3.0 t2#) 1.0) ~y0)
        (* (+ t3# (* -2.0 t2#) t#) ~t0)
        (* (+ (* -2.0 t3#) (* 3.0 t2#)) ~y1)
        (* (- t3# t2#) ~t1))))

(defmacro hermite' [y0 y1 t0 t1 t]
  `(let [t# ~t
         t2# (* t# t#)]
     (+ (* (+ (* 6.0 t2#) (* -6.0 t#)) ~y0)
        (* (+ (* 3.0 t2#) (* -4.0 t#) 1.0) ~t0)
        (* (+ (* -6.0 t2#) (* 6.0 t#)) ~y1)
        (* (+ (* 3.0 t2#) (* -2.0 t#)) ~t1))))

(defn derive-normal-transform
  ^Matrix4d [^Matrix4d transform]
  (try
    (let [normal-transform (Matrix3d.)]
      (.getRotationScale transform normal-transform)
      (.invert normal-transform)
      (.transpose normal-transform)
      (doto (Matrix4d.)
        (.setRotationScale normal-transform)
        (.setM33 1.0)))
    (catch SingularMatrixException _
      identity-mat4)))

(defn derive-render-transforms
  "Given the world, view, projection, and texture transforms, derive the normal,
  view-proj, world-view, and world-view-proj transforms and return a map with
  all the resulting transforms."
  [^Matrix4d world ^Matrix4d view ^Matrix4d projection ^Matrix4d texture]
  ;; Matrix multiplication A * B = C is c.mul(a, b) in vecmath.
  ;; In-place A := A * B is a.mul(b).
  ;; The matrix naming is in the order the transforms will be applied to the
  ;; vertices. For instance, view-proj is "Proj * View", and
  ;; view-proj.transform(v) is (Proj * (View * V))
  (let [view-proj (doto (Matrix4d. projection) (.mul view))
        world-view (doto (Matrix4d. view) (.mul world))
        world-view-proj (doto (Matrix4d. view-proj) (.mul world))
        normal (derive-normal-transform world-view)]
    ;; Some of these may be overwritten by the rederive-render-transforms
    ;; function, so we include the actually derived transforms for all world
    ;; space transforms here as well.
    {:actual/world world
     :actual/world-view world-view
     :actual/world-view-proj world-view-proj
     :actual/normal normal
     :world world
     :view view
     :projection projection
     :texture texture
     :normal normal
     :view-proj view-proj
     :world-view world-view
     :world-view-proj world-view-proj}))

(defn rederive-render-transforms
  "Given a result from the derive-render-transforms function, returns a new
  map of transforms where the world transform contributions have been canceled
  out if all attributes of the affected semantic-type are in world-space. This
  is a compatibility hack that enables a vertex shader that applies these
  transforms to be shared among materials that require either world-space or
  local-space attributes.

  TODO:
  We should probably just deprecate this behavior, since it is likely to confuse
  users that mix local-space and world-space attributes of the same type."
  [derived-render-transforms coordinate-space-info]
  (let [world-space-semantic-types (:coordinate-space-world coordinate-space-info)
        local-space-semantic-types (:coordinate-space-local coordinate-space-info)
        has-world-space-position (contains? world-space-semantic-types :semantic-type-position)
        has-world-space-normal (contains? world-space-semantic-types :semantic-type-normal)
        has-local-space-position (contains? local-space-semantic-types :semantic-type-position)
        has-local-space-normal (contains? local-space-semantic-types :semantic-type-normal)
        {:keys [view view-proj]} derived-render-transforms]
    (cond-> derived-render-transforms

            (and has-world-space-position (not has-local-space-position))
            (assoc :world identity-mat4
                   :world-view view
                   :world-view-proj view-proj)

            (and has-world-space-normal (not has-local-space-normal))
            (assoc :normal (derive-normal-transform view)
                   :world-rotation identity-quat))))

(def render-transform-keys
  #{:actual/normal
    :actual/world
    :actual/world-rotation
    :actual/world-view
    :actual/world-view-proj
    :normal
    :projection
    :texture
    :view
    :view-proj
    :world
    :world-rotation
    :world-view
    :world-view-proj})

(fn/defamong render-transform-key? render-transform-keys)

(defn- vecmath-matrix-dim
  ^long [matrix]
  (condp instance? matrix
    Matrix3d 3
    Matrix3f 3
    Matrix4d 4
    Matrix4f 4))

(defmulti ^:private vecmath-matrix-row (fn [matrix ^long _row-index] (class matrix)))

(defmethod vecmath-matrix-row Matrix3d [^Matrix3d matrix ^long row-index]
  (let [row (double-array 3)]
    (.getRow matrix row-index row)
    row))

(defmethod vecmath-matrix-row Matrix3f [^Matrix3f matrix ^long row-index]
  (let [row (float-array 3)]
    (.getRow matrix row-index row)
    row))

(defmethod vecmath-matrix-row Matrix4d [^Matrix4d matrix ^long row-index]
  (let [row (double-array 4)]
    (.getRow matrix row-index row)
    row))

(defmethod vecmath-matrix-row Matrix4f [^Matrix4f matrix ^long row-index]
  (let [row (float-array 4)]
    (.getRow matrix row-index row)
    row))

(defn vecmath-matrix-pprint-strings [matrix]
  (let [dim (vecmath-matrix-dim matrix)
        fmt-num #(eutil/format* "%.3f" %)
        num-strs (coll/transfer (range dim) []
                   (mapcat (fn [^long row-index]
                             (let [row (vecmath-matrix-row matrix row-index)]
                               (map fmt-num row)))))
        first-col-width (transduce (comp (take-nth dim)
                                         (map count))
                                   max
                                   0
                                   num-strs)
        rest-col-width (transduce (map count)
                                  max
                                  0
                                  num-strs)
        first-col-width-fmt (str \% first-col-width \s)
        rest-col-width-fmt (str \% rest-col-width \s)
        fmt-col (fn [^long index num-str]
                  (let [fmt (if (zero? (rem index dim))
                              first-col-width-fmt
                              rest-col-width-fmt)]
                    (eutil/format* fmt num-str)))]
    (coll/transfer num-strs []
      (partition-all dim)
      (map (partial into [] (map-indexed fmt-col))))))

(defn zero-vecmath-matrix-col-str? [^String col-str]
  (let [last-index (.lastIndexOf col-str "0.000")]
    (case last-index
      -1 false
      0 true
      (case (.charAt col-str (dec last-index))
        (\space \-) true
        false))))

(ns editor.math
  (:import [java.lang Math]
           [java.math RoundingMode]
           [javax.vecmath Matrix3d Matrix4d Point3d Vector3d Vector4d Quat4d AxisAngle4d
            Tuple3d Tuple4d]))

(set! *warn-on-reflection* true)

(def epsilon 0.000001)
(def epsilon-sq (* epsilon epsilon))

(def pi Math/PI)
(def half-pi (* 0.5 pi))
(def two-pi (* 2.0 pi))
(def recip-180 (/ 1.0 180.0))
(def recip-pi (/ 1.0 Math/PI))

(defn deg->rad [deg]
  (* deg Math/PI recip-180))

(defn rad->deg [rad]
  (* rad 180.0 recip-pi))

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
(defn line-plane-intersection [^Point3d line-pos ^Vector3d line-dir ^Point3d plane-pos ^Vector3d plane-normal]
  "Returns the position at which the line intersects the plane. If the line and plane are parallel, nil is returned."
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

(defn euler->quat ^Quat4d [euler]
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

(defn ^Vector3d rotate [^Quat4d rotation ^Vector3d v]
  (let [q (doto (Quat4d.) (.set (Vector4d. v)))
        _ (.mul q rotation q)
        _ (.mulInverse q rotation)]
    (Vector3d. (.getX q) (.getY q) (.getZ q))))

(defn ^Vector3d transform-vector [^Matrix4d mat ^Vector3d v]
  (let [v' (Vector3d. v)]
    (.transform mat v')
    v'))

(defn round-vector
  ^Vector3d [^Vector3d v]
  (Vector3d. (Math/round (.getX v))
             (Math/round (.getY v))
             (Math/round (.getZ v))))

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

(defn ->mat4 ^Matrix4d []
  (doto (Matrix4d.) (.setIdentity)))

(defn ->mat4-uniform ^Matrix4d [^Vector3d position ^Quat4d rotation ^double scale]
  (Matrix4d. rotation position scale))

(defn ->mat4-non-uniform ^Matrix4d [^Vector3d position ^Quat4d rotation ^Vector3d scale]
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

(defn split-mat4 [^Matrix4d mat ^Point3d out-position ^Quat4d out-rotation ^Vector3d out-scale]
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
  (vecmath->clj [this]))

(extend-protocol VecmathConverter
  Tuple3d
  (clj->vecmath [this v] (.set this (nth v 0) (nth v 1) (nth v 2)))
  (vecmath->clj [this] [(.getX this) (.getY this) (.getZ this)])
  Tuple4d
  (clj->vecmath [this v] (.set this (nth v 0) (nth v 1) (nth v 2) (nth v 3)))
  (vecmath->clj [this] [(.getX this) (.getY this) (.getZ this) (.getW this)])
  Matrix4d
  (clj->vecmath [this v] (.set this (double-array v)))
  (vecmath->clj [this] [(.m00 this) (.m01 this) (.m02 this) (.m03 this)
                        (.m10 this) (.m11 this) (.m12 this) (.m13 this)
                        (.m20 this) (.m21 this) (.m22 this) (.m23 this)
                        (.m30 this) (.m31 this) (.m32 this) (.m33 this)]))

(defn hermite [y0 y1 t0 t1 t]
  (let [t2 (* t t)
        t3 (* t2 t)]
    (+ (* (+ (* 2 t3) (* -3 t2) 1.0) y0)
       (* (+ t3 (* -2 t2) t) t0)
       (* (+ (* -2 t3) (* 3 t2)) y1)
       (* (- t3 t2) t1))))

(defn hermite' [y0 y1 t0 t1 t]
  (let [t2 (* t t)]
    (+ (* (+ (* 6 t2) (* -6 t)) y0)
       (* (+ (* 3 t2) (* -4 t) 1) t0)
       (* (+ (* -6 t2) (* 6 t)) y1)
       (* (+ (* 3 t2) (* -2 t)) t1))))

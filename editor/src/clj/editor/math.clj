(ns editor.math
  (:import [java.lang Math]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d Quat4d AxisAngle4d]))

(def epsilon 0.000001)
(def epsilon-sq (* epsilon epsilon))

(def recip-180 (/ 1.0 180.0))
(def recip-pi (/ 1.0 Math/PI))

(defn deg->rad [deg]
  (* deg Math/PI recip-180))

(defn rad->deg [rad]
  (* rad 180.0 recip-pi))

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

(defn euler->quat [euler]
  ; Implementation based on:
  ; http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
  ; Rotation sequence: 231 (YZX)
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
    q))

(defn quat->euler [^Quat4d quat]
  (let [x (.getX quat)
        y (.getY quat)
        z (.getZ quat)
        w (.getW quat)
        test (+ (* x y) (* z w))
        euler-fn (fn [heading attitude bank]
                   [(* bank 180.0 recip-pi)
                    (* heading 180.0 recip-pi)
                    (* attitude 180.0 recip-pi)])]
    (cond
      (or (> test 0.499) (< test -0.499)) ; singularity at north pole
      (let [sign (Math/signum test)
            heading (* sign 2.0 (Math/atan2 x w))
            attitude (* sign Math/PI 0.5)
            bank 0]
        (map rad->deg [bank heading attitude]))
      
      :default
      (let [sqx (* x x)
            sqy (* y y)
            sqz (* z z)
            heading (Math/atan2 (- (* 2.0 y w) (* 2.0 x z)) (- 1.0 (* 2.0 sqy) (* 2.0 sqz)))
            attitude (Math/asin (* 2.0 test))
            bank (Math/atan2 (- (* 2.0 x w) (* 2.0 y z)) (- 1.0 (* 2.0 sqx) (* 2.0 sqz)))]
        (map rad->deg [bank heading attitude])))))

(defn rotate [^Quat4d rotation ^Vector3d v]
  (let [q (doto (Quat4d.) (.set (Vector4d. v)))
        _ (.mul q rotation q)
        _ (.mulInverse q rotation)]
    (Vector3d. (.getX q) (.getY q) (.getZ q))))

(defn from-to->quat [^Vector3d unit-from ^Vector3d unit-to]
  (let [dot (.dot unit-from unit-to)]
    (let [cos-half (Math/sqrt (* 2.0 (+ 1.0 dot)))
          recip-cos-half (/ 1.0 cos-half)
        axis (doto (Vector3d.) (.cross unit-from unit-to) (.scale recip-cos-half))]
      (doto (Quat4d. (.x axis) (.y axis) (.z axis) (* 0.5 cos-half))))))

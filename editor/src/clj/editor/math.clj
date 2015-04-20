(ns editor.math
  (:import [java.lang Math]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(def recip-180 (/ 1.0 180.0))
(def recip-pi (/ 1.0 Math/PI))

(defn deg->rad [deg]
  (* deg Math/PI recip-180))

(defn rad->deg [rad]
  (* rad 180.0 recip-pi))

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

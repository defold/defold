(ns editor.math-test
 (:require [clojure.test :refer :all]
           [editor.math :as math])
 (:import [java.lang Math]
          [javax.vecmath Point3d Vector3d Quat4d]))

(def epsilon 0.000001)

(defn- near? [v1 v2]
  (<= (Math/abs (- v1 v2)) epsilon))

(defn- eq? [q1 q2]
  (boolean (and (near? (.getX q1) (.getX q2)) (near? (.getY q1) (.getY q2)) (near? (.getZ q1) (.getZ q2)) (near? (.getW q1) (.getW q2)))))

(defn- ->vec3 [v]
  (Vector3d. (double-array v)))
(defn- ->point3 [v]
  (Point3d. (double-array v)))
(defn- ->quat [v]
  (Quat4d. (double-array v)))

(deftest quat->euler->quat []
  (let [test-vals [[1 0 0 0]
                   [0 1 0 0]
                   [0 0 -1 0]
                   [0 0 0 1]
                   [1 1 1 1]]
        quats (map ->quat test-vals)]
    (doseq [result (map eq? quats (map (comp math/euler->quat math/quat->euler) quats))]
      (is result))))

(defn- vec-eq? [^Vector3d lhs ^Vector3d rhs]
  (let [diff (doto (Vector3d. rhs) (.sub lhs))
        sq (.dot diff diff)
        ep-sq (* math/epsilon math/epsilon)]
    (< sq ep-sq)))

(deftest from-to->quat->vec
  (let [test-vals [[1 0 0]
                   [0 1 0]
                   [0 0 1]
                   [-1 0 0]
                   [0 -1 0]
                   [0 0 -1]]
        test-vecs (map ->vec3 test-vals)]
    (doall
      (for [from test-vecs
           to test-vecs
           :let [q (math/from-to->quat from to)]]
        (or
          (vec-eq? from (doto (Vector3d. to) (.negate)))
          (is (vec-eq? to (math/rotate q from))))))))

(deftest line-circle []
  (testing "Line circle projections"
           ; Line orthogonal to circle axis
           (let [x-vals [-1.5 -1 0 1 1.5]
                 line-positions (map #(do [% 1 2]) x-vals)
                 line-dir [0 0 -1]
                 circle-pos [0 0 0]
                 circle-axis [0 1 0]
                 circle-radius 1
                 expected-vals [[-1 0 0]
                                [-1 0 0]
                                [0 0 1]
                                [1 0 0]
                                [1 0 0]]]
             (doseq [[line-pos expected] (partition 2 (interleave (map ->point3 line-positions) (map ->vec3 expected-vals)))
                     :let [line-dir (->vec3 line-dir)
                           circle-pos (->point3 circle-pos)
                           circle-axis (->vec3 circle-axis)
                           result (math/project-line-circle line-pos line-dir circle-pos circle-axis circle-radius)]]
               (is (.epsilonEquals expected result math/epsilon))))))

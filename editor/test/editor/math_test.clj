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

(ns editor.math-test
  (:require [clojure.test :refer :all]
            [editor.math :as math])
  (:import [java.lang Math]
           [javax.vecmath Matrix3d Matrix4d Point3d Quat4d Tuple3d Tuple4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- near? [^double v1 ^double v2]
  (<= (Math/abs (- v1 v2)) math/epsilon))

(defn- clj-vec-eq? [a b]
  (and (= (count a)
          (count b))
       (every? true?
               (map near? a b))))

(defn- vec3-eq? [^Tuple3d a ^Tuple3d b]
  (and (near? (.getX a) (.getX b))
       (near? (.getY a) (.getY b))
       (near? (.getZ a) (.getZ b))))

(defn- vec4-eq? [^Tuple4d a ^Tuple4d b]
  (and (near? (.getX a) (.getX b))
       (near? (.getY a) (.getY b))
       (near? (.getZ a) (.getZ b))
       (near? (.getW a) (.getW b))))

(defn- quat-eq? [^Quat4d a ^Quat4d b]
  ;; Rotation applied by a negated quaternion is equivalent.
  (or (vec4-eq? a b)
      (vec4-eq? a (doto (Quat4d. b) (.negate)))))

(defn- quat-key [^Quat4d quat]
  (let [[x y z w]
        (let [components [(.getX quat)
                          (.getY quat)
                          (.getZ quat)
                          (.getW quat)]
              sign (double
                     (or (some (fn [^double component]
                                 (when-not (near? 0.0 component)
                                   (if (neg? component) -1.0 1.0)))
                               components)
                         1.0))]
          (into []
                (map (fn [^double component]
                       (math/round-with-precision (* (double sign) component) math/precision-general)))
                components))]
    [x y z w]))

(defn- mat4-eq? [^Matrix4d a ^Matrix4d b]
  (boolean
    (every?
      true?
      (for [row (range 4)
            col (range 4)]
        (near? (.getElement a row col)
               (.getElement b row col))))))

(defn- ->vec3
  ^Vector3d [v]
  (Vector3d. (double-array v)))

(defn- ->point3
  ^Point3d [v]
  (Point3d. (double-array v)))

(defn- ->quat
  ^Quat4d [v]
  (Quat4d. (double-array v)))

(defn- axis-angle->mat3
  ^Matrix3d [axis angle-deg]
  (let [angle-rad (math/deg->rad angle-deg)]
    (case axis
      :x (doto (Matrix3d.) (.rotX angle-rad))
      :y (doto (Matrix3d.) (.rotY angle-rad))
      :z (doto (Matrix3d.) (.rotZ angle-rad)))))

(defn- axis-angle->quat
  ^Quat4d [axis angle-deg]
  (let [half-angle-rad (* 0.5 (math/deg->rad angle-deg))
        sin (Math/sin half-angle-rad)
        cos (Math/cos half-angle-rad)]
    (case axis
      :x (Quat4d. sin 0.0 0.0 cos)
      :y (Quat4d. 0.0 sin 0.0 cos)
      :z (Quat4d. 0.0 0.0 sin cos))))

(defn- mul-quats
  ^Quat4d [^Quat4d q1 ^Quat4d q2]
  (doto (Quat4d. q1)
    (.mul q2)))

(defn- mul-mat3s
  ^Matrix3d [^Matrix3d m1 ^Matrix3d m2]
  (doto (Matrix3d. m1)
    (.mul m2)))

(deftest quat->euler->quat
  (let [test-vals [[1 0 0 0]
                   [0 1 0 0]
                   [0 0 -1 0]
                   [0 0 0 1]
                   [1 1 1 1]]
        quats (map ->quat test-vals)]
    (doseq [result (map quat-eq? quats (map (comp math/euler->quat math/quat->euler) quats))]
      (is result))))

(deftest canonicalize-euler
  (is (= [180.0 0.0 0.0] (math/canonicalize-euler [-180.0 0.0 0.0])))
  (is (= [0.0 180.0 0.0] (math/canonicalize-euler [0.0 -180.0 0.0])))
  (is (= [0.0 0.0 180.0] (math/canonicalize-euler [0.0 0.0 -180.0]))))

(def ^:private particular-xyz-euler-angles
  (mapv #(apply vector-of :double %)
        [[-35.0 120.0 -15.0]
         [10.0 20.0 30.0]
         [30.0 0.0 20.0]]))

(def ^:private common-xyz-euler-angles
  (let [component-values
        (into (vector-of :double -0.0)
              (distinct)
              (for [^double component [0.0 15.0 45.0 90.0 180.0 270.0 360.0]
                    ^double jitter [-1.0 0.0 1.0]
                    negate [false true]]
                (if negate
                  (- (+ component jitter))
                  (+ component jitter))))]
    (into particular-xyz-euler-angles
          (for [x component-values
                y component-values
                z component-values]
            (vector-of :double x y z)))))

(def ^:private particular-xyz-euler-angles-strict-round-trip-equality
  (mapv #(apply vector-of :double %)
        [[180.0 0.0 0.0]
         [0.0 180.0 0.0]
         [0.0 0.0 180.0]

         [180.0 90.0 0.0]
         #_[180.0 -90.0 0.0] ; Inferior to [0.0 90.0 180.0].
         #_[180.0 0.0 90.0] ; Inferior to [0.0 180.0 90.0].
         #_[180.0 0.0 -90.0] ; Inferior to [0.0 180.0 -90.0].

         [90.0 180.0 0.0]
         #_[-90.0 180.0 0.0] ; Inferior to [90.0 0.0 180.0].
         [0.0 180.0 90.0]
         [0.0 180.0 -90.0]

         [90.0 0.0 180.0]
         #_[-90.0 0.0 180.0] ; Inferior to [90.0 180.0 0.0].
         [0.0 90.0 180.0]
         #_[0.0 -90.0 180.0]])) ; Inferior to [180.0 90.0 0.0].

(def ^:private checked-xyz-euler-angles-strict-round-trip-equality
  ;; Check the particular XYZ euler angles and all the checked-xyz-euler-angles
  ;; that have a single representation as a quaternion.
  (into particular-xyz-euler-angles-strict-round-trip-equality
        (comp (filter (fn [[_ representations]]
                        (= 1 (count representations))))
              (mapcat val))
        (group-by #(quat-key (math/euler->quat %))
                  common-xyz-euler-angles)))

(def ^:private checked-xyz-euler-angles
  (into common-xyz-euler-angles
        particular-xyz-euler-angles-strict-round-trip-equality))

(deftest euler->quat-matches-yzx-composition
  (let [non-equivalent
        (for [[x y z :as euler] checked-xyz-euler-angles
              :let [expected (-> (axis-angle->quat :y y)
                                 (mul-quats (axis-angle->quat :z z))
                                 (mul-quats (axis-angle->quat :x x)))
                    actual (math/euler->quat euler)]
              :when (not (quat-eq? expected actual))]
          {:euler euler
           :expected expected
           :actual actual})]
    (is (= [] non-equivalent))))

(deftest euler->quat-matches-yzx-transformations
  (let [non-equivalent
        (for [[x y z :as euler] checked-xyz-euler-angles
              :let [rotation-mat (-> (axis-angle->mat3 :y y)
                                     (mul-mat3s (axis-angle->mat3 :z z))
                                     (mul-mat3s (axis-angle->mat3 :x x)))
                    rotation-quat (math/euler->quat euler)
                    unrotated (Vector3d. 1.0 2.0 3.0)
                    mat-rotated (let [mat-rotated (Vector3d. unrotated)]
                                  (.transform rotation-mat mat-rotated)
                                  mat-rotated)
                    quat-rotated (let [q (doto (Quat4d.) (.set (Vector4d. unrotated)))]
                                   (.mul q rotation-quat q)
                                   (.mulInverse q rotation-quat)
                                   (Vector3d. (.getX q) (.getY q) (.getZ q)))]
              :when (not (vec3-eq? mat-rotated quat-rotated))]
          {:euler euler
           :mat-rotated mat-rotated
           :quat-rotated quat-rotated})]
    (is (= [] non-equivalent))))

(deftest euler->quat->euler-round-trip
  (let [non-equivalent
        (for [in-euler checked-xyz-euler-angles-strict-round-trip-equality
              :let [in-quat (math/euler->quat in-euler)
                    out-euler (math/quat->euler in-quat)]
              :when (not (clj-vec-eq? in-euler out-euler))]
          {:in-euler in-euler
           :out-euler out-euler})]
    (is (= [] non-equivalent))))

(deftest quat->euler->quat-round-trip
  (let [non-equivalent
        (for [in-euler checked-xyz-euler-angles
              :let [in-quat (math/euler->quat in-euler)
                    out-euler (math/quat->euler in-quat)
                    out-quat (math/euler->quat out-euler)]
              :when (not (quat-eq? in-quat out-quat))]
          {:in-euler in-euler
           :in-quat in-quat
           :out-euler out-euler
           :out-quat out-quat})]
    (is (= [] non-equivalent))))

(deftest split-mat4-round-trip
  (let [scale-values (vector-of :double -10.0 1.0 10.0)

        scales
        (vec (for [^double scale-x scale-values
                   ^double scale-y scale-values
                   ^double scale-z scale-values]
               (Vector3d. scale-x scale-y scale-z)))

        split-translation (Vector3d.)
        split-rotation (Quat4d.)
        split-scale (Vector3d.)

        non-equivalent
        (for [scale scales
              euler checked-xyz-euler-angles
              :let [rotation (math/euler->quat euler)
                    translation (Vector3d. 1.0 2.0 3.0)
                    input-matrix (math/->mat4-non-uniform translation rotation scale)
                    _ (math/split-mat4 input-matrix split-translation split-rotation split-scale)
                    reconstructed-matrix (math/->mat4-non-uniform split-translation split-rotation split-scale)]
              :when (not (mat4-eq? input-matrix reconstructed-matrix))]
          {:translation (math/vecmath->clj translation)
           :euler euler
           :scale (math/vecmath->clj scale)
           :split-translation (math/vecmath->clj split-translation)
           :split-rotation (math/vecmath->clj split-rotation)
           :split-scale (math/vecmath->clj split-scale)})]

    (is (= [] non-equivalent))))

(deftest split-mat4-collapsed-axis-round-trip
  (let [collapsed-scales
        [(Vector3d. 1.0 1.0 0.0)
         (Vector3d. 1.0 0.0 1.0)
         (Vector3d. 0.0 1.0 1.0)
         (Vector3d. 1.0 0.0 0.0)
         (Vector3d. 0.0 1.0 0.0)
         (Vector3d. 0.0 0.0 1.0)]

        eulers
        [(vector-of :double 0.0 0.0 90.0)
         (vector-of :double 45.0 0.0 0.0)
         (vector-of :double 0.0 45.0 0.0)
         (vector-of :double 10.0 20.0 30.0)]

        split-translation (Vector3d.)
        split-rotation (Quat4d.)
        split-scale (Vector3d.)

        non-equivalent
        (for [scale collapsed-scales
              euler eulers
              :let [rotation (math/euler->quat euler)
                    translation (Vector3d. 1.0 2.0 3.0)
                    input-matrix (math/->mat4-non-uniform translation rotation scale)
                    _ (math/split-mat4 input-matrix split-translation split-rotation split-scale)
                    reconstructed-matrix (math/->mat4-non-uniform split-translation split-rotation split-scale)
                    rotation-length-squared (+ (* (.x split-rotation) (.x split-rotation))
                                               (* (.y split-rotation) (.y split-rotation))
                                               (* (.z split-rotation) (.z split-rotation))
                                               (* (.w split-rotation) (.w split-rotation)))]
              :when (or (not (near? 1.0 rotation-length-squared))
                        (not (mat4-eq? input-matrix reconstructed-matrix)))]
          {:euler euler
           :scale (math/vecmath->clj scale)
           :split-rotation (math/vecmath->clj split-rotation)
           :rotation-length-squared rotation-length-squared
           :split-scale (math/vecmath->clj split-scale)})]

    (is (= [] non-equivalent))))

(deftest split-mat4-uniform-zero-scale
  (let [eulers
        [(vector-of :double 0.0 0.0 0.0)
         (vector-of :double 0.0 0.0 90.0)
         (vector-of :double 45.0 0.0 0.0)
         (vector-of :double 10.0 20.0 30.0)]

        split-translation (Vector3d.)
        split-rotation (Quat4d.)
        split-scale (Vector3d.)

        non-equivalent
        (for [euler eulers
              :let [rotation (math/euler->quat euler)
                    translation (Vector3d. 1.0 2.0 3.0)
                    scale (Vector3d. 0.0 0.0 0.0)
                    input-matrix (math/->mat4-non-uniform translation rotation scale)
                    _ (math/split-mat4 input-matrix split-translation split-rotation split-scale)
                    reconstructed-matrix (math/->mat4-non-uniform split-translation split-rotation split-scale)
                    rotation-length-squared (+ (* (.x split-rotation) (.x split-rotation))
                                               (* (.y split-rotation) (.y split-rotation))
                                               (* (.z split-rotation) (.z split-rotation))
                                               (* (.w split-rotation) (.w split-rotation)))]
              :when (or (not (vec3-eq? translation split-translation))
                        (not (vec3-eq? scale split-scale))
                        (not (near? 1.0 rotation-length-squared))
                        (not (mat4-eq? input-matrix reconstructed-matrix)))]
          {:euler euler
           :split-translation (math/vecmath->clj split-translation)
           :split-rotation (math/vecmath->clj split-rotation)
           :rotation-length-squared rotation-length-squared
           :split-scale (math/vecmath->clj split-scale)})]

    (is (= [] non-equivalent))))

(deftest from-to->quat->vec
  (let [test-vals [[1 0 0]
                   [0 1 0]
                   [0 0 1]
                   [-1 0 0]
                   [0 -1 0]
                   [0 0 -1]]
        test-vecs (map ->vec3 test-vals)]
    (doall
      (for [^Vector3d from test-vecs
            ^Vector3d to test-vecs
            :let [q (math/from-to->quat from to)]]
        (or
          (vec3-eq? from (doto (Vector3d. to) (.negate)))
          (is (vec3-eq? to (math/rotate q from))))))))

(deftest line-circle
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
      (doseq [[^Point3d line-pos ^Vector3d expected] (partition 2 (interleave (map ->point3 line-positions) (map ->vec3 expected-vals)))
              :let [line-dir (->vec3 line-dir)
                    circle-pos (->point3 circle-pos)
                    circle-axis (->vec3 circle-axis)
                    result (math/project-line-circle line-pos line-dir circle-pos circle-axis circle-radius)]]
        (is (.epsilonEquals expected result math/epsilon))))))

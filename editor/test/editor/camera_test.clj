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

(ns editor.camera-test
  (:require [clojure.test :refer :all]
            [editor.camera :as c]
            [editor.math :as math]
            [editor.types :as t]
            [editor.geom :as geom])
  (:import [editor.types Camera Region AABB]
           [javax.vecmath Point3d SingularMatrixException Vector3d]))

(deftest frame-zero-aabb
  (testing "Framing an orthographic camera on a zero AABB should produce a valid projection matrix"
    (let [camera (c/make-camera :orthographic)
          viewport (t/->Region 0 10 0 10)
          zero-aabb (t/->AABB (Point3d. 0 0 0) (Point3d. 0 0 0))
          camera (c/camera-orthographic-frame-aabb camera viewport zero-aabb)
          proj (c/camera-projection-matrix camera)]
      ;; Would fail for SingularMatrixException if the matrix could not be inverted
      (is (doto proj (.invert))))))

(deftest frame-null-aabb
  (testing "Framing an orthographic camera on a null AABB should not affect the projection matrix"
    (let [camera (c/make-camera :orthographic)
          viewport (t/->Region 0 10 0 10)
          proj-before (c/camera-projection-matrix camera)
          framed-camera (c/camera-orthographic-frame-aabb camera viewport geom/null-aabb)
          proj (c/camera-projection-matrix framed-camera)]
      (is (.epsilonEquals proj-before proj math/epsilon)))))

(deftest mouse-button-interpretation
  (are [move button shift control alt meta]
    (= move (c/camera-movement {:button button :shift shift :control control :alt alt :meta meta}
                               #{:tumble :track :dolly}))

    ;; move button     shift ctrl  alt   meta
    :tumble :primary   false true  false false
    :track  :primary   false false true  false
    :dolly  :primary   false true  true  false
    :idle   :primary   false false false false
    :idle   :primary   true  false false false
    :idle   :primary   false false false true
    :track  :secondary false false false false
    :idle   :secondary true  false false false
    :idle   :secondary false true  false false
    :dolly  :secondary false false true false
    :idle   :secondary false false false true
    :track  :middle    false false false false
    :idle   :middle    true  false false false
    :idle   :middle    false true  false false
    :idle   :middle    false false true  false))

(deftest look-rotation-pitch-clamp
  (testing "Pitch should clamp at ±86 degrees even with extreme dy input"
    (let [camera (c/make-camera :perspective)
          initial-state {:free-cam-pitch 0.0
                         :free-cam-yaw 0.0
                         :free-cam-smoothed-pitch 0.0
                         :free-cam-smoothed-yaw 0.0}
          sensitivity 0.1
          dt 1.0
          [_ state-down] (#'c/look-rotation initial-state camera 0.0 100000.0 sensitivity dt)
          [_ state-up] (#'c/look-rotation initial-state camera 0.0 -100000.0 sensitivity dt)]
      (is (<= (:free-cam-pitch state-down) 86.0)
          "Pitch should not exceed 86 degrees looking down")
      (is (>= (:free-cam-pitch state-up) -86.0)
          "Pitch should not go below -86 degrees looking up"))))

(deftest look-rotation-smoothing-converges
  (testing "Smoothed pitch/yaw should converge toward target over multiple frames"
    (let [camera (c/make-camera :perspective)
          initial-state {:free-cam-pitch 0.0
                         :free-cam-yaw 0.0
                         :free-cam-smoothed-pitch 0.0
                         :free-cam-smoothed-yaw 0.0}
          sensitivity 0.1
          dt (/ 1.0 60.0)
          [_ state-after-impulse] (#'c/look-rotation initial-state camera 50.0 30.0 sensitivity dt)
          state-after-settling (loop [state state-after-impulse
                                      cam camera
                                      i 0]
                                 (if (< i 300)
                                   (let [[new-cam new-state] (#'c/look-rotation state cam 0.0 0.0 sensitivity dt)]
                                     (recur new-state new-cam (inc i)))
                                   state))
          target-pitch (:free-cam-pitch state-after-settling)
          smoothed-pitch (:free-cam-smoothed-pitch state-after-settling)
          target-yaw (:free-cam-yaw state-after-settling)
          smoothed-yaw (:free-cam-smoothed-yaw state-after-settling)]
      (is (< (Math/abs (- ^double smoothed-pitch ^double target-pitch)) 0.01)
          "Smoothed pitch should converge to target pitch")
      (is (< (Math/abs (- ^double smoothed-yaw ^double target-yaw)) 0.01)
          "Smoothed yaw should converge to target yaw"))))

(deftest wasd-move-applies-velocity-in-target-direction
  (let [camera (c/make-camera :perspective)
        camera (assoc camera :focus-distance 100.0)
        velocity (Vector3d. 0.0 0.0 0.0)
        forward (Vector3d. 1.0 0.0 0.0)
        dt (/ 1.0 60.0)]
    (let [result (reduce
                   (fn [cam _]
                     (or (#'c/wasd-move velocity cam forward 5.0 dt) cam))
                   camera
                   (range 20))
          start-pos (t/position camera)
          end-pos (t/position result)
          displacement (doto (Vector3d. end-pos) (.sub start-pos))]
      (is (> (.length displacement) 0.0))
      (let [norm-disp (doto (Vector3d. displacement) (.normalize))
            norm-fwd (doto (Vector3d. forward) (.normalize))]
        (is (> (.dot norm-disp norm-fwd) 0.99))))))

(deftest wasd-move-decelerates-when-no-target-direction
  (let [camera (c/make-camera :perspective)
        camera (assoc camera :focus-distance 100.0)
        velocity (Vector3d. 0.0 0.0 0.0)
        forward (c/camera-forward-vector camera)
        zero-dir (Vector3d. 0.0 0.0 0.0)
        dt 0.016
        moving-camera (reduce
                        (fn [cam _]
                          (or (#'c/wasd-move velocity cam forward 5.0 dt) cam))
                        camera
                        (range 30))
        speed-before (.length velocity)
        stopped-camera (reduce
                         (fn [cam _]
                           (or (#'c/wasd-move velocity cam zero-dir 5.0 dt) cam))
                         moving-camera
                         (range 200))
        speed-after (.length velocity)]
    (is (> speed-before 0.1) "Should have built up velocity")
    (is (< speed-after 0.001) "Velocity should have decayed to near zero")))

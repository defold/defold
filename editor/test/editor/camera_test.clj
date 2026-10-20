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
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.geom :as geom]
            [editor.math :as math]
            [editor.types :as t]
            [integration.test-util :as test-util]
            [support.test-support :refer [tx-nodes with-clean-system]])
  (:import [editor.types Region]
           [javax.vecmath Point3d Quat4d Vector3d Vector4d]))

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

(deftest frame-wide-aabb
  (testing "Framing a wide AABB should fit the whole bounds in the viewport"
    (let [camera (c/make-camera :orthographic identity {:fov-x 960.0
                                                        :fov-y 640.0})
          viewport (t/->Region 0 960 0 640)
          aabb (t/->AABB (Point3d. 0.0 0.0 0.0)
                         (Point3d. 1224.0 640.0 0.0))
          camera (c/camera-orthographic-frame-aabb camera viewport aabb)
          min-proj (c/camera-project camera viewport (.. aabb min))
          max-proj (c/camera-project camera viewport (.. aabb max))]
      (is (<= (:left viewport) (.x min-proj) (:right viewport)))
      (is (<= (:left viewport) (.x max-proj) (:right viewport)))
      (is (<= (:top viewport) (.y min-proj) (:bottom viewport)))
      (is (<= (:top viewport) (.y max-proj) (:bottom viewport))))))

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
        dt (/ 1.0 60.0)
        result (reduce
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
      (is (> (.dot norm-disp norm-fwd) 0.99)))))

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
        _ (reduce
            (fn [cam _]
              (or (#'c/wasd-move velocity cam zero-dir 5.0 dt) cam))
            moving-camera
            (range 200))
        speed-after (.length velocity)]
    (is (> speed-before 0.1) "Should have built up velocity")
    (is (< speed-after 0.001) "Velocity should have decayed to near zero")))

(defn- framed-test-camera []
  (assoc (c/make-camera :perspective)
    :position (Point3d. 13.0 24.0 35.0)
    :focus-point (Vector4d. 10.0 20.0 30.0 1.0)))

(defn- same-rotation? [^Quat4d a ^Quat4d b]
  (let [dot (+ (* (.x a) (.x b)) (* (.y a) (.y b)) (* (.z a) (.z b)) (* (.w a) (.w b)))]
    (< (Math/abs (- 1.0 (Math/abs dot))) 1e-9)))

;; Axis alignment preserves the focus point and distance for all six directions.
(deftest frame-camera-to-axis-faces-axis-at-same-distance
  (let [camera (framed-test-camera)
        focus (c/camera-focus-point camera)
        distance (.distance focus ^Point3d (:position camera))]
    (doseq [[axis ^Vector3d direction] {:+x (Vector3d. 1.0 0.0 0.0)
                                        :-x (Vector3d. -1.0 0.0 0.0)
                                        :+y (Vector3d. 0.0 1.0 0.0)
                                        :-y (Vector3d. 0.0 -1.0 0.0)
                                        :+z (Vector3d. 0.0 0.0 1.0)
                                        :-z (Vector3d. 0.0 0.0 -1.0)}]
      (testing axis
        (let [framed (c/frame-camera-to-axis camera axis)
              expected-forward (doto (Vector3d. direction) (.negate))]
          (is (.epsilonEquals expected-forward (c/camera-forward-vector framed) 1e-9))
          (is (< (Math/abs (- distance (.distance focus ^Point3d (:position framed)))) 1e-9))
          (is (.epsilonEquals focus (c/camera-focus-point framed) 1e-9)))))))

;; Orbit interpolation preserves position and rotation at both endpoints.
(deftest interpolate-orbit-matches-endpoints
  (let [from (c/frame-camera-to-axis (framed-test-camera) :+x)
        to (c/frame-camera-to-axis (framed-test-camera) :+y)
        start (#'c/interpolate-orbit from to 0.0)
        end (#'c/interpolate-orbit from to 1.0)]
    (is (.epsilonEquals ^Point3d (:position from) ^Point3d (:position start) 1e-9))
    (is (same-rotation? (:rotation from) (:rotation start)))
    (is (.epsilonEquals ^Point3d (:position to) ^Point3d (:position end) 1e-9))
    (is (same-rotation? (:rotation to) (:rotation end)))))

;; Opposite-axis interpolation keeps its radius instead of crossing the focus point.
(deftest interpolate-orbit-to-opposite-axis-keeps-distance
  (let [from (c/frame-camera-to-axis (framed-test-camera) :+x)
        to (c/frame-camera-to-axis (framed-test-camera) :-x)
        focus (c/camera-focus-point from)
        distance (.distance focus ^Point3d (:position from))
        halfway (#'c/interpolate-orbit from to 0.5)]
    (is (< (Math/abs (- distance (.distance focus ^Point3d (:position to)))) 1e-9))
    (is (< (Math/abs (- distance (.distance focus ^Point3d (:position halfway)))) 1e-9))))

;; Dollying past the focus must not cause an initial jump to its opposite side.
(deftest interpolate-orbit-keeps-start-past-focus-point
  (let [from (c/dolly (c/frame-camera-to-axis (framed-test-camera) :+z) 2.0)
        to (c/frame-camera-to-axis from :+x)
        start (#'c/interpolate-orbit from to 0.0)
        halfway (#'c/interpolate-orbit from to 0.5)
        focus (c/camera-focus-point from)
        distance (.distance focus ^Point3d (:position from))]
    (is (.epsilonEquals ^Point3d (:position from) ^Point3d (:position start) 1e-9))
    (is (< (Math/abs (- distance (.distance focus ^Point3d (:position halfway)))) 1e-9))))

(deftest mode-2d-is-independent-of-the-view-direction
  (let [camera-2d (c/make-camera :orthographic)]
    (is (c/mode-2d? camera-2d))
    (is (not (c/mode-2d? (c/make-camera :perspective))))
    (testing "Orbiting leaves 2D mode"
      (is (not (c/mode-2d? (c/tumble camera-2d 10.0 0.0)))))
    (testing "Aligning to the front view does not enter 2D mode"
      (let [front (c/frame-camera-to-axis (c/tumble camera-2d 10.0 0.0) :+z)]
        (is (= (:rotation camera-2d) (:rotation front)))
        (is (not (c/mode-2d? front)))))
    (testing "Converting to perspective leaves 2D mode"
      (is (not (c/mode-2d? (c/camera-orthographic->perspective camera-2d 30.0)))))
    (testing "Moving and zooming stay in 2D mode"
      (is (c/mode-2d? (c/dolly camera-2d 2.0)))
      (is (c/mode-2d? (c/camera-move camera-2d 10.0 20.0 0.0))))))

(deftest interpolation-is-2d-only-between-2d-cameras
  (let [camera-2d (c/make-camera :orthographic)
        camera-3d (c/tumble camera-2d 10.0 0.0)]
    (doseq [interpolate [c/interpolate #'c/interpolate-orbit]]
      (is (c/mode-2d? (interpolate camera-2d (c/dolly camera-2d 2.0) 0.5)))
      (is (not (c/mode-2d? (interpolate camera-2d camera-3d 0.5))))
      (is (not (c/mode-2d? (interpolate camera-3d camera-2d 0.5)))))))

(g/defnode ViewportNode
  (property viewport Region))

(defn- make-camera-node! [camera]
  (first
    (tx-nodes (g/make-nodes [camera-node [c/CameraController :local-camera camera :prefs (test-util/make-test-prefs)]
                             viewport-node [ViewportNode :viewport (t/->Region 0.0 200.0 0.0 100.0)]]
                (g/connect viewport-node :viewport camera-node :viewport)))))

(deftest realign-camera-toggles-2d-mode
  (doseq [projection [:orthographic :perspective]]
    (testing (name projection)
      (with-clean-system
        (let [camera-3d (c/tumble (assoc (c/make-camera projection)
                                    :position (Point3d. 0.0 0.0 100.0))
                                  10.0 20.0)
              camera-node (make-camera-node! camera-3d)
              local-camera #(g/node-value camera-node :local-camera)]
          (is (not (c/mode-2d? (local-camera))))
          (c/realign-camera camera-node false)
          (is (c/mode-2d? (local-camera)))
          (is (= :orthographic (:type (local-camera))))
          (is (= (Quat4d. 0.0 0.0 0.0 1.0) (:rotation (local-camera))))
          (c/realign-camera camera-node false)
          (is (not (c/mode-2d? (local-camera))))
          (is (= projection (:type (local-camera))))
          (is (= (:rotation camera-3d) (:rotation (local-camera)))))))))

(deftest switching-to-perspective-leaves-2d-mode
  (with-clean-system
    (let [camera-node (make-camera-node! (c/make-camera :orthographic))]
      (c/set-camera-type! camera-node :perspective)
      (is (not (c/mode-2d? (g/node-value camera-node :local-camera)))))))

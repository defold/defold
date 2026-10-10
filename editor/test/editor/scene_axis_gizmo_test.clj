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

(ns editor.scene-axis-gizmo-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.scene-axis-gizmo :as axis-gizmo]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [util.coll :as coll])
  (:import [javax.vecmath Point3d Vector4d]))

(defn- test-camera []
  (assoc (c/make-camera :perspective)
    :position (Point3d. 3.0 4.0 5.0)
    :focus-point (Vector4d. 0.0 0.0 0.0 1.0)))

(defn- make-gizmo! [camera]
  (tx-nodes (g/make-nodes [camera-node [c/CameraController :local-camera camera]
                           gizmo axis-gizmo/AxisGizmoController]
              (g/connect camera-node :_node-id gizmo :camera-node-id))))

(defn- input! [gizmo type x y hits & {:keys [button] :or {button :primary}}]
  (axis-gizmo/handle-input gizmo nil {:type type :x x :y y :button button} {gizmo hits}))

(defn- framed-axes
  "Calls `f` and returns the axes it framed the camera to, without animating."
  [f]
  (let [framed (atom [])]
    (with-redefs [c/frame-camera-to-axis! (fn [_camera-node _camera axis _animate]
                                            (swap! framed conj axis))]
      (f))
    @framed))

;; Clicking a ball frames its axis and consumes the press and release.
(deftest click-on-ball-frames-axis
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (test-camera))]
      (is (= [:+x]
             (framed-axes
               #(do (is (nil? (input! gizmo :mouse-pressed 10 10 [:+x])))
                    (is (nil? (input! gizmo :mouse-released 10 10 [:+x]))))))))))

;; Clicking the axis the camera already faces switches to the opposite view.
(deftest click-on-aligned-axis-frames-opposite
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (c/frame-camera-to-axis (test-camera) :+x))]
      (is (= [:-x]
             (framed-axes
               #(do (input! gizmo :mouse-pressed 10 10 [:+x])
                    (input! gizmo :mouse-released 10 10 [:+x]))))))))

;; A ball wins over the backdrop that encloses it.
(deftest click-on-handle-in-front-of-backdrop-frames-handle
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (test-camera))]
      (is (= [:+y]
             (framed-axes
               #(do (input! gizmo :mouse-pressed 10 10 [:backdrop :+y])
                    (input! gizmo :mouse-released 10 10 [:backdrop :+y]))))))))

;; The backdrop swallows clicks so they don't select scene objects behind it.
(deftest click-on-backdrop-is-consumed-without-framing
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (test-camera))]
      (is (coll/empty?
            (framed-axes
              #(do (is (nil? (input! gizmo :mouse-pressed 10 10 [:backdrop])))
                   (is (nil? (input! gizmo :mouse-released 10 10 [:backdrop]))))))))))

;; Dragging from a ball past the threshold tumbles the camera instead of framing.
(deftest drag-from-ball-tumbles-without-framing
  (with-clean-system
    (let [[camera-node gizmo] (make-gizmo! (test-camera))
          release {:type :mouse-released :x 20 :y 10 :button :primary}]
      (is (coll/empty?
            (framed-axes
              #(do (input! gizmo :mouse-pressed 10 10 [:+x])
                   (testing "Small movements stay a click"
                     (is (nil? (input! gizmo :mouse-moved 12 12 [:+x])))
                     (is (nil? (:movement (g/user-data camera-node ::c/camera-state)))))
                   (testing "Larger movements hand the drag to the camera"
                     (is (nil? (input! gizmo :mouse-moved 20 10 [:+x])))
                     (is (= :tumble (:movement (g/user-data camera-node ::c/camera-state)))))
                   (is (= release (axis-gizmo/handle-input gizmo nil release {gizmo [:+x]}))))))))))

;; Input the gizmo doesn't own reaches the tool and selection controllers.
(deftest input-outside-gizmo-passes-through
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (test-camera))]
      (is (coll/empty?
            (framed-axes
              #(do (testing "Press that misses the gizmo"
                     (is (some? (input! gizmo :mouse-pressed 10 10 nil)))
                     (is (some? (input! gizmo :mouse-released 10 10 nil))))
                   (testing "Non-primary button on a ball"
                     (is (some? (input! gizmo :mouse-pressed 10 10 [:+x] :button :secondary)))
                     (is (some? (input! gizmo :mouse-released 10 10 [:+x] :button :secondary)))))))))))

;; Hovering highlights a handle, and leaving it or the view clears the highlight.
(deftest hover-sets-and-clears-hot-handle
  (with-clean-system
    (let [[_camera-node gizmo] (make-gizmo! (test-camera))]
      (is (some? (input! gizmo :mouse-moved 10 10 [:+z])))
      (is (= :+z (g/node-value gizmo :hot-handle)))
      (input! gizmo :mouse-moved 10 10 nil)
      (is (nil? (g/node-value gizmo :hot-handle)))
      (input! gizmo :mouse-moved 10 10 [:-y])
      (is (= :-y (g/node-value gizmo :hot-handle)))
      (input! gizmo :mouse-exited 10 10 nil)
      (is (nil? (g/node-value gizmo :hot-handle))))))

;; The gizmo is hidden in 2D mode, where axis alignment doesn't apply.
(deftest hidden-in-2d-mode
  (let [produce-renderables #'axis-gizmo/produce-renderables
        args {:_node-id 1 :backdrop-alpha 0.0 :hot-handle nil}
        camera-2d (c/frame-camera-to-axis (c/make-camera :orthographic) :+z)]
    (is (c/mode-2d? camera-2d))
    (is (= {} (produce-renderables (assoc args :camera camera-2d))))
    (is (not (coll/empty? (produce-renderables (assoc args :camera (test-camera))))))))

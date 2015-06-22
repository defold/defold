(ns editor.camera-test
  (:require [clojure.test :refer :all]
            [editor.camera :as camera]
            [editor.math :as math]
            [dynamo.types :as t]
            [dynamo.geom :as geom])
  (:import [dynamo.types Camera Region AABB]
           [javax.vecmath Point3d]
           [javax.vecmath SingularMatrixException]))

(deftest frame-zero-aabb
  (testing "Framing an orthographic camera on a zero AABB should produce a valid projection matrix"
           (let [camera (camera/make-camera :orthographic)
                 viewport (t/->Region 0 10 0 10)
                 zero-aabb (t/->AABB (Point3d. 0 0 0) (Point3d. 0 0 0))
                 camera (camera/camera-orthographic-frame-aabb camera viewport zero-aabb)
                 proj (camera/camera-projection-matrix camera)]
             ; Would fail for SingularMatrixException if the matrix could not be inverted
             (is (doto proj (.invert))))))

(deftest frame-null-aabb
  (testing "Framing an orthographic camera on a null AABB should not affect the projection matrix"
           (let [camera (camera/make-camera :orthographic)
                 viewport (t/->Region 0 10 0 10)
                 null-aabb (geom/null-aabb)
                 proj-before (camera/camera-projection-matrix camera)
                 framed-camera (camera/camera-orthographic-frame-aabb camera viewport null-aabb)
                 proj (camera/camera-projection-matrix framed-camera)]
             (is (.epsilonEquals proj-before proj math/epsilon)))))

(deftest mouse-button-interpretation
  (are [move button-scheme button shift control alt meta]
    (= move (camera/camera-movement button-scheme button shift control alt meta))
       ;; move  scheme        button     shift  ctrl   alt meta
       :tumble :one-button   :primary   false false  true false
       :track  :one-button   :primary   false  true  true false
       :dolly  :one-button   :primary   false  true false false
       :idle   :one-button   :primary   false false false false
       :tumble :three-button :primary   false false  true false
       :track  :three-button :secondary false false  true false
       :dolly  :three-button :middle    false false  true false
       :idle   :three-button :primary   false false false false
       :idle   :three-button :secondary false  true false false
       :idle   :three-button :secondary false false false false
       :idle   :three-button :middle    false  true false false))

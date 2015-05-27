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
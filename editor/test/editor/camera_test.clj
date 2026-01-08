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
            [editor.camera :as camera]
            [editor.math :as math]
            [editor.types :as t]
            [editor.geom :as geom])
  (:import [editor.types Camera Region AABB]
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
                 proj-before (camera/camera-projection-matrix camera)
                 framed-camera (camera/camera-orthographic-frame-aabb camera viewport geom/null-aabb)
                 proj (camera/camera-projection-matrix framed-camera)]
             (is (.epsilonEquals proj-before proj math/epsilon)))))

(deftest mouse-button-interpretation
  (are [move button shift control alt meta]
    (= move (camera/camera-movement button shift control alt meta))

    ;; move button     shift ctrl  alt   meta
    :tumble :primary   false true  false false
    :track  :primary   false false true  false
    :dolly  :primary   false true  true  false
    :dolly  :secondary false false true  false
    :track  :middle    false false false false
    :idle   :primary   false false false false
    :idle   :primary   true  false false false
    :idle   :primary   false false false true
    :track  :secondary false false false false
    :idle   :secondary true  false false false
    :idle   :secondary false true  false false
    :idle   :secondary false false false true
    :idle   :middle    true  false false false
    :idle   :middle    false true  false false
    :idle   :middle    false false true  false))

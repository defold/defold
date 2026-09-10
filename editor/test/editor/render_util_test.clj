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

(ns editor.render-util-test
  (:require [clojure.test :refer :all]
            [editor.gl.texture :as texture]
            [editor.pose :as pose]
            [editor.render-util :as render-util])
  (:import [clojure.lang ExceptionInfo]
           [javax.vecmath Matrix4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest make-outlined-textured-quad-scene-accepts-pose-or-matrix-test
  (let [quad-pose (pose/translation-pose 1.0 2.0 3.0)
        transform (doto (Matrix4d.)
                    (.setIdentity)
                    (.setTranslation (Vector3d. 1.0 2.0 3.0)))]
    (is (= quad-pose
           (:pose (render-util/make-outlined-textured-quad-scene #{} quad-pose 10 20 @texture/white-pixel 0))))
    (is (= quad-pose
           (:pose (render-util/make-outlined-textured-quad-scene #{} transform 10 20 @texture/white-pixel 0))))))

(deftest make-outlined-textured-quad-scene-rejects-unsupported-transform-test
  (is (thrown-with-msg?
        ExceptionInfo
        #"Expected Pose or Matrix4d"
        (render-util/make-outlined-textured-quad-scene #{} ::invalid 10 20 @texture/white-pixel 0))))

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

(ns editor.pose-test
  (:require [clojure.test :refer :all]
            [editor.math :as math]
            [editor.pose :as pose])
  (:import [javax.vecmath Matrix4d Quat4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- round-numbers [numbers]
  (mapv #(math/round-with-precision % 0.001)
        numbers))

(defn- round-euler-rotation [pose]
  (round-numbers
    (math/clj-quat->euler (:rotation pose))))

(deftest euler-rotation-test
  (is (thrown? Throwable (pose/euler-rotation 1.0 2.0 nil)))
  (is (identical? pose/default-rotation (pose/euler-rotation 0.0 0.0 0.0)))
  (is (identical? pose/default-rotation (pose/euler-rotation 0 0 0)))
  (is (= [0.5 0.5 -0.5 0.5] (round-numbers (pose/euler-rotation 90.0 90.0 0.0))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/euler-rotation 0.0 90.0 0.0))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/euler-rotation 0 90 0)))))

(deftest euler-seq-rotation-test
  (is (thrown? Throwable (pose/seq-euler-rotation [1.0 2.0])))
  (is (identical? pose/default-rotation (pose/seq-euler-rotation nil)))
  (is (identical? pose/default-rotation (pose/seq-euler-rotation [0.0 0.0 0.0])))
  (is (identical? pose/default-rotation (pose/seq-euler-rotation [0 0 0])))
  (is (= [0.5 0.5 -0.5 0.5] (round-numbers (pose/seq-euler-rotation [90.0 90.0 0.0]))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/seq-euler-rotation [0.0 90.0 0.0]))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/seq-euler-rotation [0.0 90.0 0.0 0.0]))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/seq-euler-rotation [0.0 90.0 0.0 1.0]))))
  (is (= [0.0 0.707 0.0 0.707] (round-numbers (pose/seq-euler-rotation (take 10 (interpose 90.0 (repeat 0.0))))))))

(deftest seq-translation-test
  (is (thrown? Throwable (pose/seq-translation [1.0 2.0])))
  (is (identical? pose/default-translation (pose/seq-translation nil)))
  (is (identical? pose/default-translation (pose/seq-translation [0.0 0.0 0.0])))
  (is (identical? pose/default-translation (pose/seq-translation [0 0 0])))
  (is (= [1.0 2.0 3.0] (pose/seq-translation [1 2 3])))
  (is (= [1.5 2.5 3.5] (pose/seq-translation [1.5 2.5 3.5])))
  (is (= [1.0 2.0 3.0] (pose/seq-translation [1.0 2.0 3.0 0.0])))
  (is (= [1.0 2.0 3.0] (pose/seq-translation [1.0 2.0 3.0 1.0])))
  (is (= [0.5 1.5 2.5] (round-numbers (pose/seq-translation (map #(+ (double %) 0.5) (take 10 (iterate inc 0.0))))))))

(deftest seq-rotation-test
  (is (thrown? Throwable (pose/seq-rotation [1.0 2.0 3.0])))
  (is (identical? pose/default-rotation (pose/seq-rotation nil)))
  (is (identical? pose/default-rotation (pose/seq-rotation [0.0 0.0 0.0 1.0])))
  (is (identical? pose/default-rotation (pose/seq-rotation [0 0 0 1])))
  (is (= [1.0 2.0 3.0 4.0] (pose/seq-rotation [1 2 3 4])))
  (is (= [1.5 2.5 3.5 4.5] (pose/seq-rotation [1.5 2.5 3.5 4.5])))
  (is (= [1.0 2.0 3.0 4.0] (pose/seq-rotation [1.0 2.0 3.0 4.0 0.0])))
  (is (= [1.0 2.0 3.0 4.0] (pose/seq-rotation [1.0 2.0 3.0 4.0 1.0])))
  (is (= [0.5 1.5 2.5 3.5] (round-numbers (pose/seq-rotation (map #(+ (double %) 0.5) (take 10 (iterate inc 0.0))))))))

(deftest seq-scale-test
  (is (thrown? Throwable (pose/seq-scale [1.0 2.0])))
  (is (identical? pose/default-scale (pose/seq-scale nil)))
  (is (identical? pose/default-scale (pose/seq-scale [1.0 1.0 1.0])))
  (is (identical? pose/default-scale (pose/seq-scale [1 1 1])))
  (is (= [1.0 2.0 3.0] (pose/seq-scale [1 2 3])))
  (is (= [1.5 2.5 3.5] (pose/seq-scale [1.5 2.5 3.5])))
  (is (= [1.0 2.0 3.0] (pose/seq-scale [1.0 2.0 3.0 0.0])))
  (is (= [1.0 2.0 3.0] (pose/seq-scale [1.0 2.0 3.0 1.0])))
  (is (= [0.5 1.5 2.5] (round-numbers (pose/seq-scale (map #(+ (double %) 0.5) (take 10 (iterate inc 0.0))))))))

(deftest pose?-test
  (is (false? (pose/pose? nil)))
  (is (false? (pose/pose? {:translation [0.0 0.0 0.0]
                           :rotation [0.0 0.0 0.0 1.0]
                           :scale [1.0 1.0 1.0]})))
  (is (true? (pose/pose? (pose/make [0.0 0.0 0.0]
                                    [0.0 0.0 0.0 1.0]
                                    [1.0 1.0 1.0]))))
  (is (true? (pose/pose? (-> (pose/make [0.0 0.0 0.0]
                                        [0.0 0.0 0.0 1.0]
                                        [1.0 1.0 1.0])
                             (assoc :extra "Extra Data"))))))

(deftest make-test
  (is (pose/pose? (pose/make nil nil nil)))
  (is (pose/pose? (pose/make [1.0 0.0 0.0] [0.0 0.707 0.707 0.0] [2.0 1.0 1.0])))
  (is (pose/pose? (pose/make [1 0 0] [0 1 0 1] [2 1 1])))
  (is (pose/pose? (pose/make (vector-of :float 1.0 0.0 0.0) (vector-of :float 0.0 0.707 0.707 0.0) (vector-of :float 2.0 1.0 1.0))))
  (is (thrown? Throwable (pose/make [1.0 0.0] nil nil))) ; Too few components in translation.
  (is (thrown? Throwable (pose/make [1.0 0.0 0.0 0.0] nil nil))) ; Too many components in translation.
  (is (thrown? Throwable (pose/make nil [0.0 0.707 0.707] nil))) ; Too few components in rotation.
  (is (thrown? Throwable (pose/make nil [0.0 0.707 0.707 0.0 0.0] nil))) ; Too many components in rotation.
  (is (thrown? Throwable (pose/make nil nil [2.0 1.0]))) ; Too few components in scale.
  (is (thrown? Throwable (pose/make nil nil [2.0 1.0 1.0 1.0]))) ; Too many components in scale.
  (is (thrown? Throwable (pose/make (Vector3d. 1.0 0.0 0.0) (Quat4d. 0.0 0.707 0.707 0.0) (Vector3d. 2.0 1.0 1.0)))))

(deftest from-translation-test
  (is (thrown? Throwable (pose/from-translation [1.0 2.0])))
  (is (thrown? Throwable (pose/from-translation [1.0 2.0 3.0 4.0])))
  (is (identical? pose/default (pose/from-translation nil)))
  (is (identical? pose/default (pose/from-translation [0.0 0.0 0.0])))
  (is (identical? pose/default (pose/from-translation [0 0 0])))
  (let [pose (pose/from-translation [1.0 2.0 3.0])]
    (is (= [1.0 2.0 3.0] (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/from-translation [1 2 3])]
    (is (= [1.0 2.0 3.0] (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest from-rotation-test
  (is (thrown? Throwable (pose/from-rotation [1.0 2.0 3.0])))
  (is (thrown? Throwable (pose/from-rotation [1.0 2.0 3.0 4.0 5.0])))
  (is (identical? pose/default (pose/from-rotation nil)))
  (is (identical? pose/default (pose/from-rotation [0.0 0.0 0.0 1.0])))
  (is (identical? pose/default (pose/from-rotation [0 0 0 1])))
  (let [pose (pose/from-rotation [1.0 2.0 3.0 4.0])]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [1.0 2.0 3.0 4.0] (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/from-rotation [1 2 3 4])]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [1.0 2.0 3.0 4.0] (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest from-euler-rotation-test
  (is (thrown? Throwable (pose/from-euler-rotation [1.0 2.0])))
  (is (thrown? Throwable (pose/from-euler-rotation [1.0 2.0 3.0 4.0])))
  (is (identical? pose/default (pose/from-euler-rotation nil)))
  (is (identical? pose/default (pose/from-euler-rotation [0.0 0.0 0.0])))
  (is (identical? pose/default (pose/from-euler-rotation [0 0 0])))
  (let [pose (pose/from-euler-rotation [0.0 90.0 0.0])]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [0.0 0.707 0.0 0.707] (round-numbers (:rotation pose))))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest from-scale-test
  (is (thrown? Throwable (pose/from-scale [1.0 2.0])))
  (is (thrown? Throwable (pose/from-scale [1.0 2.0 3.0 4.0])))
  (is (identical? pose/default (pose/from-scale nil)))
  (is (identical? pose/default (pose/from-scale [1.0 1.0 1.0])))
  (is (identical? pose/default (pose/from-scale [1 1 1])))
  (let [pose (pose/from-scale [1.0 2.0 3.0])]
    (is (identical? pose/default-translation (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (= [1.0 2.0 3.0] (:scale pose))))
  (let [pose (pose/from-scale [1 2 3])]
    (is (identical? pose/default-translation (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (= [1.0 2.0 3.0] (:scale pose)))))

(deftest translation-pose-test
  (is (thrown? Throwable (pose/translation-pose nil nil nil)))
  (is (identical? pose/default (pose/translation-pose 0.0 0.0 0.0)))
  (let [pose (pose/translation-pose 1.0 2.0 3.0)]
    (is (= [1.0 2.0 3.0] (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/translation-pose 1 2 3)]
    (is (= [1.0 2.0 3.0] (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest rotation-pose-test
  (is (thrown? Throwable (pose/rotation-pose nil nil nil nil)))
  (is (identical? pose/default (pose/rotation-pose 0.0 0.0 0.0 1.0)))
  (let [pose (pose/rotation-pose 1.0 2.0 3.0 4.0)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [1.0 2.0 3.0 4.0] (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/rotation-pose 1 2 3 4)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [1.0 2.0 3.0 4.0] (:rotation pose)))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest euler-rotation-pose-test
  (is (thrown? Throwable (pose/euler-rotation-pose nil nil nil)))
  (is (identical? pose/default (pose/euler-rotation-pose 0.0 0.0 0.0)))
  (let [pose (pose/euler-rotation-pose 90.0 90.0 90.0)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [0.707 0.707 0.0 0.0] (round-numbers (:rotation pose))))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/euler-rotation-pose 90 0 90)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (= [0.5 0.5 0.5 0.5] (round-numbers (:rotation pose))))
    (is (identical? pose/default-scale (:scale pose)))))

(deftest scale-pose-test
  (is (thrown? Throwable (pose/scale-pose nil nil nil)))
  (is (identical? pose/default (pose/scale-pose 1.0 1.0 1.0)))
  (let [pose (pose/scale-pose 1.0 2.0 3.0)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (= [1.0 2.0 3.0] (:scale pose))))
  (let [pose (pose/scale-pose 1 2 3)]
    (is (identical? pose/default-translation (:translation pose)))
    (is (identical? pose/default-rotation (:rotation pose)))
    (is (= [1.0 2.0 3.0] (:scale pose)))))

(deftest pre-multiply-test
  (is (thrown? Throwable (pose/pre-multiply pose/default nil)))
  (is (thrown? Throwable (pose/pre-multiply nil pose/default)))
  (is (identical? pose/default (pose/pre-multiply pose/default pose/default)))
  (let [original (pose/make [1.0 0.0 0.0]
                            [0.0 0.707 0.707 1.0]
                            [2.0 1.0 1.0])]
    (is (identical? original (pose/pre-multiply original (pose/make [0.0 0.0 0.0]
                                                                    [0.0 0.0 0.0 1.0]
                                                                    [1.0 1.0 1.0])))))
  (is (= (pose/translation-pose 3.0 0.0 0.0)
         (pose/pre-multiply (pose/translation-pose 1.0 0.0 0.0)
                            (pose/translation-pose 2.0 0.0 0.0))))
  (is (= (pose/make [6.0 20.0 56.0]
                    nil
                    [2.0 4.0 8.0])
         (pose/pre-multiply (pose/from-translation [3.0 5.0 7.0])
                            (pose/from-scale [2.0 4.0 8.0]))))
  (is (= (pose/euler-rotation-pose 40.0 0.0 0.0)
         (pose/pre-multiply (pose/euler-rotation-pose 10.0 0.0 0.0)
                            (pose/euler-rotation-pose 30.0 0.0 0.0))))
  (is (= (pose/euler-rotation-pose 0.0 -40.0 0.0)
         (pose/pre-multiply (pose/euler-rotation-pose 0.0 -10.0 0.0)
                            (pose/euler-rotation-pose 0.0 -30.0 0.0))))
  (is (= (pose/rotation-pose -0.6615634399999999 -0.24966735999999995 -0.2496673599999999 0.6615634400000001)
         (pose/pre-multiply (pose/rotation-pose -0.5 -0.5 -0.4999999999999999 0.5000000000000001)
                            (pose/rotation-pose 0.0 0.41189608 0.0 0.9112308))))
  (is (= [-90.0 135.0 0.0]
         (round-euler-rotation
           (pose/pre-multiply (pose/euler-rotation-pose 0.0 90.0 0.0)
                              (pose/euler-rotation-pose 45.0 0.0 90.0)))))
  (is (= [3.342 103.606 68.165]
         (round-euler-rotation
           (pose/pre-multiply (pose/euler-rotation-pose 45.0 45.0 45.0)
                              (pose/euler-rotation-pose 10.0 20.0 30.0)))))
  (let [pose (pose/pre-multiply pose/default
                                (pose/euler-rotation-pose -90.0 180.0 0.0))]
    (is (= [0.0 0.0 0.0] (:translation pose)))
    (is (= [0.0 0.707 0.707 0.0] (round-numbers (:rotation pose))))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/pre-multiply (pose/euler-rotation-pose 0.0 0.0 90.0)
                                (pose/euler-rotation-pose 0.0 0.0 90.0))]
    (is (= [0.0 0.0 0.0] (:translation pose)))
    (is (= [0.0 0.0 1.0 0.0] (round-numbers (:rotation pose))))
    (is (identical? pose/default-scale (:scale pose))))
  (let [pose (pose/pre-multiply (pose/translation-pose 10.0 20.0 30.0)
                                (pose/make [3.0 5.0 7.0]
                                           (pose/euler-rotation 90.0 0.0 0.0)
                                           [2.0 4.0 8.0]))]
    (is (= [23.0 -235.0 87.0] (round-numbers (:translation pose))))
    (is (= (pose/euler-rotation 90.0 0.0 0.0) (:rotation pose)))
    (is (= [2.0 4.0 8.0] (:scale pose)))))

(deftest matrix-test
  (is (thrown? Throwable (pose/matrix {:translation "Not a pose"})))
  (is (= (doto (Matrix4d.) (.setIdentity))
         (pose/matrix pose/default)))
  (is (= (math/clj->mat4 [1.0 2.0 3.0]
                         [0.0 0.707 0.707 0.0]
                         [2.0 1.0 1.0])
         (pose/matrix (pose/make [1.0 2.0 3.0]
                                 [0.0 0.707 0.707 0.0]
                                 [2.0 1.0 1.0])))))

(deftest accessors-test
  (is (= [1.0 2.0 3.0] (pose/translation-v3 (pose/translation-pose 1.0 2.0 3.0))))
  (is (= [1.0 2.0 3.0 0.0] (pose/translation-v4 (pose/translation-pose 1.0 2.0 3.0) 0.0)))
  (is (= [1.0 2.0 3.0 1.0] (pose/translation-v4 (pose/translation-pose 1.0 2.0 3.0) 1.0)))
  (is (= [1.0 2.0 3.0 4.0] (pose/rotation-q4 (pose/rotation-pose 1.0 2.0 3.0 4.0))))
  (is (= [0.0 180.0 90.0] (pose/euler-rotation-v3 (pose/euler-rotation-pose 0.0 180.0 90.0))))
  (is (= [0.0 180.0 90.0 0.0] (pose/euler-rotation-v4 (pose/euler-rotation-pose 0.0 180.0 90.0))))
  (is (= [1.0 2.0 3.0] (pose/scale-v3 (pose/scale-pose 1.0 2.0 3.0))))
  (is (= [1.0 2.0 3.0 1.0] (pose/scale-v4 (pose/scale-pose 1.0 2.0 3.0)))))

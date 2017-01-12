(ns integration.collada-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.collada :as collada]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest normals
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (workspace/file-resource workspace "/mesh/box_blender.dae")
                    io/input-stream
                    collada/->mesh-set)]
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  (->> (get-in content [:mesh-entries 0 :meshes 0 :normals])
                    (partition 3)))))))

(deftest texcoords
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (workspace/file-resource workspace "/mesh/plane.dae")
                    io/input-stream
                    collada/->mesh-set)]
      (let [c (get-in content [:mesh-entries 0 :meshes 0])
            zs (map #(nth % 2) (partition 3 (:positions c)))
            vs (map second (partition 2 (:texcoord0 c)))]
        (is (= vs (map (fn [y] (- 1.0 (* (+ y 1.0) 0.5))) zs)))))))

(deftest comma-decimal-points-throws-number-format-exception
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          malformed-resource (workspace/file-resource workspace "/mesh/test_autodesk_dae.dae")]
      (is (thrown? NumberFormatException (-> malformed-resource io/input-stream collada/->mesh-set))))))

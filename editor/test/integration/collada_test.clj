(ns integration.collada-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.spine :as spine]
            [editor.types :as types]
            [editor.collada :as collada])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]
           [javax.vecmath Point3d]))

(deftest normals
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (workspace/file-resource workspace "/mesh/box_blender.dae")
                    io/input-stream
                    collada/->mesh)]
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  (->> (get-in content [:components 0 :normals])
                    (partition 3)))))))

(deftest texcoords
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (workspace/file-resource workspace "/mesh/plane.dae")
                    io/input-stream
                    collada/->mesh)]
      (let [c (get-in content [:components 0])
            ys (map second (partition 3 (:positions c)))
            vs (map second (partition 2 (:texcoord0 c)))]
        (is (every? identity (map (fn [y v] (= v (- 1.0 (* (+ y 1.0) 0.5)))) ys vs)))))))

(deftest comma-decimal-points
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (workspace/file-resource workspace "/mesh/test_autodesk_dae.dae")
                    io/input-stream
                    collada/->mesh)]
      (is (not-empty (get-in content [:components 0 :normals]))))))

(ns integration.collada-scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.collada-scene :as collada-scene]
            [editor.math :as math]
            [editor.types :as types])
  (:import [javax.vecmath Point3d]))

(deftest aabb
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/mesh/test.dae")
          aabb (g/node-value node-id :aabb)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)]
      (is (< 10 (.distance max min))))))

(deftest vbs
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/mesh/test.dae")
          mesh-set (g/node-value node-id :mesh-set)
          vbs (collada-scene/mesh-set->vbs (math/->mat4) mesh-set)]
      (is (= 1 (count vbs)))
      (let [vb (first vbs)]
        (is (= (count vb) (count (get-in mesh-set [:mesh-entries 0 :meshes 0 :indices]))))))))

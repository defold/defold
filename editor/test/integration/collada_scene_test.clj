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
          mesh (first (g/node-value node-id :meshes))
          scene (g/node-value node-id :scene)
          vb (-> (get-in scene [:renderable :user-data :vbuf])
               (collada-scene/mesh->vb! (math/->mat4) mesh))]
      (is (= (count vb) (alength (get mesh :indices)))))))

(deftest invalid-scene
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/mesh/invalid.dae")
          scene (g/node-value node-id :scene)]
      (is (g/error? scene)))))

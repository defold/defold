(ns integration.collada-scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.collada-scene :as collada-scene]
            [editor.math :as math]
            [editor.types :as types])
  (:import [javax.vecmath Point3d]))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          aabb (g/node-value node-id :aabb)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)
          dist (.distance max min)]
      (is (and (> 20 dist) (< 10 dist))))))

(deftest vbs
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          mesh (first (g/node-value node-id :meshes))
          scene (g/node-value node-id :scene)
          user-data (get-in scene [:renderable :user-data])
          vb (-> (collada-scene/->vtx-pos-nrm-tex (alength (get-in user-data [:meshes 0 :indices])))
               (collada-scene/mesh->vb! (math/->mat4) mesh (get user-data :scratch-arrays)))]
      (is (= (count vb) (alength (get mesh :indices)))))))

(deftest invalid-scene
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/invalid.dae")
          scene (g/node-value node-id :scene)]
      (is (g/error? scene)))))

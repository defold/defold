(ns integration.collision-object-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.collision-object :as collision-object]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util])
  (:import [editor.types Region]
           [javax.vecmath Point3d Matrix4d]))

(defn- outline-seq
  [outline]
  (map :label (tree-seq :children :children outline)))

(deftest new-collision-object
  (testing "A new collision object"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/collision_object/new.collisionobject")
            scene     (g/node-value node-id :scene)
            outline   (g/node-value node-id :node-outline)]
        (is (not (nil? scene)))
        (is (empty? (:children scene)))
        (is (empty? (:children outline)))))))

(deftest collision-object-with-three-shapes
  (testing "A collision object with shapes"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")
            outline   (g/node-value node-id :node-outline)
            scene     (g/node-value node-id :scene)]
        (is (= 3 (count (:children scene))))
        (is (= ["Collision Object" "Sphere" "Box" "Capsule"] (outline-seq outline)))))))

(deftest add-shapes
  (testing "Adding a sphere"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
        (project/select! project [node-id])
        (test-util/handler-run :add [{:name :global :env {:selection [node-id]}}] {:shape-type :type-sphere})
        (let [outline (g/node-value node-id :node-outline)]
          (is (= 4 (count (:children outline))))
          (is (= "Sphere" (last (outline-seq outline)))))))))

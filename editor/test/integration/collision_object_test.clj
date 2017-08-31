(ns integration.collision-object-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.collection :as collection]
            [editor.collision-object :as collision-object]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
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
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/new.collisionobject")
            scene     (g/node-value node-id :scene)
            outline   (g/node-value node-id :node-outline)]
        (is (not (nil? scene)))
        (is (empty? (:children scene)))
        (is (empty? (:children outline)))))))

(deftest collision-object-with-three-shapes
  (testing "A collision object with shapes"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")
            outline   (g/node-value node-id :node-outline)
            scene     (g/node-value node-id :scene)]
        (is (= 3 (count (:children scene))))
        (is (= ["Collision Object" "Box" "Capsule" "Sphere"] (outline-seq outline)))))))

(deftest add-shapes
  (testing "Adding a sphere"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
        (app-view/select! app-view [node-id])
        (test-util/handler-run :add [{:name :workbench :env {:selection [node-id] :app-view app-view}}] {:shape-type :type-sphere})
        (let [outline (g/node-value node-id :node-outline)]
          (is (= 4 (count (:children outline))))
          (is (= "Sphere" (last (outline-seq outline)))))))))

(deftest validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
      (testing "collision object"
               (test-util/with-prop [node-id :mass 0]
                 (is (g/error? (test-util/prop-error node-id :mass))))
               (let [r (workspace/resolve-workspace-resource workspace "/nope.convexshape")]
                 (test-util/with-prop [node-id :collision-shape r]
                   (is (g/error? (test-util/prop-error node-id :collision-shape))))))
      (doseq [[type index props] [["box" 0 {:dimensions [-1 1 1]}]
                                  ["capsule" 1 {:diameter -1
                                                :height -1}]
                                  ["sphere" 2 {:diameter -1}]]]
        (testing type
               (let [shape (:node-id (test-util/outline node-id [index]))]
                 (doseq [[prop value] props]
                   (test-util/with-prop [shape prop value]
                    (is (g/error? (test-util/prop-error shape prop)))))))))))

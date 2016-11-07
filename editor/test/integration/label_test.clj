(ns integration.label-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.math :as math]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [clojure.string :as string]))

(deftest label-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (project/get-resource-node project "/label/test.label")]
      (doseq [[prop cases] [[:font {"no font" ""
                                    "unknown font" "/fonts/unknown.font"}]
                            [:material {"no material" ""
                                        "unknown material" "/materials/unknown.material"}]]
              [case path] cases]
        (testing case
          (test-util/with-prop [node-id prop (workspace/resolve-workspace-resource workspace path)]
                               (is (g/error? (test-util/prop-error node-id prop)))))))))

(deftest label-aabb
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (project/get-resource-node project "/label/test.label")]
      (let [aabb (g/node-value node-id :aabb)
            [x y z] (mapv - (math/vecmath->clj (:max aabb)) (math/vecmath->clj (:min aabb)))]
        (is (< 0.0 x))
        (is (< 0.0 y))
        (is (= 0.0 z))))))

(deftest label-scene
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (project/get-resource-node project "/label/test.label")]
      (let [scene (g/node-value node-id :scene)
            aabb (g/node-value node-id :aabb)]
        (is (= aabb (:aabb scene)))
        (is (= node-id (:node-id scene)))
        (is (= node-id (some-> scene :renderable :select-batch-key)))
        (is (= :blend-mode-alpha (some-> scene :renderable :batch-key :blend-mode)))
        (is (= "Label" (some-> scene :renderable :user-data :text-data :text-layout :lines first)))
        (is (string/includes? (some-> scene :renderable :user-data :material-shader :verts) "gl_Position"))
        (is (string/includes? (some-> scene :renderable :user-data :material-shader :frags) "gl_FragColor"))))))

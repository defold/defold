(ns integration.spine-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.spine :as spine])
  (:import [javax.vecmath Point3d]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(defn- outline-label [nid path]
  (get (test-util/outline nid path) :label))

(deftest key->curve-data-test
  (testing "well-formed input"
    (are [expected input]
      (= expected (spine/key->curve-data {"curve" input}))
      nil "stepped"
      [0 0 1 1] "linear"
      [0.1 0.2 0.3 0.4] [0.1, 0.2, 0.3, 0.4]))
  (testing "malformed input falls back to linear"
    (are [expected input]
      (= expected (spine/key->curve-data {"curve" input}))
      [0 0 1 1] nil
      [0 0 1 1] ""
      [0 0 1 1] "other"
      [0 0 1 1] []
      [0 0 1 1] [0 0 0]
      [0 0 1 1] [0 0 0 0 0]
      [0 0 1 1] #{0.1 0.2 0.3 0.4}
      [0 0 1 1] {:a 1 :b 2 :c 3 :d 4})))

(deftest load-spine-scene-json
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/spine/player/export/spineboy.json")]
      (is (= "hip" (outline-label node-id [0])))
      (is (= "front_thigh" (outline-label node-id [0 0])))
      (is (= "front_shin" (outline-label node-id [0 0 0])))
      (let [root (:skeleton (g/node-value node-id :structure))]
        (is (some? (:transform root)))
        (is (= "hip" (:name root)))))))

(deftest load-spine-scene
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/spine/player/spineboy.spinescene")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.))))
      (is (= "hip" (outline-label node-id [0]))))))

(deftest load-spine-model
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/spine/player/spineboy.spinemodel")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.))))
      (is (= "hip" (outline-label node-id [0]))))))

(deftest spine-scene-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/spine/player/spineboy.spinescene")]
      (is (nil? (test-util/prop-error node-id :spine-json)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.json")]]
        (test-util/with-prop [node-id :spine-json v]
          (is (g/error? (test-util/prop-error node-id :spine-json)))))
      (is (nil? (test-util/prop-error node-id :atlas)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.atlas")]]
        (test-util/with-prop [node-id :atlas v]
          (is (g/error? (test-util/prop-error node-id :atlas))))))))

(deftest spine-model-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/spine/player/spineboy.spinemodel")]
      (is (nil? (test-util/prop-error node-id :spine-json)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.spinescene")]]
        (test-util/with-prop [node-id :spine-scene v]
          (is (g/error? (test-util/prop-error node-id :spine-scene)))))
      (is (nil? (test-util/prop-error node-id :material)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
        (test-util/with-prop [node-id :material v]
          (is (g/error? (test-util/prop-error node-id :material)))))
      (is (nil? (test-util/prop-error node-id :default-animation)))
      (doseq [v ["no_such_anim"]]
        (test-util/with-prop [node-id :default-animation v]
          (is (g/error? (test-util/prop-error node-id :default-animation)))))
      (is (nil? (test-util/prop-error node-id :skin)))
      (doseq [v ["no_such_skin"]]
        (test-util/with-prop [node-id :skin v]
          (is (g/error? (test-util/prop-error node-id :skin))))))))

(deftest spine-model-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/spine/reload.spinemodel")]
      (test-util/test-uses-assigned-multi-texture-material workspace project node-id
                                                           :material
                                                           [:renderable :user-data :shader]
                                                           [:renderable :user-data :gpu-textures]))))

(ns integration.spine-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.spine :as spine])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]
           [javax.vecmath Point3d]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(defn- outline-label [nid path]
  (get (test-util/outline nid path) :label))

(deftest load-spine-scene-json
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/export/spineboy.json")]
      (is (= "hip" (outline-label node-id [0])))
      (is (= "front_thigh" (outline-label node-id [0 0])))
      (is (= "front_shin" (outline-label node-id [0 0 0])))
      (let [root (:skeleton (g/node-value node-id :structure))]
        (is (some? (:transform root)))
        (is (= "hip" (:name root)))))))

(deftest load-spine-scene
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinescene")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.))))
      (is (= "hip" (outline-label node-id [0]))))))

(deftest load-spine-model
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinemodel")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.))))
      (is (= "hip" (outline-label node-id [0]))))))

(deftest spine-scene-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinescene")]
      (is (nil? (test-util/prop-error node-id :spine-json)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.json")]]
        (test-util/with-prop [node-id :spine-json v]
          (is (g/error? (test-util/prop-error node-id :spine-json)))))
      (is (nil? (test-util/prop-error node-id :atlas)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.atlas")]]
        (test-util/with-prop [node-id :atlas v]
          (is (g/error? (test-util/prop-error node-id :atlas))))))))

(deftest spine-model-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinemodel")]
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

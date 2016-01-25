(ns integration.spine-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.project :as project]
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

(deftest load-spine-scene
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinescene")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.)))))))

(deftest load-spine-model
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/spine/player/spineboy.spinemodel")]
      (is (< 0.0 (.distanceSquared (geom/aabb-extent (g/node-value node-id :aabb)) (Point3d.)))))))

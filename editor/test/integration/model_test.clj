(ns integration.model-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.spine :as spine]
            [editor.types :as types]
            [editor.properties :as properties])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]
           [javax.vecmath Point3d]))

(deftest aabb
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/model/test.model")
          aabb (g/node-value node-id :aabb)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)]
      (is (< 10 (.distance max min))))))

(deftest textures
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/model/test.model")]
      (let [original-texture (first (test-util/prop node-id :textures))
            t [original-texture nil nil]]
        (test-util/prop! node-id :textures t)
        (is (= t (test-util/prop node-id :textures)))
        (let [p (-> [(g/node-value node-id :_properties)]
                  (properties/coalesce)
                  :properties
                  :texture2)]
          (properties/set-values! p [original-texture]))))))

(textures)

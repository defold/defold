(ns integration.game-object-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.game-object :as game-object]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util])
  (:import [editor.types Region]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]))

(defn- build-error? [node-id]
  (g/error? (g/node-value node-id :build-targets)))

(deftest validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          go-id   (test-util/resource-node project "/game_object/props.go")
          comp-id   (:node-id (test-util/outline go-id [0]))]
      (testing "component ref instance"
               (is (not (build-error? go-id)))
               (is (nil? (test-util/prop-error comp-id :path)))
               (test-util/with-prop [comp-id :path {:resource nil :overrides []}]
                 (is (g/error? (test-util/prop-error comp-id :path))))
               (let [not-found (workspace/resolve-workspace-resource workspace "/not_found.script")]
                 (test-util/with-prop [comp-id :path {:resource not-found :overrides []}]
                   (is (g/error? (test-util/prop-error comp-id :path))))))
      (testing "component embedded instance"
               (let [r-type (workspace/get-resource-type workspace "factory")]
                 (game-object/add-embedded-component-handler {:_node-id go-id :resource-type r-type})
                 (let [factory (:node-id (test-util/outline go-id [0]))]
                   (test-util/with-prop [factory :id "script"]
                     (is (g/error? (test-util/prop-error factory :id)))
                     (is (g/error? (test-util/prop-error comp-id :id)))
                     (is (build-error? go-id)))))))))

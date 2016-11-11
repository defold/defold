(ns integration.game-object-test
  (:require [clojure.data :as data]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.game-object :as game-object]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import (com.dynamo.gameobject.proto GameObject$PrototypeDesc)
           (java.io StringReader)))

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

(defn- save-data
  [project resource]
  (first (filter #(= resource (:resource %))
                 (project/save-data project))))

(deftest embedded-components
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          resource-types (game-object/embeddable-component-resource-types workspace)
          save-data (partial save-data project)
          make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          add-component! (partial test-util/add-embedded-component! project)
          go-id (project/get-resource-node project "/game_object/test.go")
          go-resource (g/node-value go-id :resource)]
      (doseq [resource-type resource-types]
        (testing (:label resource-type)
          (with-open [_ (make-restore-point!)]
            (add-component! resource-type go-id)
            (let [saved-string (:content (save-data go-resource))
                  saved-embedded-components (g/node-value go-id :embed-ddf)
                  loaded-embedded-components (:embedded-components (protobuf/read-text GameObject$PrototypeDesc (StringReader. saved-string)))
                  [only-in-saved only-in-loaded] (data/diff saved-embedded-components loaded-embedded-components)]
              (is (nil? only-in-saved))
              (is (nil? only-in-loaded)))))))))

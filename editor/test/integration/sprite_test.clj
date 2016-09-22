(ns integration.sprite-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.tile-source :as tile-source]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest replacing-sprite-image-replaces-dep-build-targets
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (project/get-resource-node project "/logic/session/pow.sprite")
          old-image (g/node-value node-id :image)]
      (let [old-sources (g/sources-of node-id :dep-build-targets)]
        (g/transact (g/set-property node-id :image (workspace/find-resource workspace "/switcher/switcher.atlas")))
        (is (= (count old-sources) (count (g/sources-of node-id :dep-build-targets))))
        (is (not (= (set old-sources) (set (g/sources-of node-id :dep-build-targets)))))
        (g/transact (g/set-property node-id :image old-image))
        (is (= (count old-sources) (count (g/sources-of node-id :dep-build-targets))))
        (is (= (set old-sources) (set (g/sources-of node-id :dep-build-targets))))))))

(deftest sprite-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (project/get-resource-node project "/sprite/atlas.sprite")]
      (testing "unknown atlas"
               (test-util/with-prop [node-id :image (workspace/resolve-workspace-resource workspace "/graphics/unknown_atlas.atlas")]
                 (is (g/error? (test-util/prop-error node-id :image)))))
      (testing "invalid atlas"
               (test-util/with-prop [node-id :image (workspace/resolve-workspace-resource workspace "/graphics/img_not_found.atlas")]
                 (is (g/error? (test-util/prop-error node-id :image))))))))

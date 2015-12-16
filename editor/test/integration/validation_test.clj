(ns integration.validation-test
  (:require [clojure.test :refer :all]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.project :as project]
            [editor.workspace :as workspace]            
            [support.test-support :refer [with-clean-system undo-stack]]
            [integration.test-util :as test-util]
            [service.log :as log]))

(def ^:dynamic *temp-project-path*)

(defn- setup [ws-graph project-path]
   (let [workspace (test-util/setup-workspace! ws-graph project-path)
         project (test-util/setup-project! workspace)]
     [workspace project]))

(deftest missing-tilesource-image
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world "resources/test_project"))
          node-id (project/get-resource-node project "/tilesource/invalid.tilesource")]
      (is (g/error? (g/node-value node-id :image)))
      ;; collision being nil is not an error
      (is (not (g/error? (g/node-value node-id :collision))))
      (is (nil? (g/node-value node-id :collision))))))

(deftest replacing-sprite-image-replaces-dep-build-targets
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world "resources/test_project"))
          node-id (project/get-resource-node project "/logic/session/pow.sprite")
          old-image (g/node-value node-id :image)]
      (let [old-sources (g/sources-of node-id :dep-build-targets)]
        (g/transact (g/set-property node-id :image (workspace/find-resource workspace "/switcher/switcher.atlas")))
        (is (= (count old-sources) (count (g/sources-of node-id :dep-build-targets))))
        (is (not (= (set old-sources) (set (g/sources-of node-id :dep-build-targets)))))
        (g/transact (g/set-property node-id :image old-image))
        (is (= (count old-sources) (count (g/sources-of node-id :dep-build-targets))))
        (is (= (set old-sources) (set (g/sources-of node-id :dep-build-targets))))))))





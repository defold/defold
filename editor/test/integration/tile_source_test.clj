(ns integration.tile-source-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.tile-source :as tile-source]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest tile-source-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/tilesource/valid.tilesource")]
      (testing "image dim error"
        (test-util/with-prop [node-id :collision (workspace/resolve-workspace-resource workspace "/graphics/paddle.png")]
         (is (some? (test-util/prop-error node-id :image)))
         (is (= (test-util/prop-error node-id :image)
                (test-util/prop-error node-id :collision)))))
      (testing "tile dim error"
               (test-util/with-prop [node-id :tile-width 5000]
                 (is (some? (test-util/prop-error node-id :image)))
                 (is (= (test-util/prop-error node-id :image)
                        (test-util/prop-error node-id :collision)
                        (test-util/prop-error node-id :tile-width)
                        (test-util/prop-error node-id :tile-margin)))))
      (testing "save and build data errors"
               (test-util/with-prop [node-id :tile-width -1]
                 (is (g/error? (g/node-value node-id :build-targets)))
                 (is (not (g/error? (g/node-value node-id :save-data)))))))))

(defn- add-collision-group! [app-view node-id]
  (tile-source/add-collision-group-node! node-id (fn [node-ids] (app-view/select app-view node-ids)))
  (first (g/node-value app-view :selected-node-ids)))

(defn- add-animation! [app-view node-id]
  (tile-source/add-animation-node! node-id (fn [node-ids] (app-view/select app-view node-ids)))
  (first (g/node-value app-view :selected-node-ids)))

(deftest collision-group-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")]
      (app-view/select! app-view [node-id])
      (testing "collision-group-id"
               (let [group (add-collision-group! app-view node-id)]
                 (test-util/with-prop [group :id ""]
                   (is (g/error? (test-util/prop-error group :id))))))
      (testing "collision-group-max"
               (let [groups (mapv (fn [_] (add-collision-group! app-view node-id)) (range 17))]
                 (is (every? #(test-util/prop-error % :id) groups))
                 (g/transact
                   (for [group groups]
                     (g/delete-node group))))))))

(deftest animation-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")]
      (app-view/select! app-view [node-id])
      (testing "animation-id"
               (let [anim (add-animation! app-view node-id)]
                 (test-util/with-prop [anim :id ""]
                   (is (some? (test-util/prop-error anim :id))))))
      (testing "animation-tile-ranges"
               (let [anim (add-animation! app-view node-id)]
                 (test-util/with-prop [anim :start-tile 2000]
                   (is (some? (test-util/prop-error anim :start-tile)))
                   (is (g/error? (g/node-value node-id :build-targets)))
                   (is (not (g/error? (g/node-value node-id :save-data)))))
                 (test-util/with-prop [anim :end-tile 2000]
                   (is (some? (test-util/prop-error anim :end-tile)))))))))

(deftest missing-tilesource-image
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/tilesource/invalid.tilesource")]
      (is (g/error? (test-util/prop-error node-id :image)))
      ;; collision being nil is not an error
      (is (not (g/error? (test-util/prop-error node-id :collision))))
      (is (nil? (g/node-value node-id :collision))))))

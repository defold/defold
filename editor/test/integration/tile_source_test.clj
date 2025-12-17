;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.tile-source-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.texture-util :as texture-util]
            [editor.tile-source :as tile-source]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

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

(deftest sprite-trim-mode-image-io-error
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/image_project")
          project (test-util/setup-project! workspace)
          tile-source (project/get-resource-node project "/main/main.tilesource")
          image-file (io/as-file (g/node-value tile-source :image))
          image-bytes (fs/read-bytes image-file)
          texture-set-data-generator (g/node-value tile-source :texture-set-data-generator)
          packed-image-generator (g/node-value tile-source :packed-image-generator)]

      (testing "Initial project state"
        (is (not= :sprite-trim-mode-off (g/node-value tile-source :sprite-trim-mode)))
        (testing "Generators"
          (is (not (g/error? (texture-util/call-generator texture-set-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-image-generator)))))
        (testing "Graph"
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Corrupting referenced image file"
        (test-support/spit-until-new-mtime image-file "This is no longer an image file.")
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator texture-set-data-generator)))
          (is (g/error? (texture-util/call-generator packed-image-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Restoring referenced image file"
        (test-support/write-until-new-mtime image-file image-bytes)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (not (g/error? (texture-util/call-generator texture-set-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-image-generator)))))
        (testing "Graph before resource-sync"
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Deleting referenced image file"
        (fs/delete! image-file)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator texture-set-data-generator)))
          (is (g/error? (texture-util/call-generator packed-image-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))))))

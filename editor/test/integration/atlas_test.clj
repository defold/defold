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

(ns integration.atlas-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(deftest valid-fps
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/atlas.atlas")
          anim (:node-id (test-util/outline node-id [0]))]
      (is (nil? (test-util/prop-error anim :fps)))
      (test-util/prop! anim :fps -1)
      (is (g/error? (test-util/prop-error anim :fps))))))

(deftest img-not-found
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/img_not_found.atlas")
          img (:node-id (test-util/outline node-id [0]))]
      (is (g/error? (g/node-value img :animation))))))

(deftest empty-anim
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/empty_anim.atlas")
          ddf-texture-set (g/node-value node-id :texture-set)
          animation-ids-in-ddf (mapv :id (:animations ddf-texture-set))]
      (is (= ["ball_anim"
              "block_anim"
              "pow_anim"]
             animation-ids-in-ddf)))))

(deftest rename-anim
  (test-util/with-loaded-project "test/resources/image_project"
    (let [atlas (project/get-resource-node project "/main/rename.atlas")
          ddf-texture-set (g/node-value atlas :texture-set)
          animation-ids-in-ddf (into #{}
                                     (map :id)
                                     (:animations ddf-texture-set))]
      (is (= #{"ball"
               "diamond_dogs"
               "test_anim"}
             animation-ids-in-ddf)))))

(deftest sprite-trim-mode-image-io-error
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/image_project")
          project (test-util/setup-project! workspace)
          atlas (project/get-resource-node project "/main/main.atlas")
          atlas-image (:node-id (test-util/outline atlas [0]))
          image-file (io/as-file (g/node-value atlas-image :image))
          image-bytes (fs/read-bytes image-file)
          layout-data-generator (g/node-value atlas :layout-data-generator)
          packed-page-images-generator (g/node-value atlas :packed-page-images-generator)]

      (testing "Initial project state"
        (is (not= :sprite-trim-mode-off (g/node-value atlas-image :sprite-trim-mode)))
        (testing "Generators"
          (is (not (g/error? (texture-util/call-generator layout-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-page-images-generator)))))
        (testing "Graph"
          (is (not (g/error? (g/node-value atlas :scene))))
          (is (not (g/error? (g/node-value atlas :build-targets))))
          (is (not (g/error? (g/node-value atlas :save-data))))))

      (testing "Corrupting referenced image file"
        (test-support/spit-until-new-mtime image-file "This is no longer an image file.")
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator layout-data-generator)))
          (is (g/error? (texture-util/call-generator packed-page-images-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data))))))

      (testing "Restoring referenced image file"
        (test-support/write-until-new-mtime image-file image-bytes)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (not (g/error? (texture-util/call-generator layout-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-page-images-generator)))))
        (testing "Graph before resource-sync"
          (is (not (g/error? (g/node-value atlas :scene))))
          (is (not (g/error? (g/node-value atlas :build-targets))))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (not (g/error? (g/node-value atlas :scene))))
          (is (not (g/error? (g/node-value atlas :build-targets))))
          (is (not (g/error? (g/node-value atlas :save-data))))))

      (testing "Deleting referenced image file"
        (fs/delete! image-file)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator layout-data-generator)))
          (is (g/error? (texture-util/call-generator packed-page-images-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))))))

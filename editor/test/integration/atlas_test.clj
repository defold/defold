;; Copyright 2020-2022 The Defold Foundation
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
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(deftest valid-fps
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/atlas.atlas")
          anim (:node-id (test-util/outline node-id [2]))]
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

(deftest sprite-trim-mode-image-io-error
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/small_project")
          project (test-util/setup-project! workspace)
          atlas (project/get-resource-node project "/main/logo.atlas")
          atlas-image (:node-id (test-util/outline atlas [0]))
          image-file (io/as-file (g/node-value atlas-image :image))
          image-bytes (fs/read-bytes image-file)]

      ;; Enable sprite trimming for our AtlasImage.
      (g/set-property! atlas-image :sprite-trim-mode :sprite-trim-mode-6)
      (is (not (g/error? (g/node-value atlas :scene))))

      (testing "Corrupting referenced image file"
        (test-support/spit-until-new-mtime image-file "This is no longer an image file.")
        (testing "Before resource-sync"
          (g/clear-system-cache!)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "After resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data))))))

      (testing "Restoring referenced image file"
        (test-support/write-until-new-mtime image-file image-bytes)
        (testing "Before resource-sync"
          (g/clear-system-cache!)
          (is (not (g/error? (g/node-value atlas :scene))))
          (is (not (g/error? (g/node-value atlas :build-targets))))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "After resource-sync"
          (workspace/resource-sync! workspace)
          (is (not (g/error? (g/node-value atlas :scene))))
          (is (not (g/error? (g/node-value atlas :build-targets))))
          (is (not (g/error? (g/node-value atlas :save-data))))))

      (testing "Deleting referenced image file"
        (fs/delete! image-file)
        (testing "Before resource-sync"
          (g/clear-system-cache!)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))
        (testing "After resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value atlas :scene)))
          (is (g/error? (g/node-value atlas :build-targets)))
          (is (not (g/error? (g/node-value atlas :save-data)))))))))

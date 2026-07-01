;; Copyright 2020-2026 The Defold Foundation
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
            [editor.resource :as resource]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(defn- image-orders [image-node-ids]
  (g/with-auto-evaluation-context evaluation-context
    (mapv #(g/node-value % :order evaluation-context) image-node-ids)))

(defn- animation-image-paths [atlas]
  (->> (g/node-value atlas :save-value)
       :animations
       first
       :images
       (mapv :image)))

(defn- animation-output-image-paths [animation-node]
  (mapv (comp resource/proj-path :path)
        (:images (g/node-value animation-node :animation))))

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

(deftest reorder-animation-images-uses-order
  (test-util/with-scratch-project "test/resources/test_project"
    (test-util/make-resource! workspace "/graphics/reorder.atlas"
                              {:animations [{:id "anim"
                                             :images [{:image "/graphics/ball.png"}
                                                      {:image "/graphics/block.png"}
                                                      {:image "/graphics/pow.png"}]}]})
    (workspace/resource-sync! workspace)
    (let [atlas (project/get-resource-node project "/graphics/reorder.atlas")
          animation (:node-id (test-util/outline atlas [0]))
          image-node-ids (vec (g/node-value animation :nodes))
          [_ block-image] image-node-ids
          block-selection-context [{:name :workbench
                                    :env {:selection [block-image]}}]]
      (is (= [0 1 2] (image-orders image-node-ids)))
      (is (= ["/graphics/ball.png"
              "/graphics/block.png"
              "/graphics/pow.png"]
             (animation-image-paths atlas)))
      (is (= ["/graphics/ball.png"
              "/graphics/block.png"
              "/graphics/pow.png"]
             (animation-output-image-paths animation)))

      (is (test-util/handler-enabled? :edit.reorder-up block-selection-context {}))
      (test-util/handler-run :edit.reorder-up block-selection-context {})

      (is (= image-node-ids (vec (g/node-value animation :nodes))))
      (is (= [1 0 2] (image-orders image-node-ids)))
      (is (= ["/graphics/block.png"
              "/graphics/ball.png"
              "/graphics/pow.png"]
             (animation-image-paths atlas)))
      (is (= ["/graphics/block.png"
              "/graphics/ball.png"
              "/graphics/pow.png"]
             (animation-output-image-paths animation)))
      (is (not (test-util/handler-enabled? :edit.reorder-up block-selection-context {})))
      (is (test-util/handler-enabled? :edit.reorder-down block-selection-context {}))

      (test-util/handler-run :edit.reorder-down block-selection-context {})

      (is (= image-node-ids (vec (g/node-value animation :nodes))))
      (is (= [0 1 2] (image-orders image-node-ids)))
      (is (= ["/graphics/ball.png"
              "/graphics/block.png"
              "/graphics/pow.png"]
             (animation-image-paths atlas)))
      (is (= ["/graphics/ball.png"
              "/graphics/block.png"
              "/graphics/pow.png"]
             (animation-output-image-paths animation))))))

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

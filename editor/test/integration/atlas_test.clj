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
            [editor.atlas :as atlas]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.gl.texture :as texture]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.murmur :as murmur]))

;; Verifies atlas settings select the native texture target and invalidate it
;; when paging changes, including retaining an array for a single packed page.
(deftest native-atlas-texture-paging-test
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          project (test-util/setup-project! workspace)
          resource (test-util/make-resource!
                     workspace "/main/native-texture.atlas"
                     {:margin 0
                      :extrude-borders 0
                      :inner-padding 0
                      :images [{:image "/images/ball_anim.png"}
                               {:image "/images/diamond.png"}]})
          _ (workspace/resource-sync! workspace)
          atlas (project/get-resource-node project resource)]
      (doseq [[page-size expected-page-count] [[[0 0] 0]
                                               [[32 32] 1]
                                               [[16 16] 2]
                                               [[0 0] 0]]]
        (testing (str "Maximum page size " page-size)
          (test-util/prop! atlas :max-page-size page-size)
          (let [native-texture (g/node-value atlas :gpu-texture)]
            (is (= expected-page-count (g/node-value atlas :texture-page-count)))
            (is (= [0] (:texture-units native-texture)))
            (is (= [4] (:texture-units (texture/set-base-unit native-texture 4))))
            (if (zero? expected-page-count)
              (is (= ::texture/texture (:cache-id native-texture)))
              (do
                (is (= ::texture/array-texture (:cache-id native-texture)))
                (is (= expected-page-count (count (first (:texture-request-datas native-texture)))))))))))))

;; Verifies the runtime ES2 page limit still blocks builds while native atlas,
;; GUI, particle and collection previews remain available above eight pages.
(deftest native-atlas-preview-independent-of-es2-page-limit-test
  (test-util/with-loaded-project "test/resources/max_paged_count_project"
    (let [atlas (test-util/resource-node project "/assets/paged_atlas.atlas")
          game-project (test-util/resource-node project "/game.project")
          preview-texture (g/valid-node-value atlas :gpu-texture)]
      (is (= 9 (g/valid-node-value atlas :texture-page-count)))
      (is (texture/array-texture? preview-texture))
      (is (= [0] (:texture-units preview-texture)))
      (doseq [exclude-gles-sm100 [false true false]]
        (test-util/set-setting! game-project ["shader" "exclude_gles_sm100"] exclude-gles-sm100)
        (is (= (not exclude-gles-sm100) (boolean (g/error? (g/node-value atlas :build-targets)))))
        (is (identical? preview-texture (g/valid-node-value atlas :gpu-texture)))
        (doseq [resource-path ["/assets/paged_atlas.atlas"
                               "/assets/paged_gui.gui"
                               "/assets/paged_particlefx.particlefx"
                               "/main/main.collection"]]
          (testing resource-path
            (is (map? (g/valid-node-value (test-util/resource-node project resource-path) :scene)))))))))

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

(deftest image-name-hashes-follow-frames-across-atlases
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          project (test-util/setup-project! workspace)
          atlas (project/get-resource-node project "/main/rename.atlas")
          reordered-resource (test-util/make-resource!
                               workspace "/main/reordered.atlas"
                               (update (g/node-value atlas :save-value) :images #(vec (rseq %))))
          _ (workspace/resource-sync! workspace)
          original (g/node-value atlas :texture-set)
          reordered (g/node-value (project/get-resource-node project reordered-resource) :texture-set)]
      (doseq [[source target] [[original reordered] [reordered original]]
              [animation-id image-id] [["diamond_dogs" "diamond_dogs"]
                                       ["ball" "ball"]
                                       ["test_anim" "ball"]]]
        (testing (str "Animation: " animation-id)
          (let [source-animation (coll/first-where #(= animation-id (:id %)) (:animations source))
                source-frame (get (:frame-indices source) (:start source-animation))
                source-name-hash (get (:image-name-hashes source) source-frame)
                ;; Match the runtime's lookup, but get the expected frame from
                ;; the target's named animation, independently of its hashes.
                target-frame-by-hash (zipmap (:image-name-hashes target) (:frame-indices target))
                target-animation (coll/first-where #(= animation-id (:id %)) (:animations target))
                expected-frame (get (:frame-indices target) (:start target-animation))]
            (is (= (murmur/hash64 image-id) source-name-hash))
            (is (= expected-frame (target-frame-by-hash source-name-hash)))))))))

(deftest image-variants-share-a-rect-but-keep-their-own-geometry
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          project (test-util/setup-project! workspace)
          resource (test-util/make-resource!
                     workspace "/main/variants.atlas"
                     {:margin 0
                      :extrude-borders 0
                      :inner-padding 0
                      :images [{:image "/images/diamond.png"}]
                      :animations [{:id "variant"
                                    :images [{:image "/images/diamond.png"
                                              :pivot-x 0.0
                                              :sprite-trim-mode :sprite-trim-mode-6}]}]})
          _ (workspace/resource-sync! workspace)
          atlas (project/get-resource-node project resource)
          image->rect (g/node-value atlas :image->rect)
          rects (coll/vals image->rect)]

      (testing "The source image is packed once"
        (is (= 1 (count (g/node-value atlas :layout-rects))))
        (is (= 1 (count (into #{} (map (juxt :x :y :width :height :page)) rects)))))

      (testing "But each entry keeps its own geometry"
        (is (= 2 (count image->rect)))
        (is (= #{0.0 0.5} (into #{} (map (comp double :pivot-x)) (coll/keys image->rect))))
        (is (= #{:sprite-trim-mode-off :sprite-trim-mode-6} (into #{} (map :sprite-trim-mode) (coll/keys image->rect))))
        (is (= #{-0.5 0.0} (into #{} (map (comp double :pivot-x :geometry)) rects)))
        (is (= #{:sprite-trim-mode-off :sprite-trim-mode-6} (into #{} (map (comp :trim-mode :geometry)) rects))))

      (testing "Image hashes include each geometry variant before the animation frames"
        (is (= (mapv murmur/hash64 ["diamond" "diamond" "diamond/diamond" "variant/diamond"])
               (:image-name-hashes (g/node-value atlas :texture-set)))))

      (testing "And the scene of each image node renders its own geometry"
        (let [image-nodes (into []
                                (comp (map :node-id)
                                      (filter #(g/node-instance? atlas/AtlasImage %)))
                                (tree-seq :children :children (g/node-value atlas :node-outline)))
              scene-trim-modes (into #{}
                                     (map (fn [image-node]
                                            (-> (g/node-value image-node :scene) :renderable :user-data :rect :geometry :trim-mode)))
                                     image-nodes)]
          (is (= 2 (count image-nodes)))
          (is (= #{:sprite-trim-mode-off :sprite-trim-mode-6} scene-trim-modes)))))))

(deftest sprite-trim-mode-image-io-error
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
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

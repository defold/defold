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

(ns integration.extension-texturepacker-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.bob.util TextureUtil]
           [com.dynamo.gamesys.proto TextureSetProto$TextureSet]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/texturepacker_project")

(defn- outline-info [{:keys [children label read-only]}]
  (cond-> {:label label}
          read-only (assoc :read-only true)
          (not-empty children) (assoc :children (mapv outline-info children))))

(defn- node-outline-info [node-id]
  (outline-info (g/valid-node-value node-id :node-outline)))

(deftest registered-resource-types-test
  (test-util/with-loaded-project project-path
    (is (= #{} (test-util/protobuf-resource-exts-that-read-defaults workspace)))))

(deftest dirty-save-data-test
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)
    (is (= #{} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/examples/basic/basic.tpatlas")
    (is (= #{"/examples/basic/basic.tpatlas"} (test-util/dirty-proj-paths project)))))

(deftest tpinfo-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/examples/basic/basic.tpinfo")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Texture Packer Export File"
                :read-only true
                :children [{:label "basic-0.png (256 x 256)"
                            :read-only true
                            :children [{:label "box_fill_128" :read-only true}
                                       {:label "box_small_128" :read-only true}
                                       {:label "anim/test-0" :read-only true}
                                       {:label "box_fill_64" :read-only true}
                                       {:label "anim/test-1" :read-only true}
                                       {:label "circle_fill_64" :read-only true}
                                       {:label "anim/test-2" :read-only true}
                                       {:label "triangle_fill_64" :read-only true}]}
                           {:label "basic-1.png (256 x 256)"
                            :read-only true
                            :children [{:label "circle_fill_128" :read-only true}]}
                           {:label "basic-2.png (256 x 256)"
                            :read-only true
                            :children [{:label "shape_L_128" :read-only true}]}
                           {:label "basic-3.png (256 x 256)"
                            :read-only true
                            :children [{:label "triangle_fill_128" :read-only true}]}]}
               (node-outline-info node-id))))

      (testing "scene"
        (is (= "Texture Packer Export File: 4 pages, 256 x 256"
               (:info-text (g/valid-node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:description "Exported using TexturePacker"
                :version "1.0"
                :pages [{:name "basic-0.png"
                         :size {:height 256.0
                                :width 256.0}
                         :sprites [{:name "box_fill_128"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 128.0
                                                 :width 128.0
                                                 :x 1.0
                                                 :y 1.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid true
                                    :rotated false
                                    :source-rect {:height 128.0
                                                  :width 128.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 128.0
                                                     :width 128.0}
                                    :vertices [{:x 128.0 :y 128.0}
                                               {:x 0.0 :y 128.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 128.0 :y 0.0}]}
                                   {:name "box_small_128"
                                    :corner-offset {:x 79.0 :y 57.0}
                                    :frame-rect {:height 65.0
                                                 :width 42.0
                                                 :x 1.0
                                                 :y 130.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 65.0
                                                  :width 42.0
                                                  :x 79.0
                                                  :y 57.0}
                                    :trimmed true
                                    :untrimmed-size {:height 128.0
                                                     :width 128.0}
                                    :vertices [{:x 121.0 :y 122.0}
                                               {:x 79.0 :y 122.0}
                                               {:x 79.0 :y 57.0}
                                               {:x 121.0 :y 57.0}]}
                                   {:name "anim/test-0"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 44.0
                                                 :y 130.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid true
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 64.0 :y 0.0}]}
                                   {:name "box_fill_64"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 44.0
                                                 :y 130.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid true
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 64.0 :y 0.0}]}
                                   {:name "anim/test-1"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 109.0
                                                 :y 130.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 64.0 :y 0.0}]}
                                   {:name "circle_fill_64"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 109.0
                                                 :y 130.0}
                                    :indices [1 2 3 0 1 3]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 64.0 :y 0.0}]}
                                   {:name "anim/test-2"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 174.0
                                                 :y 1.0}
                                    :indices [4 5 6 0 4 6 2 0 1 2 3 0 0 3 4]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 58.0}
                                               {:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 57.0}
                                               {:x 28.0 :y 1.0}
                                               {:x 29.0 :y 0.0}
                                               {:x 35.0 :y 0.0}]}
                                   {:name "triangle_fill_64"
                                    :corner-offset {:x 0.0
                                                    :y 0.0}
                                    :frame-rect {:height 64.0
                                                 :width 64.0
                                                 :x 174.0
                                                 :y 1.0}
                                    :indices [4 5 6 0 4 6 2 0 1 2 3 0 0 3 4]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 64.0
                                                  :width 64.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 64.0
                                                     :width 64.0}
                                    :vertices [{:x 64.0 :y 58.0}
                                               {:x 64.0 :y 64.0}
                                               {:x 0.0 :y 64.0}
                                               {:x 0.0 :y 57.0}
                                               {:x 28.0 :y 1.0}
                                               {:x 29.0 :y 0.0}
                                               {:x 35.0 :y 0.0}]}]}
                        {:name "basic-1.png"
                         :size {:height 256.0
                                :width 256.0}
                         :sprites [{:name "circle_fill_128"
                                    :corner-offset {:x 0.0
                                                    :y 0.0}
                                    :frame-rect {:height 128.0
                                                 :width 128.0
                                                 :x 1.0
                                                 :y 1.0}
                                    :indices [2 6 7 4 5 6 2 4 6 2 3 4 0 1 2 0 2 7]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 128.0
                                                  :width 128.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 128.0
                                                     :width 128.0}
                                    :vertices [{:x 128.0 :y 19.0}
                                               {:x 128.0 :y 110.0}
                                               {:x 83.0 :y 128.0}
                                               {:x 34.0 :y 128.0}
                                               {:x 0.0 :y 94.0}
                                               {:x 0.0 :y 24.0}
                                               {:x 37.0 :y 0.0}
                                               {:x 85.0 :y 0.0}]}]}
                        {:name "basic-2.png"
                         :size {:height 256.0
                                :width 256.0}
                         :sprites [{:name "shape_L_128"
                                    :corner-offset {:x 0.0
                                                    :y 0.0}
                                    :frame-rect {:height 128.0
                                                 :width 128.0
                                                 :x 1.0
                                                 :y 1.0}
                                    :indices [0 4 5 0 1 2 0 2 3 0 3 4]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 128.0
                                                  :width 128.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 128.0
                                                     :width 128.0}
                                    :vertices [{:x 17.0 :y 114.0}
                                               {:x 128.0 :y 114.0}
                                               {:x 128.0 :y 128.0}
                                               {:x 0.0 :y 128.0}
                                               {:x 0.0 :y 0.0}
                                               {:x 15.0 :y 0.0}]}]}
                        {:name "basic-3.png"
                         :size {:height 256.0
                                :width 256.0}
                         :sprites [{:name "triangle_fill_128"
                                    :corner-offset {:x 0.0 :y 0.0}
                                    :frame-rect {:height 128.0
                                                 :width 128.0
                                                 :x 1.0
                                                 :y 1.0}
                                    :indices [3 4 5 2 0 1 2 3 0 0 3 5]
                                    :is-solid false
                                    :rotated false
                                    :source-rect {:height 128.0
                                                  :width 128.0
                                                  :x 0.0
                                                  :y 0.0}
                                    :trimmed false
                                    :untrimmed-size {:height 128.0
                                                     :width 128.0}
                                    :vertices [{:x 128.0 :y 122.0}
                                               {:x 128.0 :y 128.0}
                                               {:x 0.0 :y 128.0}
                                               {:x 0.0 :y 120.0}
                                               {:x 60.0 :y 0.0}
                                               {:x 67.0 :y 0.0}]}]}]}
               (g/node-value node-id :save-value)))))))

(deftest tpatlas-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/examples/basic/basic.tpatlas")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Texture Packer Atlas",
                :children [{:label "box_fill_128", :read-only true}
                           {:label "box_small_128", :read-only true}
                           {:label "anim/test-0", :read-only true}
                           {:label "box_fill_64", :read-only true}
                           {:label "anim/test-1", :read-only true}
                           {:label "circle_fill_64", :read-only true}
                           {:label "anim/test-2", :read-only true}
                           {:label "triangle_fill_64", :read-only true}
                           {:label "circle_fill_128", :read-only true}
                           {:label "shape_L_128", :read-only true}
                           {:label "triangle_fill_128", :read-only true}
                           {:label "BoxFlip",
                            :children [{:label "box_fill_128"}
                                       {:label "box_fill_64"}]}]}
               (node-outline-info node-id))))

      (testing "scene"
        (is (= "Texture Packer Atlas: 4 pages, 256 x 256 (Default profile)"
               (:info-text (g/valid-node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:file "/examples/basic/basic.tpinfo"
                :animations [{:id "BoxFlip"
                              :fps 4
                              :playback :playback-loop-forward
                              :images ["box_fill_128"
                                       "box_fill_64"]}]}
               (g/node-value node-id :save-value)))))))

(deftest tpatlas-build-test
  (test-util/with-scratch-project project-path
    (let [node-id (test-util/resource-node project "/examples/basic/basic.tpatlas")]
      (with-open [_ (test-util/build! node-id)]
        (let [built-textureset (->> node-id test-util/node-build-output (protobuf/bytes->map-without-defaults TextureSetProto$TextureSet))
              built-texture (->> built-textureset :texture (workspace/build-path workspace) fs/read-bytes TextureUtil/textureResourceBytesToTextureImage (protobuf/pb->map-with-defaults))]
          (is (= :type-2d-array (:type built-texture)))
          (is (= 4 (:count built-texture)))
          (is (= 4 (:page-count built-textureset)))
          (is (= ["box_fill_128"
                  "box_small_128"
                  "anim/test-0"
                  "box_fill_64"
                  "anim/test-1"
                  "circle_fill_64"
                  "anim/test-2"
                  "triangle_fill_64"
                  "circle_fill_128"
                  "shape_L_128"
                  "triangle_fill_128"
                  "BoxFlip"]
                 (mapv :id (:animations built-textureset)))))))))

(deftest collection-usage-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

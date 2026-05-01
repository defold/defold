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

(ns integration.sprite-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(deftest replacing-sprite-image-replaces-dep-build-targets
  (test-util/with-loaded-project
    (let [sprite-id (project/get-resource-node project "/logic/session/pow.sprite")
          old-texture-binding-info (first (g/node-value sprite-id :texture-binding-infos))
          old-texture-binding-node-id (:_node-id old-texture-binding-info)
          old-image (:texture old-texture-binding-info)
          old-default-animation (g/node-value sprite-id :default-animation)
           _ (g/set-property! sprite-id :default-animation "test")
          old-build-resources (mapv :resource (test-util/resolve-build-dependencies sprite-id project))]
      (g/transact
        [(g/set-property old-texture-binding-node-id :texture (workspace/find-resource workspace "/switcher/switcher.atlas"))
         (g/set-property sprite-id :default-animation "blue_candy")])
      (is (= (count old-build-resources) (count (mapv :resource (test-util/resolve-build-dependencies sprite-id project)))))
      (is (not= (set old-build-resources) (set (mapv :resource (test-util/resolve-build-dependencies sprite-id project)))))
      (g/transact
        [(g/set-property old-texture-binding-node-id :texture old-image)
         (g/set-property sprite-id :default-animation "test")])
      (is (= (count old-build-resources) (count (mapv :resource (test-util/resolve-build-dependencies sprite-id project)))))
      (is (= (set old-build-resources) (set (mapv :resource (test-util/resolve-build-dependencies sprite-id project)))))
      (g/set-property! sprite-id :default-animation old-default-animation)
      nil)))

(deftest sprite-validation
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/sprite/atlas.sprite")]
      (testing "unknown atlas"
        (test-util/with-prop [node-id :__sampler__texture_sampler__0 (workspace/resolve-workspace-resource workspace "/graphics/unknown_atlas.atlas")]
          (is (g/error? (test-util/prop-error node-id :__sampler__texture_sampler__0)))))
      (testing "invalid atlas"
        (test-util/with-prop [node-id :__sampler__texture_sampler__0 (workspace/resolve-workspace-resource workspace "/graphics/img_not_found.atlas")]
          (is (g/error? (test-util/prop-error node-id :__sampler__texture_sampler__0))))))))

(deftest sprite-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/sprite/atlas.sprite")]
      (test-util/test-uses-assigned-material workspace project node-id
                                             :material
                                             [:renderable :user-data :shader]
                                             [:renderable :user-data :scene-infos 0 :gpu-texture]))))

(deftest multi-textures
  (test-util/with-loaded-project
    (let [sprite (project/get-resource-node project "/sprite/multi_texture.sprite")]
      (test-util/with-prop [sprite :material (workspace/find-resource workspace "/sprite/multi_texture.material")]
        (test-util/with-prop [sprite :__sampler__texture_diffuse__0 (workspace/find-resource workspace "/switcher/switcher.atlas")]
          (test-util/with-prop [sprite :__sampler__texture_normal__0 (workspace/find-resource workspace "/switcher/switcher.atlas")]
            (test-util/with-prop [sprite :default-animation "bomb"]
              (is (not (g/error? (g/node-value sprite :build-targets))))
              (is (not (g/error? (g/node-value sprite :scene)))))))))))
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

(ns integration.model-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.types :as types]
            [editor.properties :as properties]
            [util.murmur :as murmur])
  (:import [javax.vecmath Point3d]))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")
          aabb (g/node-value node-id :aabb)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)]
      (is (< 10 (.distance max min))))))

(deftest textures
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")]
      (let [original-texture (first (test-util/prop node-id :textures))
            t [original-texture nil nil]]
        (test-util/prop! node-id :textures t)
        (is (= t (test-util/prop node-id :textures)))
        (let [p (-> [(g/node-value node-id :_properties)]
                  (properties/coalesce)
                  :properties
                  :texture2)]
          (properties/set-values! p [original-texture]))))))

(deftest animations
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")]
      (testing "can assign single dae file as animations"
        (let [dae-resource (workspace/resolve-workspace-resource workspace "/mesh/treasure_chest.dae")]
          (test-util/with-prop [node-id :animations dae-resource]
            (is (= #{(murmur/hash64 "treasure_chest")} (set (map :id (:animations (g/node-value node-id :animation-set)))))))))
      (testing "can assign animation set as animations"
        (let [animation-set-resource (workspace/resolve-workspace-resource workspace "/model/treasure_chest.animationset")]
          (test-util/with-prop [node-id :animations animation-set-resource]
            (is (= #{(murmur/hash64 "treasure_chest")
                     (murmur/hash64 "treasure_chest_sub_animation/treasure_chest_anim_out")
                     (murmur/hash64 "treasure_chest_sub_sub_animation/treasure_chest_anim_out")}
                   (set (map :id (:animations (g/node-value node-id :animation-set))))))))))))

(deftest model-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")]
      
      (testing "mesh is required"
        (is (nil? (test-util/prop-error node-id :mesh)))
        (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.dae")]]
          (test-util/with-prop [node-id :mesh v]
            (is (g/error? (test-util/prop-error node-id :mesh))))))

      (testing "material is required"
        (is (nil? (test-util/prop-error node-id :material)))
        (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
          (test-util/with-prop [node-id :material v]
            (is (g/error? (test-util/prop-error node-id :material))))))

      (testing "default-animation should be empty string or a valid animation"
        (is (nil? (test-util/prop-error node-id :animations)))
        (is (nil? (test-util/prop-error node-id :default-animation)))
        (test-util/with-prop [node-id :animations (workspace/resolve-workspace-resource workspace "/model/treasure_chest.animationset")]
          (test-util/with-prop [node-id :default-animation ""]
            (is (not (g/error? (test-util/prop-error node-id :default-animation)))))
          (test-util/with-prop [node-id :default-animation "treasure_chest"]
            (is (not (g/error? (test-util/prop-error node-id :default-animation)))))
          (test-util/with-prop [node-id :default-animation "gurka"]
            (is (g/error? (test-util/prop-error node-id :default-animation)))))))))

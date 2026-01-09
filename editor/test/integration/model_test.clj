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

(ns integration.model-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.murmur :as murmur])
  (:import [com.dynamo.gamesys.proto ModelProto$ModelDesc]
           [javax.vecmath Point3d]))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")
          scene (g/node-value node-id :scene)
          aabb (:aabb scene)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)]
      (is (< 10 (.distance max min))))))

(deftest textures
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/test.model")
          original-materials (test-util/prop node-id :materials)
          original-texture (get-in original-materials [0 :textures 0 :texture])
          texture-binding-id (get-in (g/node-value node-id :material-binding-infos) [0 :texture-binding-infos 0 :_node-id])]
      (test-util/prop! texture-binding-id :texture original-texture)
      (is (= original-materials (test-util/prop node-id :materials)))
      (let [p (-> (properties/coalesce [(g/node-value node-id :_properties)])
                  :properties
                  :__sampler__0__0)]
        (properties/set-values! p [original-texture]))))
  (testing "Loading textures renames them to match the material"
    (test-util/with-loaded-project "test/resources/model_migration_project"
      ;; test2.model has textures that don't match the names of the material, so
      ;; loading the model renames the texture bindings
      (let [node-id (test-util/resource-node project "/test.model")]
        (is (g/node-value node-id :dirty))
        (is (= #{"tex_foo" "tex_bar" "tex_baz"}
               (->> (g/node-value node-id :resource)
                    (protobuf/read-map-without-defaults ModelProto$ModelDesc)
                    :materials
                    (into #{}
                          (comp
                            (mapcat :textures)
                            (map :sampler))))))
        (is (= #{"tex0" "tex1" "tex2"}
               (into #{}
                     (comp
                       (mapcat :textures)
                       (map :sampler))
                     (g/node-value node-id :materials))))))))

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
            (is (g/error? (test-util/prop-error node-id :mesh)))
            (is (g/error-value? (g/node-value node-id :build-targets))))))

      (testing "material must be assigned and exist"
        (is (not (g/error-value? (g/node-value node-id :build-targets))))
        (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
          (test-util/with-prop [node-id :__material__0 v]
            (is (g/error? (test-util/prop-error node-id :__material__0)))
            (is (g/error-value? (g/node-value node-id :build-targets))))))

      (testing "default-animation should be empty string or a valid animation"
        (is (nil? (test-util/prop-error node-id :animations)))
        (is (nil? (test-util/prop-error node-id :default-animation)))
        (test-util/with-prop [node-id :animations (workspace/resolve-workspace-resource workspace "/model/treasure_chest.animationset")]
          (test-util/with-prop [node-id :default-animation ""]
            (is (not (g/error? (test-util/prop-error node-id :default-animation)))))
          (test-util/with-prop [node-id :default-animation "treasure_chest"]
            (is (not (g/error? (test-util/prop-error node-id :default-animation)))))
          (test-util/with-prop [node-id :default-animation "gurka"]
            (is (g/error? (test-util/prop-error node-id :default-animation)))
            (is (g/error-value? (g/node-value node-id :build-targets)))))))))

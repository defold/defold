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

(ns integration.extension-rive-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/rive_project")

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
    (test-util/edit-proj-path! project "/bones/marty.rivescene")
    (is (= #{"/bones/marty.rivescene"} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/bones/marty.rivemodel")
    (is (= #{"/bones/marty.rivescene" "/bones/marty.rivemodel"} (test-util/dirty-proj-paths project)))))

(deftest rivescene-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/bones/marty.rivescene")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Rive Scene"
                :children [{:label "Belly"
                            :read-only true
                            :children [{:label "Chest"
                                        :read-only true
                                        :children [{:label "Neck"
                                                    :read-only true
                                                    :children [{:label "Head"
                                                                :read-only true
                                                                :children [{:label "Hair"
                                                                            :read-only true}]}]}
                                                   {:label "Arm_right"
                                                    :read-only true
                                                    :children [{:label "Forearm_right"
                                                                :read-only true
                                                                :children [{:label "Hand_right"
                                                                            :read-only true}]}]}
                                                   {:label "Arm_left"
                                                    :read-only true
                                                    :children [{:label "Forearm_left"
                                                                :read-only true
                                                                :children [{:label "Hand_left"
                                                                            :read-only true}]}]}]}]}
                           {:label "Chest"
                            :read-only true
                            :children [{:label "Neck"
                                        :read-only true
                                        :children [{:label "Head"
                                                    :read-only true
                                                    :children [{:label "Hair"
                                                                :read-only true}]}]}
                                       {:label "Arm_right"
                                        :read-only true
                                        :children [{:label "Forearm_right"
                                                    :read-only true
                                                    :children [{:label "Hand_right"
                                                                :read-only true}]}]}
                                       {:label "Arm_left"
                                        :read-only true
                                        :children [{:label "Forearm_left"
                                                    :read-only true
                                                    :children [{:label "Hand_left"
                                                                :read-only true}]}]}]}
                           {:label "Neck"
                            :read-only true
                            :children [{:label "Head"
                                        :read-only true
                                        :children [{:label "Hair"
                                                    :read-only true}]}]}
                           {:label "Head"
                            :read-only true
                            :children [{:label "Hair"
                                        :read-only true}]}
                           {:label "Hair"
                            :read-only true}
                           {:label "Arm_right"
                            :read-only true
                            :children [{:label "Forearm_right"
                                        :read-only true
                                        :children [{:label "Hand_right"
                                                    :read-only true}]}]}
                           {:label "Forearm_right"
                            :read-only true
                            :children [{:label "Hand_right"
                                        :read-only true}]}
                           {:label "Hand_right"
                            :read-only true}
                           {:label "Arm_left"
                            :read-only true
                            :children [{:label "Forearm_left"
                                        :read-only true
                                        :children [{:label "Hand_left"
                                                    :read-only true}]}]}
                           {:label "Forearm_left"
                            :read-only true
                            :children [{:label "Hand_left"
                                        :read-only true}]}
                           {:label "Hand_left"
                            :read-only true}
                           {:label "Pocket_rignt"
                            :read-only true}
                           {:label "Pocket_left"
                            :read-only true}]}
               (node-outline-info node-id))))

      (testing "scene"
        (is (not (g/error? (g/node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:atlas "/defold-rive/assets/empty.atlas"
                :scene "/bones/marty.riv"}
               (g/node-value node-id :save-value)))))))

(deftest rivemodel-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/bones/marty.rivemodel")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Rive Model"}
               (node-outline-info node-id))))

      (testing "scene"
        (is (not (g/error? (g/node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:artboard "New Artboard"
                :blit-material "/defold-rive/assets/shader-library/rivemodel_blit.material"
                :default-animation "Animation1"
                :material "/defold-rive/assets/rivemodel.material"
                :scene "/bones/marty.rivescene"}
               (g/node-value node-id :save-value)))))))

(deftest collection-usage-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

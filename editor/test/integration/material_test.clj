;; Copyright 2020 The Defold Foundation
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

(ns integration.material-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")
          samplers (g/node-value node-id :samplers)]
      (is (some? (g/node-value node-id :shader)))
      (is (= 1 (count samplers))))))

(deftest material-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")]
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.vp")]]
        (test-util/with-prop [node-id :vertex-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets)))))
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.fp")]]
        (test-util/with-prop [node-id :fragment-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets))))))))

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

(ns integration.factory-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(deftest new-factory-object
  (testing "A new factory object"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/factory/new.factory")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= (localization/message "resource.type.factory") (:label outline)))
        (is (empty? (:children outline)))
        (is (= nil (get-in form-data [:values [:prototype]])))))))

(deftest factory-object-with-prototype
  (testing "A factory object with a prototype set"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/factory/with_prototype.factory")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= (localization/message "resource.type.factory") (:label outline)))
        (is (empty? (:children outline)))
        (is (= "/game_object/test.go"
               (resource/resource->proj-path (get-in form-data [:values [:prototype]]))))))))

(deftest validation
  (test-util/with-loaded-project
    (doseq [[path bad-prototype-path] [["/factory/with_prototype.factory" "/not_exists.go"]
                                       ["/factory/with_prototype.collectionfactory" "/not_exists.collection"]]]
      (let [node-id (test-util/resource-node project path)]
        (is (nil? (test-util/prop-error node-id :prototype)))
        (test-util/with-prop [node-id :prototype nil]
          (is (g/error-info? (test-util/prop-error node-id :prototype))))
        (test-util/with-prop [node-id :prototype (workspace/resolve-workspace-resource workspace bad-prototype-path)]
          (is (g/error-fatal? (test-util/prop-error node-id :prototype))))))))

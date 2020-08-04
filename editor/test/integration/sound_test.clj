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

(ns integration.sound-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.sound :as sound]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest new-sound-object
  (testing "A new sound object"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/sounds/new.sound")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Sound" (:label outline)))
        (is (empty? (:children outline)))
        (is (= nil (get-in form-data [:values [:sound]])))))))

(deftest sound-object-with-sound
  (testing "A sound object with a sound set"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/sounds/tink.sound")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Sound" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "/sounds/tink.wav"
               (resource/resource->proj-path (get-in form-data [:values [:sound]]))))))))

(deftest sound-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/sounds/tink.sound")]
      (is (nil? (test-util/prop-error node-id :sound)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.ogg")]]
        (test-util/with-prop [node-id :sound v]
          (is (g/error? (test-util/prop-error node-id :sound)))))
      (is (nil? (test-util/prop-error node-id :gain)))
      (doseq [v [-0.5 1.5]]
        (test-util/with-prop [node-id :gain v]
          (is (g/error? (test-util/prop-error node-id :gain))))))))

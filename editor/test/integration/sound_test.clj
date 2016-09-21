(ns integration.sound-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.sound :as sound]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest new-sound-object
  (testing "A new sound object"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/sounds/new.sound")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Sound" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "" (get-in form-data [:values [:sound]])))))))

(deftest sound-object-with-sound
  (testing "A sound object with a sound set"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/sounds/tink.sound")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Sound" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "/sounds/tink.wav" (get-in form-data [:values [:sound]])))))))

(deftest sound-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/sounds/tink.sound")]
      (is (nil? (test-util/prop-error node-id :sound)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.ogg")]]
        (test-util/with-prop [node-id :sound v]
          (is (g/error? (test-util/prop-error node-id :sound)))))
      (is (nil? (test-util/prop-error node-id :gain)))
      (doseq [v [-0.5 1.5]]
        (test-util/with-prop [node-id :gain v]
          (is (g/error? (test-util/prop-error node-id :gain))))))))

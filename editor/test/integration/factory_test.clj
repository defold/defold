(ns integration.factory-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.factory :as factory]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest new-factory-object
  (testing "A new factory object"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/factory/new.factory")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Factory" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "" (get-in form-data [:values [:prototype]])))))))

(deftest factory-object-with-prototype
  (testing "A factory object with a prototype set"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            node-id   (test-util/resource-node project "/factory/with_prototype.factory")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "Factory" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "/game_object/test.go" (get-in form-data [:values [:prototype]])))))))

(deftest validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)]
      (doseq [[path bad-prototype-path] [["/factory/with_prototype.factory" "/not_exists.go"]
                                         ["/factory/with_prototype.collectionfactory" "/not_exists.collection"]]]
        (let [node-id (test-util/resource-node project path)]
          (is (nil? (test-util/prop-error node-id :prototype)))
          (test-util/with-prop [node-id :prototype nil]
            (is (g/error-info? (test-util/prop-error node-id :prototype))))
          (test-util/with-prop [node-id :prototype (workspace/resolve-workspace-resource workspace bad-prototype-path)]
            (is (g/error-fatal? (test-util/prop-error node-id :prototype)))))))))

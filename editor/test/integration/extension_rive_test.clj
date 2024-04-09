(ns integration.extension-rive-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(deftest collection-usage-test
  (test-util/with-loaded-project "test/resources/rive_project"
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

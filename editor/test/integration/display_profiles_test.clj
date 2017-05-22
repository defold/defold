(ns integration.display-profiles-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [integration.test-util :as test-util]))

(deftest display-profiles
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/builtins/render/default.display_profiles")
          form-data (g/node-value node-id :form-data)]
      (is (= ["Landscape" "Portrait"] (map :name (get-in form-data [:values [:profiles]])))))))

(deftest display-profiles-from-game-project
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/game.project")
          profiles-data (g/node-value node-id :display-profiles-data)]
      (is (= ["Landscape" "Portrait"] (map :name profiles-data))))))

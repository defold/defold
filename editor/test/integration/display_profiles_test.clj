(ns integration.display-profiles-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(deftest display-profiles
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/builtins/render/default.display_profiles")
          form-data (g/node-value node-id :form-data)]
      (is (= ["Landscape" "Portrait"] (map :name (get-in form-data [:values [:profiles]])))))))

(deftest display-profiles-from-game-project
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/game.project")
          profiles-data (g/node-value node-id :display-profiles-data)]
      (is (= ["Landscape" "Portrait"] (map :name profiles-data))))))

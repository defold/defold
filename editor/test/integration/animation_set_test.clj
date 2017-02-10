(ns integration.animation-set-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest animation-set-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/model/treasure_chest.animationset")
          {:keys [animations bone-list]} (g/node-value node-id :animation-set)]
      (is (= 3 (count bone-list)))
      (is (= 2 (count animations)))
      (is (= #{"treasure_chest" "treasure_chest_sub_animation/treasure_chest_anim_out"} (set (map :id animations)))))))

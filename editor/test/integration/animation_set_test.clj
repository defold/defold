(ns integration.animation-set-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(deftest animation-set-test
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/treasure_chest.animationset")
          {:keys [animations bone-list]} (g/node-value node-id :animation-set)]
      (is (= 3 (count bone-list)))
      (is (= 3 (count animations)))
      (is (= #{"treasure_chest"
               "treasure_chest_sub_animation/treasure_chest_anim_out"
               "treasure_chest_sub_sub_animation/treasure_chest_anim_out"}
             (set (map :id animations)))))))

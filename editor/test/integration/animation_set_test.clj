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

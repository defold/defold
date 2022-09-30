;; Copyright 2020-2022 The Defold Foundation
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

(ns integration.animation-set-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [util.murmur :as murmur]))

(defn- remove-empty-channels [track]
  (into {} (filter (comp seq val)) track))

(deftest animation-set-test
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/model/treasure_chest.animationset")
          {:keys [animations bone-list]} (g/node-value node-id :animation-set)]
      (is (= 3 (count bone-list)))
      (is (= 3 (count animations)))
      (is (= #{(murmur/hash64 "treasure_chest")
               (murmur/hash64 "treasure_chest_sub_animation/treasure_chest_anim_out")
               (murmur/hash64 "treasure_chest_sub_sub_animation/treasure_chest_anim_out")}
             (set (map :id animations)))) 
      (let [animation (first animations)
            bone-count (count bone-list)]
        (testing "All bones are animated"
          (is (<= bone-count (count (:tracks animation)))))
        (testing "No events present"
          (is (= 0 (count (:event-tracks animation)))))

        ; Tracks contain keyframes for position, rotation and scale channels.
        ; Several tracks can target the same bone, but there should not be
        ; multiple tracks that target the same channel for a bone.
        (doseq [[bone-index data-by-channel] (->> (:tracks animation)
                                                  (group-by :bone-index)
                                                  (sort-by key)
                                                  (map (fn [[bone-index bone-tracks]]
                                                         [bone-index (->> bone-tracks
                                                                          (map #(dissoc % :bone-index))
                                                                          (map remove-empty-channels)
                                                                          (apply merge-with (constantly ::conflicting-data)))])))]
          (testing "Bone exists in skeleton"
            (is (< bone-index bone-count)))

          (testing "Channels are not animated by multiple tracks"
            (doseq [[channel data] data-by-channel]
              (is (not= ::conflicting-data data)
                  (str "Found multiple tracks targetting " channel " for bone " bone-index))))

          (testing "Channel data matches expected strides"
            (are [stride channel]
                 (= 0 (mod (count (data-by-channel channel)) stride))
              3 :positions
              4 :rotations
              3 :scale))

          (testing "At least two keys per channel"
            (are [stride channel]
                 (<= (* 2 stride) (count (data-by-channel channel)))
              3 :positions
              4 :rotations
              3 :scale)))))))

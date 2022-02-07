;; Copyright 2021 The Defold Foundation
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

(ns integration.atlas-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(deftest valid-fps
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/atlas.atlas")
          anim (:node-id (test-util/outline node-id [2]))]
      (is (nil? (test-util/prop-error anim :fps)))
      (test-util/prop! anim :fps -1)
      (is (g/error? (test-util/prop-error anim :fps))))))

(deftest img-not-found
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/img_not_found.atlas")
          img (:node-id (test-util/outline node-id [0]))]
      (is (g/error? (g/node-value img :animation))))))

(deftest empty-anim
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/empty_anim.atlas")
          ddf-texture-set (g/node-value node-id :texture-set)
          animation-ids-in-ddf (mapv :id (:animations ddf-texture-set))]
      (is (= ["ball_anim"
              "block_anim"
              "pow_anim"]
             animation-ids-in-ddf)))))

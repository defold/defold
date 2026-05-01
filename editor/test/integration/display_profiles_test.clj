;; Copyright 2020-2026 The Defold Foundation
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

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

(ns integration.game-properties-test
  (:require [clojure.test :refer :all]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest game-properties-test
  (test-util/with-loaded-project "test/resources/game_properties_project"
    (let [game-project (test-util/resource-node project "/game.project")]
      (is (true? (test-util/get-setting game-project ["project" "subtitles"])))
      (is (true? (test-util/get-setting game-project ["tv" "show_ads"])))
      (is (= 128 (test-util/get-setting game-project ["spine" "max_count"])))
      (is (thrown-with-msg? Throwable #"Invalid setting path." (test-util/get-setting game-project ["unrelated" "unexpected"]))))))
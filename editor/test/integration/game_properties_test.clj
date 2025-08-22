(ns integration.game-properties-test
  (:require [clojure.test :refer :all]
            [integration.test-util :as test-util]))

(deftest game-properties-test
  (test-util/with-loaded-project "test/resources/game_properties_project"
    (let [game-project (test-util/resource-node project "/game.project")]
      (is (true? (test-util/get-setting game-project ["project" "subtitles"])))
      (is (true? (test-util/get-setting game-project ["tv" "show_ads"])))
      (is (= 128 (test-util/get-setting game-project ["spine" "max_count"])))
      (is (thrown-with-msg? Throwable #"Invalid setting path." (test-util/get-setting game-project ["unrelated" "unexpected"]))))))
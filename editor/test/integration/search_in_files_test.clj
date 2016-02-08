(ns integration.search-in-files-test
  (:require [clojure.test :refer :all]
            [editor.project :as project]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest search-in-files-test
  (testing "Searching in all 'file' resource nodes in the project"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            res       (project/search-in-files project "go" "session")]
        (is (= 11 (count res)))
        (is (every? #(re-find #"session" (:content %)) res))
        (is (every? :line (:matches (first res))))

        (testing "search is case insensitive"
          (is (= 11 (count (project/search-in-files project "go" "seSSiOn")))))

        (is (= 21 (count (project/search-in-files project nil "session"))))
        (is (= 21 (count (project/search-in-files project "" "session"))))
        (is (= 0 (count (project/search-in-files project "lua" "session"))))
        (is (= 11 (count (project/search-in-files project "lua,go" "session"))))
        (is (= 11 (count (project/search-in-files project " lua,  go" "session"))))
        (is (= 11 (count (project/search-in-files project " lua,  GO" "session"))))))))

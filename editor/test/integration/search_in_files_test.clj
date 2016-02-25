(ns integration.search-in-files-test
  (:require [clojure.test :refer :all]
            [editor.defold-project :as project]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest search-in-files-test
  (testing "Searching in all 'file' resource nodes in the project"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            save-data (project/save-data project)
            res       (project/search-in-files save-data "go" "session")]
        (is (= 11 (count res)))
        (is (every? #(re-find #"session" (:content %)) res))
        (is (every? :line (:matches (first res))))

        (testing "search is case insensitive"
          (is (= 11 (count (project/search-in-files save-data "go" "seSSiOn")))))

        (testing "empty search string gives no results"
          (is (zero? (count (project/search-in-files save-data "" "")))))

        (is (= 21 (count (project/search-in-files save-data nil "session"))))
        (is (= 21 (count (project/search-in-files save-data "" "session"))))
        (is (= 0 (count (project/search-in-files save-data "lua" "session"))))
        (is (= 11 (count (project/search-in-files save-data "lua,go" "session"))))
        (is (= 11 (count (project/search-in-files save-data " lua,  go" "session"))))
        (is (= 11 (count (project/search-in-files save-data " lua,  GO" "session"))))))))

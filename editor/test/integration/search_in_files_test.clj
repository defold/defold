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
            search-string "peanutbutterjellytime"
            res       (project/search-in-files project "go" search-string)]
        (is (= 1 (count res)))
        (is (every? #(.contains (:content %) search-string) res))
        (is (every? :line (:matches (first res))))

        (testing "search is case insensitive"
          (is (= 1 (count (project/search-in-files project "go" "peaNUTbutterjellytime")))))

        (testing "file extensions"
          (is (= 1 (count (project/search-in-files project "g" search-string))))
          (is (= 1 (count (project/search-in-files project "go" search-string))))
          (is (= 1 (count (project/search-in-files project ".go" search-string))))
          (is (= 1 (count (project/search-in-files project "*.go" search-string))))
          (is (= 2 (count (project/search-in-files project "go,sCR" search-string))))
          (is (= 2 (count (project/search-in-files project nil search-string))))
          (is (= 2 (count (project/search-in-files project "" search-string))))
          (is (= 0 (count (project/search-in-files project "lua" search-string))))
          (is (= 1 (count (project/search-in-files project "lua,go" search-string))))
          (is (= 1 (count (project/search-in-files project " lua,  go" search-string))))
          (is (= 1 (count (project/search-in-files project " lua,  GO" search-string))))          
          (is (= 1 (count (project/search-in-files project "script" search-string)))))

        (testing "empty search string gives no results"
          (is (zero? (count (project/search-in-files project "" "")))))))))

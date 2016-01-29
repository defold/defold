(ns integration.search-in-files-test
  (:require [clojure
             [string :as str]
             [test :refer :all]]
            [editor
             [project :as project]
             [resource :refer [resource-type]]]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest comple-find-in-files-regex-test
  (is (= "foo" (str (project/compile-find-in-files-regex "foo"))))
  (is (= "foo.*bar" (str (project/compile-find-in-files-regex "foo*bar"))))
  (is (= "foo.*bar" (str (project/compile-find-in-files-regex "foo*bar[]().$^")))))

(deftest search-in-files-test
  (testing "Searching in all 'file' resource nodes in the project"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   res       (project/search-in-files project "go" "session")]
               (is (= 11 (count res)))
               (is (every? #(re-find #"session" (:content %)) res))

               (is (= 21 (count (project/search-in-files project nil "session"))))
               (is (= 21 (count (project/search-in-files project "" "session"))))
               (is (= 0 (count (project/search-in-files project "lua" "session"))))
               (is (= 11 (count (project/search-in-files project "lua,go" "session"))))
               (is (= 11 (count (project/search-in-files project " lua,  go" "session"))))
               (is (= 11 (count (project/search-in-files project " lua,  GO" "session"))))))))

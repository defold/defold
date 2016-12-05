(ns editor.defold-project-search-test
  (:require [clojure.test :refer :all]
            [editor.defold-project-search :as project-search]))

(deftest comple-find-in-files-regex-test
  (is (= "^(.*)(foo)(.*)$" (str (project-search/compile-find-in-files-regex "foo"))))
  (testing "* is handled correctly"
    (is (= "^(.*)(foo.*bar)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar")))))
  (testing "other wildcard chars are stripped"
    (is (= "^(.*)(foo.*bar)(.*)$" (str (project-search/compile-find-in-files-regex "foo*bar[]().$^")))))
  (testing "case insensitive search strings"
    (is (= "^(.*)(fooo)(.*)$" (str (project-search/compile-find-in-files-regex "fOoO"))))))

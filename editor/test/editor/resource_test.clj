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

(ns editor.resource-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.fn :as fn]
            [util.path :as path]))

(set! *warn-on-reflection* true)

(deftest lines->proj-path-patterns-test
  (testing "Returns empty vector for nil."
    (is (= []
           (resource/lines->proj-path-patterns nil))))

  (testing "Patterns without a leading slash are ignored."
    (is (= []
           (resource/lines->proj-path-patterns ["a" "b" "c"]))))

  (testing "Patterns with a leading slash are included."
    (is (= ["/b" "/d"]
           (resource/lines->proj-path-patterns ["a" "/b" "c" "/d"]))))

  (testing "Patterns are distinct."
    (is (= ["/a" "/b"]
           (resource/lines->proj-path-patterns ["/a" "/b" "/b" "/a"]))))

  (testing "Trailing slashes are stripped from patterns."
    (is (= ["/a" "/b" "/c"]
           (resource/lines->proj-path-patterns ["/a" "/b" "/b/" "/a/" "/c/"])))))

(deftest make-proj-path-patterns-pred-test
  (testing "Returns fn/constantly-false when there are no valid patterns."
    (is (identical? fn/constantly-false (resource/make-proj-path-patterns-pred-raw nil)))
    (is (identical? fn/constantly-false (resource/make-proj-path-patterns-pred-raw []))))

  (testing "Returns functioning predicate when there are some valid patterns."
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/b"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/b")))))

  (testing "Matches multiple patterns."
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/a" "/b"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/b")))
      (is (false? (proj-path-patterns-pred "/c")))))

  (testing "Path match rules."
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/A"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/A"))))
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/a"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (false? (proj-path-patterns-pred "/A"))))
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/a"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/a/b")))
      (is (false? (proj-path-patterns-pred "/ab")))
      (is (false? (proj-path-patterns-pred "/aba")))
      (is (false? (proj-path-patterns-pred "/ab/a"))))
    (let [proj-path-patterns-pred (resource/make-proj-path-patterns-pred-raw ["/ab"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (false? (proj-path-patterns-pred "/a/b")))
      (is (true? (proj-path-patterns-pred "/ab")))
      (is (false? (proj-path-patterns-pred "/aba")))
      (is (true? (proj-path-patterns-pred "/ab/a"))))))

(defn- set-file-lines! [file lines]
  (if (nil? lines)
    (fs/delete-file! file)
    (let [content (string/join \newline lines)]
      (test-support/write-until-new-mtime file content))))

(defn- set-game-project-log-dir-pathname! [project-directory pathname]
  (let [game-project-file (io/file project-directory "game.project")
        lines (when (some? pathname)
                ["[project]"
                 (str "log_dir = " pathname)])]
    (set-file-lines! game-project-file lines)))

(defn- set-project-defignore-lines! [project-directory defignore-lines]
  (let [defignore-file (io/file project-directory ".defignore")]
    (set-file-lines! defignore-file defignore-lines)))

(deftest project-defignore-patterns-test
  (test-support/with-cleared-system-properties!
    ["defold.log.dir"]

    (testing "Throws unless called with an existing canonical directory File."
      (let [temp-directory (fs/create-temp-directory!)
            canonical-project-directory (fs/create-directory! (io/file temp-directory "TestProject"))
            non-canonical-directory (io/file temp-directory "testProject")]
        (with-open [_ (test-util/make-directory-deleter temp-directory)]
          (is (any? (resource/project-defignore-patterns canonical-project-directory)))
          (is (thrown? Exception (resource/project-defignore-patterns nil)))
          (is (thrown? Exception (resource/project-defignore-patterns non-canonical-directory)))
          (is (thrown? Exception (resource/project-defignore-patterns (.getCanonicalPath canonical-project-directory))))
          (is (thrown? Exception (resource/project-defignore-patterns (path/of canonical-project-directory)))))))

    (testing "Completely empty project directory."
      (let [project-directory (fs/create-temp-directory!)]
        (with-open [_ (test-util/make-directory-deleter project-directory)]
          (is (= ["/log.txt"
                  "/instance_1_log.txt"
                  "/instance_2_log.txt"
                  "/instance_3_log.txt"
                  "/instance_4_log.txt"]
                 (resource/project-defignore-patterns project-directory)))
          (is (identical? (resource/project-defignore-patterns project-directory)
                          (resource/project-defignore-patterns project-directory))))))

    (testing "Defold project directory."
      (let [project-directory (io/file (test-util/make-temp-project-copy! "test/resources/empty_project"))]
        (with-open [_ (test-util/make-directory-deleter project-directory)]

          (testing "Engine log paths are included by default."
            (is (= ["/log.txt"
                    "/instance_1_log.txt"
                    "/instance_2_log.txt"
                    "/instance_3_log.txt"
                    "/instance_4_log.txt"]
                   (resource/project-defignore-patterns project-directory)))
            (is (identical? (resource/project-defignore-patterns project-directory)
                            (resource/project-defignore-patterns project-directory))))

          (testing "Engine log directory can be overridden using JVM property."
            ;; Note: Relative to the editor process working directory when
            ;; specified as a relative pathname.
            (try

              (testing "Relative pathname inside project directory."
                (let [log-dir-path (path/relativize
                                     (path/absolute ".")
                                     (path/of project-directory "relative_logs"))]
                  (System/setProperty "defold.log.dir" (str log-dir-path)))
                (is (= ["/relative_logs/log.txt"
                        "/relative_logs/instance_1_log.txt"
                        "/relative_logs/instance_2_log.txt"
                        "/relative_logs/instance_3_log.txt"
                        "/relative_logs/instance_4_log.txt"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Relative pathname outside project directory."
                (let [log-dir-path (path/relativize
                                     (path/absolute ".")
                                     (path/of project-directory ".." "relative_logs"))]
                  (System/setProperty "defold.log.dir" (str log-dir-path)))
                (is (= []
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Absolute pathname inside project directory."
                (let [log-dir-path (path/absolute project-directory "absolute_logs")]
                  (System/setProperty "defold.log.dir" (str log-dir-path)))
                (is (= ["/absolute_logs/log.txt"
                        "/absolute_logs/instance_1_log.txt"
                        "/absolute_logs/instance_2_log.txt"
                        "/absolute_logs/instance_3_log.txt"
                        "/absolute_logs/instance_4_log.txt"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Absolute pathname outside project directory."
                (let [log-dir-path (path/absolute project-directory ".." "absolute_logs")]
                  (System/setProperty "defold.log.dir" (str log-dir-path)))
                (is (= []
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (finally
                (System/clearProperty "defold.log.dir"))))

          (testing "Engine log directory can be overridden in game.project."
            ;; Note: Relative to the project directory when specified as a
            ;; relative pathname.
            (try

              (testing "Relative pathname inside project directory."
                (let [log-dir-path (path/of "relative_logs")]
                  (set-game-project-log-dir-pathname! project-directory (str log-dir-path)))
                (is (= ["/relative_logs/log.txt"
                        "/relative_logs/instance_1_log.txt"
                        "/relative_logs/instance_2_log.txt"
                        "/relative_logs/instance_3_log.txt"
                        "/relative_logs/instance_4_log.txt"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Relative pathname outside project directory."
                (let [log-dir-path (path/of ".." "relative_logs")]
                  (set-game-project-log-dir-pathname! project-directory (str log-dir-path)))
                (is (= []
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Absolute pathname inside project directory."
                (let [log-dir-path (path/absolute project-directory "absolute_logs")]
                  (set-game-project-log-dir-pathname! project-directory (str log-dir-path)))
                (is (= ["/absolute_logs/log.txt"
                        "/absolute_logs/instance_1_log.txt"
                        "/absolute_logs/instance_2_log.txt"
                        "/absolute_logs/instance_3_log.txt"
                        "/absolute_logs/instance_4_log.txt"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Absolute pathname outside project directory."
                (let [log-dir-path (path/absolute project-directory ".." "absolute_logs")]
                  (set-game-project-log-dir-pathname! project-directory (str log-dir-path)))
                (is (= []
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (finally
                (set-game-project-log-dir-pathname! project-directory nil))))

          (testing "Includes patterns from .defignore file."
            (try

              (testing "Patterns without a leading slash are ignored."
                (set-project-defignore-lines! project-directory ["a" "b" "c"])
                (is (= ["/log.txt"
                        "/instance_1_log.txt"
                        "/instance_2_log.txt"
                        "/instance_3_log.txt"
                        "/instance_4_log.txt"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Patterns with a leading slash are included."
                (set-project-defignore-lines! project-directory ["a" "/b" "c" "/d"])
                (is (= ["/log.txt"
                        "/instance_1_log.txt"
                        "/instance_2_log.txt"
                        "/instance_3_log.txt"
                        "/instance_4_log.txt"
                        "/b"
                        "/d"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Patterns are distinct."
                (set-project-defignore-lines! project-directory ["/a" "/b" "/b" "/a" "/log.txt" "/instance_1_log.txt"])
                (is (= ["/log.txt"
                        "/instance_1_log.txt"
                        "/instance_2_log.txt"
                        "/instance_3_log.txt"
                        "/instance_4_log.txt"
                        "/a"
                        "/b"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (testing "Trailing slashes are stripped from patterns."
                (set-project-defignore-lines! project-directory ["/a" "/b" "/b/" "/a/" "/c/"])
                (is (= ["/log.txt"
                        "/instance_1_log.txt"
                        "/instance_2_log.txt"
                        "/instance_3_log.txt"
                        "/instance_4_log.txt"
                        "/a"
                        "/b"
                        "/c"]
                       (resource/project-defignore-patterns project-directory)))
                (is (identical? (resource/project-defignore-patterns project-directory)
                                (resource/project-defignore-patterns project-directory))))

              (finally
                (set-project-defignore-lines! project-directory nil)))))))))

(deftest defignore-pred-test
  (test-support/with-cleared-system-properties!
    ["defold.log.dir"]

    (let [first-project-directory (io/file (test-util/make-temp-project-copy! "test/resources/empty_project"))
          second-project-directory (io/file (test-util/make-temp-project-copy! "test/resources/empty_project"))]
      (with-open [_first-project-deleter (test-util/make-directory-deleter first-project-directory)
                  _second-project-deleter (test-util/make-directory-deleter second-project-directory)]

        (testing "Caches individual predicates per project-directory."
          (is (identical? (resource/defignore-pred first-project-directory)
                          (resource/defignore-pred first-project-directory)))
          (is (identical? (resource/defignore-pred second-project-directory)
                          (resource/defignore-pred second-project-directory)))
          (is (not (identical? (resource/defignore-pred first-project-directory)
                               (resource/defignore-pred second-project-directory)))))

        (testing "Invalidated by adding .defignore file."
          (let [first-defignore-pred (resource/defignore-pred first-project-directory)
                second-defignore-pred (resource/defignore-pred second-project-directory)]
            (set-project-defignore-lines! first-project-directory ["/first.txt"])
            (is (identical? second-defignore-pred (resource/defignore-pred second-project-directory)))
            (is (not (identical? first-defignore-pred (resource/defignore-pred first-project-directory))))
            (is (identical? (resource/defignore-pred first-project-directory)
                            (resource/defignore-pred first-project-directory)))))

        (testing "Invalidated by editing .defignore file."
          (let [first-defignore-pred (resource/defignore-pred first-project-directory)
                second-defignore-pred (resource/defignore-pred second-project-directory)]
            (set-project-defignore-lines! first-project-directory ["/ignored.txt" "/neglected.txt"])
            (is (identical? second-defignore-pred (resource/defignore-pred second-project-directory)))
            (is (not (identical? first-defignore-pred (resource/defignore-pred first-project-directory))))
            (is (identical? (resource/defignore-pred first-project-directory)
                            (resource/defignore-pred first-project-directory)))))

        (testing "Invalidated by deleting .defignore file."
          (let [first-defignore-pred (resource/defignore-pred first-project-directory)
                second-defignore-pred (resource/defignore-pred second-project-directory)]
            (set-project-defignore-lines! first-project-directory nil)
            (is (identical? second-defignore-pred (resource/defignore-pred second-project-directory)))
            (is (not (identical? first-defignore-pred (resource/defignore-pred first-project-directory))))
            (is (identical? (resource/defignore-pred first-project-directory)
                            (resource/defignore-pred first-project-directory)))))

        (testing "Invalidated by setting project.log_dir in game.project."
          (let [first-defignore-pred (resource/defignore-pred first-project-directory)
                second-defignore-pred (resource/defignore-pred second-project-directory)]
            (set-game-project-log-dir-pathname! first-project-directory "logs")
            (is (identical? second-defignore-pred (resource/defignore-pred second-project-directory)))
            (is (not (identical? first-defignore-pred (resource/defignore-pred first-project-directory))))
            (is (identical? (resource/defignore-pred first-project-directory)
                            (resource/defignore-pred first-project-directory)))))

        (testing "Invalidated by setting defold.log.dir property."
          (let [first-defignore-pred (resource/defignore-pred first-project-directory)
                second-defignore-pred (resource/defignore-pred second-project-directory)]
            (System/setProperty "defold.log.dir" "logs")
            (try
              (is (not (identical? first-defignore-pred (resource/defignore-pred first-project-directory))))
              (is (not (identical? second-defignore-pred (resource/defignore-pred second-project-directory))))
              (is (identical? (resource/defignore-pred first-project-directory)
                              (resource/defignore-pred first-project-directory)))
              (is (identical? (resource/defignore-pred second-project-directory)
                              (resource/defignore-pred second-project-directory)))
              (finally
                (System/clearProperty "defold.log.dir")))))))))

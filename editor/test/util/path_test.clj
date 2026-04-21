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

(ns util.path-test
  (:require [clojure.test :refer :all]
            [editor.fs :as fs]
            [integration.test-util :as test-util]
            [util.path :as path])
  (:import [java.nio.file NoSuchFileException]))

(set! *warn-on-reflection* true)

(deftest actual-cased-test
  (test-util/with-temp-dir! dir
    (testing "Throws for a missing file system entry."
      (is (thrown? NoSuchFileException (path/actual-cased dir "MissingFile.txt")))
      (is (thrown? NoSuchFileException (path/actual-cased dir "MissingDirectory" "MissingFile.txt"))))

    (testing "Returns a Path with the actual casing for existing file system entries."
      (test-util/make-file-tree! dir [{"ExistingDirectory" ["ExistingFile.txt"]}])
      (let [expected (path/absolute dir "ExistingDirectory" "ExistingFile.txt")]
        (is (= expected (path/actual-cased (path/of dir "ExistingDirectory" "ExistingFile.txt"))))
        (is (= expected (path/actual-cased dir "ExistingDirectory" "ExistingFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/actual-cased dir "existingDIRECTORY" "existingFILE.txt"))))))

    (testing "Does not follow file symlinks."
      (path/create-symlink! (path/of dir "ExistingDirectory" "SymlinkFile.txt")
                            (path/of dir "ExistingDirectory" "ExistingFile.txt"))
      (let [expected (path/absolute dir "ExistingDirectory" "SymlinkFile.txt")]
        (is (= expected (path/actual-cased dir "ExistingDirectory" "SymlinkFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/actual-cased dir "existingDIRECTORY" "symlinkFILE.txt"))))))

    (testing "Does not follow directory symlinks."
      (path/create-symlink! (path/of dir "SymlinkDirectory")
                            (path/of dir "ExistingDirectory"))
      (let [expected (path/absolute dir "SymlinkDirectory" "ExistingFile.txt")]
        (is (= expected (path/actual-cased dir "SymlinkDirectory" "ExistingFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/actual-cased dir "symlinkDIRECTORY" "existingFILE.txt"))))))))

(deftest real-test
  (test-util/with-temp-dir! dir
    (testing "Throws for a missing file system entry."
      (is (thrown? NoSuchFileException (path/real dir "MissingFile.txt")))
      (is (thrown? NoSuchFileException (path/real dir "MissingDirectory" "MissingFile.txt"))))

    (testing "Returns the real Path for existing file system entries."
      (test-util/make-file-tree! dir [{"ExistingDirectory" ["ExistingFile.txt"]}])
      (let [expected (path/absolute dir "ExistingDirectory" "ExistingFile.txt")]
        (is (= expected (path/real (path/of dir "ExistingDirectory" "ExistingFile.txt"))))
        (is (= expected (path/real dir "ExistingDirectory" "ExistingFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/real dir "existingDIRECTORY" "existingFILE.txt"))))))

    (testing "Follows file symlinks."
      (path/create-symlink! (path/of dir "ExistingDirectory" "SymlinkFile.txt")
                            (path/of dir "ExistingDirectory" "ExistingFile.txt"))
      (let [expected (path/absolute dir "ExistingDirectory" "ExistingFile.txt")]
        (is (= expected (path/real dir "ExistingDirectory" "SymlinkFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/real dir "existingDIRECTORY" "symlinkFILE.txt"))))))

    (testing "Follows directory symlinks."
      (path/create-symlink! (path/of dir "SymlinkDirectory")
                            (path/of dir "ExistingDirectory"))
      (let [expected (path/absolute dir "ExistingDirectory" "ExistingFile.txt")]
        (is (= expected (path/real dir "SymlinkDirectory" "ExistingFile.txt")))
        (when-not fs/is-case-sensitive
          (is (= expected (path/real dir "symlinkDIRECTORY" "existingFILE.txt"))))))))

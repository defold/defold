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

(ns lint.git-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer [deftest is]]
            [lint.git :as git])
  (:import [java.io File]
           [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]))

;; Uses read-only Git commands to check untracked selection and path handling.
(deftest uncommitted-files-test
  (let [directory
        (.toFile (Files/createTempDirectory (.toPath (io/file "."))
                                            "lint-git-test-"
                                            (make-array FileAttribute 0)))

        filenames
        (cond-> ["clean.clj" "with spaces.cljc" "settings.edn"]
          (not= "\\" File/separator) (conj "line\nbreak.cljs"))]
    (try
      (doseq [filename filenames]
        (spit (io/file directory filename) ""))
      (spit (io/file directory "notes.txt") "")
      (spit (io/file directory ".gitignore") "ignored.clj\n")
      (spit (io/file directory "ignored.clj") "")
      (is (= (set filenames) (set (git/uncommitted-files directory))))
      (doseq [filename filenames]
        (io/delete-file (io/file directory filename)))
      (is (= [] (git/uncommitted-files directory)))
      (finally
        (doseq [file (.listFiles directory)]
          (io/delete-file file))
        (io/delete-file directory)))))

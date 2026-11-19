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

(ns comments-test
  (:require [clj-kondo.core :as kondo]
            [clojure.java.io :as io]
            [clojure.test :refer [deftest is]])
  (:import [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]))

;; Keeps commented-out forms exempt while checking long prose comments.
(deftest comment-line-length-test
  (let [directory
        (.toFile (Files/createTempDirectory "comment-lint-test-" (make-array FileAttribute 0)))

        file (io/file directory "comments.clj")
        long-string (str \" (.repeat "x" 90) \")
        long-prose (.repeat "prose " 20)]
    (try
      (doseq [[line expected-count]
              [[(str "    ;" long-string) 0]
               [(str ";; " long-string) 0]
               [(str ";;(disabled-call " long-string ")") 0]
               [(str ";; [dependency " long-string "]") 0]
               [(str ";; {:key " long-string "}") 0]
               [(str ";; #{" long-string "}") 0]
               [(str ";; '(call " long-string ")") 0]
               [(str ";; @" long-string) 0]
               [(str ";; " long-prose) 1]
               [(str "; " long-prose) 1]
               [(str ";; Calling (disabled-call) " long-prose) 1]
               [(str ";; " (.repeat "x" 77)) 0]
               [(str ";; " (.repeat "x" 78)) 1]
               [(str ";; https://example.com/" (.repeat "x" 90)) 0]
               [";; Licensed under the Defold License version 1.0 (the \"License\"); you may not use" 0]
               [long-string 0]]]
        (spit file (str "(ns comments)\n" line "\n"))
        (let [findings
              (:findings (kondo/run! {:lint [(str file)]
                                      :config-dir ".clj-kondo"}))]
          (is (= expected-count (count findings)) line)
          (doseq [finding findings]
            (is (= :line-length (:type finding)) line)
            (is (= [2 81] [(:row finding) (:col finding)]) line))))
      (finally
        (io/delete-file file true)
        (io/delete-file directory)))))

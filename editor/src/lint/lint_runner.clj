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

(ns lint-runner
  (:require [babashka.pods :as pods]))

(defn -main
  "Load the native clj-kondo pod before running lint checks or local tests."
  [kondo & filenames]
  (pods/load-pod kondo)
  (if-not (contains? (set filenames) "--test")
    (apply (requiring-resolve 'lint/-main) filenames)
    (do
      (when-not (= ["--test"] (vec filenames))
        (throw (ex-info "Use --test on its own to run the lint tests." {})))
      (System/exit (int ((requiring-resolve 'lint.test-runner/run-tests) "test-lint"))))))

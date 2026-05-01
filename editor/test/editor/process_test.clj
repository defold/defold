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

(ns editor.process-test
  (:require [clojure.test :refer :all]
            [editor.os :as os]
            [editor.process :as process])
  (:import [java.io ByteArrayOutputStream]))

(deftest start-test
  (when-not (os/is-win32?)
    (let [p (process/start! "ls")
          output (process/capture! (process/out p))
          exit (process/await-exit-code p)]
      (is (zero? exit))
      (is (not-empty output)))))

(deftest exec-test
  (when-not (os/is-win32?)
    (let [output (process/exec! "ls")]
      (is (not-empty output)))))

(deftest env-test
  (when-not (os/is-win32?)
    (let [val "UHH"
          out (process/exec! {:env {"CUSTOM_VAR" val}} "printenv" "CUSTOM_VAR")]
      (is (= val out)))))

(deftest remove-env-test
  (when-not (os/is-win32?)
    (is (not-empty (process/exec! "printenv" "PATH")))
    (is (thrown? Exception (process/exec! {:env {"PATH" nil}} "printenv" "PATH")))))

(deftest redirect-test
  (when-not (os/is-win32?)
    (is (nil? (process/exec! {:err :discard} "bash" "-c" "ls >&2")))
    (is (not-empty (process/exec! {:err :stdout} "bash" "-c" "ls >&2")))))

(deftest pipe-test
  (when-not (os/is-win32?)
    (let [p (process/start! "ls")
          target (ByteArrayOutputStream.)]
      (process/pipe! (process/out p) target)
      (is (not-empty (str target))))))

(deftest on-exit-test
  (when-not (os/is-win32?)
    (let [p (process/start! "ls")
          done (promise)]
      (process/on-exit! p #(deliver done true))
      (is (true? (deref done 100 false))))))
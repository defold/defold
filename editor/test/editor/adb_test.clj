;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.adb-test
  (:require [clojure.test :refer :all]
            [editor.adb :as adb]))

(deftest path-evaluator-test
  (are [expected actual] (= expected (#'adb/evaluate-path actual))
    ;; leading ~ is expanded
    (System/getProperty "user.home") "~"
    (str (System/getProperty "user.home") "/adb") "~/adb"
    ;; non-leading ~ is preserved
    "/usr/bin/~/adb" "/usr/bin/~/adb"
    ;; $ENV vars are expanded
    (System/getenv "USER") "$USER"
    (str "/home/" (System/getenv "USER") "/adb") "/home/$USER/adb"
    ;; absent env vars make the whole output collapse into nil
    nil "/home/$UHHH"))

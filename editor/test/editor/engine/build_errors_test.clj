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

(ns editor.engine.build-errors-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.engine.build-errors :as build-errors]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [dynamo.graph :as g]))

(defn- slurp-test-file
  [name]
  (slurp (str "test/resources/native_extension_error_parsing/" name)))

(defn- make-fake-file-resource [workspace proj-path text opts]
  (let [root-dir (workspace/project-directory workspace)]
    (tu/make-fake-file-resource workspace (.getPath root-dir) (io/file root-dir proj-path) (.getBytes text "UTF-8") opts)))

(defn- make-fake-empty-files
  [workspace files]
  (for [f files]
    (make-fake-file-resource workspace f "" nil)))

(defn- parse-log-test
  [logfile result-file manifest-file project-resources]
  (tu/with-loaded-project "test/resources/empty_project"
    (let [project (tu/setup-project!
                   workspace
                   (make-fake-empty-files workspace project-resources))
          content (slurp-test-file logfile)
          expected-result (read-string (slurp-test-file result-file))
          result (build-errors/parse-compilation-log content project manifest-file)]
      [expected-result result])))

(deftest defappsflyer
  (let [[expected-result result]
        (parse-log-test
         "defappsflyer_iOS.txt"
         "defappsflyer_iOS_parsed.edn"
         "/defappsflyer/ext.manifest"
         ["defappsflyer/src/DefAppsFlyer.cpp"
          "defappsflyer/src/DefAppsFlyer.h"
          "defappsflyer/src/DefAppsFlyerIOS.mm"
          "defappsflyer/lib/ios/AppsFlyerTracker.framework/Headers/AppsFlyerTracker.h"
          "defappsflyer/src/utils/LuaUtils.h"
          "defappsflyer/ext.manifest"])]
    (is (= expected-result result))))

(deftest missing-symbols
  (let [[expected-result result]
        (parse-log-test
         "missingSymbols.txt"
         "missingSymbols_parsed.edn"
         nil
         [])]
    (is (= expected-result result))))

(deftest missing-symbols-2
  (let [[expected-result result]
        (parse-log-test
         "missingSymbols_2.txt"
         "missingSymbols_parsed_2.edn"
         "/win32nativeext/ext.manifest"
         ["win32nativeext/ext.manifest"
          "ext2/src/main.cpp"
          "win32nativeext/src/main.cpp"])]
    (is (= expected-result result))))

(deftest template-barf
  (let [[expected-result result]
        (parse-log-test
         "templateBarf.txt"
         "templateBarf_parsed.edn"
         "/win32nativeext/ext.manifest"
         ["win32nativeext/ext.manifest"
          "win32nativeext/src/main.cpp"])]
    (is (= expected-result result))))

(deftest win32-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogWin32.txt"
         "errorLogWin32_parsed.edn"
         "/androidnative/ext.manifest"
         ["androidnative/ext.manifest"
          "androidnative/src/main.cpp"
          "king_device_id/src/kdid.cpp"])]
    (is (= expected-result result))))

(deftest android-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogAndroid.txt"
         "errorLogAndroid_parsed.edn"
         "/androidnative/ext.manifest"
         ["androidnative/ext.manifest"
          "androidnative/src/main.cpp"])]
    (is (= expected-result result))))

(deftest html5-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogHTML5.txt"
         "errorLogHTML5_parsed.edn"
         nil
         [])]
    (is (= expected-result result))))

(deftest macOS-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogOSX.txt"
         "errorLogOSX_parsed.edn"
         nil
         [])]
    (is (= expected-result result))))

(deftest iOS-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogiOS.txt"
         "errorLogiOS_parsed.edn"
         nil
         [])]
    (is (= expected-result result))))

(deftest linux-errors
  (let [[expected-result result]
        (parse-log-test
         "errorLogLinux.txt"
         "errorLogLinux_parsed.edn"
         "/androidnative/ext.manifest"
         ["androidnative/ext.manifest"
          "androidnative/src/main.cpp"])]
    (is (= expected-result result))))

(deftest manifest-lookup
  (tu/with-loaded-project "test/resources/empty_project"
    (let [cpp-file (make-fake-file-resource workspace "androidnative/main.cpp" "" nil)
          manifest-file (make-fake-file-resource workspace "androidnative/ext.manifest" "" nil)
          files [(make-fake-file-resource workspace "androidnative" "" {:children [cpp-file manifest-file]})]
          project (tu/setup-project! workspace files)]
      (g/with-auto-evaluation-context evaluation-context
        (is (= "/androidnative/ext.manifest" (build-errors/find-ext-manifest-relative-to-resource project "/androidnative/main.cpp" evaluation-context)))))))


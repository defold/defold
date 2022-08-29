;; Copyright 2020-2022 The Defold Foundation
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

(ns integration.engine.native-extensions-test
  (:require
   [clojure.string :as string]
   [clojure.test :refer :all]
   [integration.test-util :as test-util]
   [support.test-support :refer [with-clean-system]]
   [dynamo.graph :as g]
   [editor.app-view :as app-view]
   [editor.defold-project :as project]
   [editor.engine.native-extensions :as native-extensions]
   [editor.fs :as fs]
   [editor.resource :as resource]
   [util.repo :as repo])
  (:import [com.dynamo.bob.archive EngineVersion]))

(defn fix-engine-sha1 [f]
  (let [engine-sha1 (or (repo/detect-engine-sha1) EngineVersion/sha1)]
    (with-redefs [editor.system/defold-engine-sha1 (constantly engine-sha1)]
      (f))))

(use-fixtures :once fix-engine-sha1)

(deftest extension-roots-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
          project (test-util/setup-project! workspace)]
      (g/with-auto-evaluation-context evaluation-context
        (is (= #{"/extension1" "/subdir/extension2"}
               (set (map resource/proj-path (native-extensions/extension-roots project evaluation-context)))))))))

(deftest extension-resource-nodes-test
  (letfn [(platform-resources [project platform]
            (let [resource-nodes (g/with-auto-evaluation-context evaluation-context
                                   (native-extensions/extension-resource-nodes project evaluation-context platform))]
              (->> resource-nodes
                   (map (comp resource/proj-path
                              #(g/node-value % :resource)))
                   set)))]
    (testing "x86_64-macos"
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
              project (test-util/setup-project! workspace)]
          (is (= #{"/extension1/ext.manifest"
                   "/extension1/include/file"
                   "/extension1/include/subdir/file"
                   "/extension1/src/file"
                   "/extension1/src/subdir/file"
                   "/extension1/lib/common/file"
                   "/extension1/lib/x86_64-osx/file"
                   "/extension1/lib/osx/file"
                   "/subdir/extension2/ext.manifest"}
                 (platform-resources project "x86_64-macos"))))))
    (testing "arm64-ios"
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
              project (test-util/setup-project! workspace)]
          (is (= #{"/extension1/ext.manifest"
                   "/extension1/include/file"
                   "/extension1/include/subdir/file"
                   "/extension1/src/file"
                   "/extension1/src/subdir/file"
                   "/extension1/lib/common/file"
                   "/extension1/lib/arm64-ios/file"
                   "/extension1/lib/ios/file"
                   "/subdir/extension2/ext.manifest"}
                 (platform-resources project "arm64-ios"))))))))

(defn- dummy-file [] (fs/create-temp-file! "dummy" ""))

(deftest cached-engine-archive-test
  (let [cache-dir (fs/create-temp-directory! "defold-test")]
    (testing "nothing cached initially"
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a")))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-win32" "a"))))

    (testing "caches platforms separately"
      ;; cache a build for x86_64-osx with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "a" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a"))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-win32" "a")))

      ;; cache a build for x86_64-win32 with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-win32" "a" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-win32" "a"))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a")))

    (testing "verifies key"
      ;; cache a build for x86_64-osx with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "a" (dummy-file)))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "b")))

      ;; cache anew with key b
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "b" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "b")))))

(defn- blocking-async-build! [project prefs]
  (let [result (promise)]
    (test-util/run-event-loop!
      (fn [exit-event-loop!]
        (app-view/async-build! project prefs {} {}
                               (constantly nil)
                               (fn [build-results engine-descriptor build-engine-exception]
                                 (deliver result [build-results engine-descriptor build-engine-exception])
                                 (exit-event-loop!)))))
    (deref result)))

(deftest async-build-on-build-server
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/trivial_extension")
          project (test-util/setup-project! workspace)
          test-prefs (test-util/make-test-prefs)]
      (assert (= (native-extensions/get-build-server-url test-prefs) "https://build-stage.defold.com"))
      (testing "clean project builds on server"
        (let [[{:keys [error artifacts artifact-map etags] :as build-results} engine-descriptor build-engine-exception] (blocking-async-build! project test-prefs)]
          (is (nil? error))
          (is (not-empty artifacts))
          (is (not-empty artifact-map))
          (is (not-empty etags))
          (is (nil? build-engine-exception))
          (is (some? engine-descriptor))
          (is (not (contains? engine-descriptor :cached)))
          (is (and engine-descriptor (.exists (:engine-archive engine-descriptor))))))
      (testing "rebuild uses cached engine archive"
        (let [[_build-results engine-descriptor build-engine-exception] (blocking-async-build! project test-prefs)]
          (is (nil? build-engine-exception))
          (is (some? engine-descriptor))
          (is (contains? engine-descriptor :cached))
          (is (and engine-descriptor (.exists (:engine-archive engine-descriptor))))))
      (testing "bad code breaks build on server"
        (let [script (project/get-resource-node project "/printer/src/main.cpp")]
          (g/update-property! script :modified-lines conj "ought to break")
          (let [[{:keys [error artifacts artifact-map etags] :as build-results} engine-descriptor ^Exception build-engine-exception] (blocking-async-build! project test-prefs)]
            (is (nil? error))
            (is (not-empty artifacts))
            (is (not-empty artifact-map))
            (is (not-empty etags))
            (is (some? build-engine-exception))
            (is (and build-engine-exception (string/includes? (.getMessage build-engine-exception) "ought to break")))
            (is (nil? engine-descriptor))))))))

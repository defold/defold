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
  (:import
   [java.io File]))

(defn fix-engine-sha1 [f]
  (let [engine-sha1 (repo/detect-engine-sha1)]
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
    (testing "x86_64-darwin"
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
                 (platform-resources project "x86_64-darwin"))))))
    (testing "arm64-darwin"
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
                 (platform-resources project "arm64-darwin"))))))))

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

(defn- blocking-async-build! [project evaluation-context prefs]
  (let [result (promise)]
    (test-util/run-event-loop!
      (fn [exit-event-loop!]
        (app-view/async-build! project evaluation-context prefs {} {}
                               (constantly nil)
                               (fn [build-results engine-descriptor build-engine-exception]
                                 (deliver result [build-results engine-descriptor build-engine-exception])
                                 (exit-event-loop!)))))
    (deref result)))

#_(deftest async-build-on-build-server
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/trivial_extension")
          project (test-util/setup-project! workspace)
          test-prefs (test-util/make-test-prefs)]
      (assert (= (native-extensions/get-build-server-url test-prefs) "https://build-stage.defold.com"))
      (testing "clean project builds on server"
        (g/with-auto-evaluation-context evaluation-context
          (let [[{:keys [error artifacts artifact-map etags] :as build-results} engine-descriptor build-engine-exception] (blocking-async-build! project evaluation-context test-prefs)]
            (is (nil? error))
            (is (not-empty artifacts))
            (is (not-empty artifact-map))
            (is (not-empty etags))
            (is (nil? build-engine-exception))
            (is (some? engine-descriptor))
            (is (not (contains? engine-descriptor :cached)))
            (is (and engine-descriptor (.exists (:engine-archive engine-descriptor)))))))
      (testing "rebuild uses cached engine archive"
        (g/with-auto-evaluation-context evaluation-context
          (let [[_build-results engine-descriptor build-engine-exception] (blocking-async-build! project evaluation-context test-prefs)]
            (is (nil? build-engine-exception))
            (is (some? engine-descriptor))
            (is (contains? engine-descriptor :cached))
            (is (and engine-descriptor (.exists (:engine-archive engine-descriptor)))))))
      (testing "bad code breaks build on server"
        (let [script (project/get-resource-node project "/printer/src/main.cpp")]
          (g/update-property! script :modified-lines conj "ought to break")
          (g/with-auto-evaluation-context evaluation-context
            (let [[{:keys [error artifacts artifact-map etags] :as build-results} engine-descriptor ^Exception build-engine-exception] (blocking-async-build! project evaluation-context test-prefs)]
              (is (nil? error))
              (is (not-empty artifacts))
              (is (not-empty artifact-map))
              (is (not-empty etags))
              (is (some? build-engine-exception))
              (is (and build-engine-exception (string/includes? (.getMessage build-engine-exception) "ought to break")))
              (is (nil? engine-descriptor)))))))))

(ns integration.engine.native-extensions-test
  (:require
   [clojure.test :refer :all]
   [integration.test-util :as test-util]
   [support.test-support :refer [with-clean-system]]
   [dynamo.graph :as g]
   [editor.engine.native-extensions :as native-extensions]
   [editor.resource :as resource])
  (:import
   (java.io File)
   (java.nio.file.attribute FileAttribute)
   (java.nio.file Files )))

(deftest extension-roots-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
          project (test-util/setup-project! workspace)]
      (is (= #{"/extension1" "/subdir/extension2"}
             (set (map resource/proj-path (native-extensions/extension-roots project))))))))

(deftest extension-resource-nodes-test
  (letfn [(platform-resources [project platform]
            (let [resource-nodes (native-extensions/extension-resource-nodes
                                   project
                                   (native-extensions/extension-roots project)
                                   platform)]
              (->> resource-nodes
                   (map (comp resource/proj-path
                              #(g/node-value % :resource)))
                   set)))]
    (testing "x86_64-osx"
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
    (testing "x86_64-win32"
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
              project (test-util/setup-project! workspace)]
          (is (= #{"/extension1/ext.manifest"
                   "/extension1/include/file"
                   "/extension1/include/subdir/file"
                   "/extension1/src/file"
                   "/extension1/src/subdir/file"
                   "/extension1/lib/common/file"
                   "/extension1/lib/x86_64-windows/file"
                   "/extension1/lib/windows/file"
                   "/subdir/extension2/ext.manifest"}
                 (platform-resources project "x86_64-win32"))))))))

(defn- dummy-file
  []
  (File/createTempFile "dummy" ""))

(deftest cached-engine-archive-test
  (let [cache-dir (.toFile (Files/createTempDirectory "defold-test" (make-array FileAttribute 0)))]
    (testing "nothing cached initially"
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a")))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-windows" "a"))))

    (testing "caches platforms separately"
      ;; cache a build for x86_64-osx with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "a" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a"))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-windows" "a")))

      ;; cache a build for x86_64-windows with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-windows" "a" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-windows" "a"))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "a")))

    (testing "verifies key"
      ;; cache a build for x86_64-osx with key a
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "a" (dummy-file)))
      (is (nil? (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "b")))

      ;; cache anew with key b
      (is (#'native-extensions/cache-engine-archive! cache-dir "x86_64-osx" "b" (dummy-file)))
      (is (#'native-extensions/cached-engine-archive cache-dir "x86_64-osx" "b")))))

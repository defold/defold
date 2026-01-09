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

(ns integration.engine.native-extensions-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.engine.native-extensions :as native-extensions]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.repo :as repo])
  (:import [com.dynamo.bob Platform]
           [com.dynamo.bob.archive EngineVersion]))

(defn fix-engine-sha1 [f]
  (let [engine-sha1 (or (repo/detect-engine-sha1) EngineVersion/sha1)]
    (with-redefs [editor.system/defold-engine-sha1 (constantly engine-sha1)]
      (f))))

(use-fixtures :once fix-engine-sha1)

(deftest ^:native-extensions extension-roots-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world "test/resources/extension_project")
          project (test-util/setup-project! workspace)]
      (g/with-auto-evaluation-context evaluation-context
        (is (= #{"/extension1" "/subdir/extension2"}
               (set (map resource/proj-path (native-extensions/engine-extension-roots project evaluation-context)))))))))

(deftest ^:native-extensions unpack-bin-zip-test
  (testing "${ext}/plugins/${platform}.zip is extracted to /build/plugins/${ext}/plugins/ folder"
   (with-clean-system
     (let [workspace (test-util/setup-scratch-workspace! world "test/resources/extension_project")
           _ (test-util/setup-project! workspace)
           root (workspace/project-directory workspace)]
       ;; The plugins/${platform}.zip archive has a following structure:
       ;; /bin
       ;;   /${platform}
       ;;     /lsp.editor_script
       (is (.exists (io/file (str root (format "/ext_with_bin_zip/plugins/%s.zip" (.getPair (Platform/getHostPlatform)))))))
       ;; We verify that there is no file resource at
       ;; plugins/bin/${platform}/lsp.editor_script path that could be extracted
       ;; to the expected place (so it must come from the zip)
       (is (not (.exists (io/file (str root (format "/ext_with_bin_zip/plugins/bin/%s/lsp.editor_script" (.getPair (Platform/getHostPlatform))))))))
       ;; The file is extracted to its place from zip:
       (is (.exists (io/file (str root (format "/build/plugins/ext_with_bin_zip/plugins/bin/%s/lsp.editor_script" (.getPair (Platform/getHostPlatform)))))))))))

(deftest ^:native-extensions extension-resource-nodes-test
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
                   "/subdir/extension2/ext.manifest"
                   "/subdir/extension2/src/.gitkeep"}
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
                   "/subdir/extension2/ext.manifest"
                   "/subdir/extension2/src/.gitkeep"}
                 (platform-resources project "arm64-ios"))))))))

(defn- dummy-file [] (fs/create-temp-file! "dummy" ""))

(defn- blocking-async-build! [project prefs]
  (let [result (promise)]
    (test-util/run-event-loop!
      (fn [exit-event-loop!]
        (app-view/async-build! project
                               :prefs prefs
                               :result-fn (fn [build-results]
                                            (deliver result build-results)
                                            (exit-event-loop!)))))
    (deref result)))

(deftest ^:native-extensions async-build-on-build-server
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/trivial_extension")
          project (test-util/setup-project! workspace)
          test-prefs (test-util/make-build-stage-test-prefs)]
      (assert (= (native-extensions/get-build-server-url test-prefs project) "https://build-stage.defold.com"))
      (testing "clean project builds on server"
        (let [{:keys [error artifacts artifact-map etags engine]} (blocking-async-build! project test-prefs)]
          (is (nil? error))
          (is (not-empty artifacts))
          (is (not-empty artifact-map))
          (is (not-empty etags))
          (is (some? engine))
          (is (not (contains? engine :cached)))
          (is (and engine (.exists (:engine-archive engine))))))
      (testing "rebuild uses cached engine archive"
        (let [{:keys [engine error]} (blocking-async-build! project test-prefs)]
          (is (nil? error))
          (is (some? engine))
          (is (contains? engine :cached))
          (is (and engine (.exists (:engine-archive engine))))))
      (testing "bad code breaks build on server"
        (let [script (project/get-resource-node project "/printer/src/main.cpp")]
          (g/update-property! script :modified-lines conj "ought to break")
          (let [{:keys [error artifacts artifact-map etags engine]} (blocking-async-build! project test-prefs)]
            (is (some? error))
            (is (not-empty artifacts))
            (is (not-empty artifact-map))
            (is (not-empty etags))
            (is (->> error
                     (tree-seq :causes :causes)
                     (keep :message)
                     (some #(string/includes? % "ought to break"))))
            (is (nil? engine))))))))

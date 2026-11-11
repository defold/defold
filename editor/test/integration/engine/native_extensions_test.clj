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
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.engine :as engine]
            [editor.engine.native-extensions :as native-extensions]
            [editor.fs :as fs]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.yaml :as yaml]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.fn :as fn]
            [util.repo :as repo])
  (:import [clojure.lang ExceptionInfo]
           [com.defold.extender.client ExtenderClientCache ExtenderResource]
           [com.dynamo.bob IProgress Platform]
           [com.dynamo.bob.archive EngineVersion]
           [java.nio.charset StandardCharsets]))

(defn fix-engine-sha1 [f]
  (let [engine-sha1 (or (repo/detect-engine-sha1) EngineVersion/sha1)]
    (with-redefs [editor.system/defold-engine-sha1 (constantly engine-sha1)]
      (f))))

(use-fixtures :once fix-engine-sha1)

(deftest ^:native-extensions extension-roots-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
          project (test-util/setup-project! workspace)]
      (g/with-auto-evaluation-context evaluation-context
        (is (= #{"/extension1" "/subdir/extension2"}
               (set (map resource/proj-path (native-extensions/engine-extension-roots project evaluation-context)))))))))

(deftest ^:native-extensions unpack-bin-zip-test
  (testing "${ext}/plugins/${platform}.zip is extracted to /build/plugins/${ext}/plugins/ folder"
    (with-clean-system
      (let [workspace (test-util/setup-scratch-workspace! "test/resources/extension_project")
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
            (g/with-auto-evaluation-context evaluation-context
              (into #{}
                    (map (comp resource/proj-path #(g/node-value % :resource evaluation-context)))
                    (native-extensions/extension-resource-nodes project evaluation-context platform))))]
    (testing "x86_64-macos"
      (with-clean-system
        (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
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
        (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
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

(defn- extender-resource [resources path]
  (some #(when (= path (.getPath ^ExtenderResource %)) %) resources))

(defn- extender-resources [resources path]
  (filter #(= path (.getPath ^ExtenderResource %)) resources))

(defn- extender-resource-content [^ExtenderResource resource]
  (String. (.getContent resource) StandardCharsets/UTF_8))

(defn- extender-resource-yaml [resources path]
  (yaml/load (extender-resource-content (extender-resource resources path))))

(def ^:private expected-editor-build-context
  {"baseVariant" "debug"
   "withSymbols" true})

;; Engine upload resources must preserve unsaved source and manifest values
;; after their evaluation-context closes, without eagerly reading binary files.
(deftest engine-archive-request-snapshot-test
  (test-util/with-temp-project-content
    {"/game.project" [{:path ["native_extension" "app_manifest"]
                       :value "/game.appmanifest"}]
     "/game.appmanifest" ["context:" "  excludeLibs: [physics]"]
     "/extension/ext.manifest" ["name: test_extension"]
     "/extension/src/main.cpp" ["original source"]
     "/extension/lib/common/native.a" (byte-array [1 2 3])}
    (let [source (test-util/resource-node project "/extension/src/main.cpp")
          manifest (test-util/resource-node project "/game.appmanifest")]
      (test-util/set-code-editor-source! source "unsaved source\n")
      (test-util/set-code-editor-source! manifest "context:\n  excludeLibs: [physics, input]\n")
      (let [{:keys [extender-resources]}
            (g/with-auto-evaluation-context evaluation-context
              (native-extensions/make-engine-archive-request
                project "x86_64-macos" (test-util/make-test-prefs) evaluation-context))]
        (is (zero? (g/evaluation-context-count)))
        (test-util/set-code-editor-source! source "newer source\n")
        (test-util/set-code-editor-source! manifest "context: {}\n")
        (is (= "unsaved source\n"
               (extender-resource-content (extender-resource extender-resources "extension/src/main.cpp"))))
        (is (= ["physics" "input"]
               (get-in (extender-resource-yaml extender-resources "_app/app.manifest")
                       ["context" "excludeLibs"])))
        (is (= expected-editor-build-context
               (select-keys (get (extender-resource-yaml extender-resources "_app/app.manifest") "context")
                            ["baseVariant" "withSymbols"])))
        (with-open [output-stream (io/output-stream (io/file project-path "extension/lib/common/native.a"))]
          (.write output-stream (byte-array [4 5 6])))
        (is (= [4 5 6]
               (vec (.getContent ^ExtenderResource (extender-resource extender-resources "extension/lib/common/native.a")))))
        (doseq [^ExtenderResource resource extender-resources]
          (is (pos? (.getLastModified resource))))))))

;; Engine requests must preserve their cache key when their snapshot closes,
;; so cached engine archives can be reused without contacting the server.
(deftest engine-archive-cache-after-context-close-test
  (test-util/with-temp-project-content
    {"/extension/ext.manifest" ["name: test_extension"]
     "/extension/src/main.cpp" ["source"]}
    (let [test-prefs (test-util/make-test-prefs)
          {:keys [extender-platform sdk-version extender-resources]}
          (g/with-auto-evaluation-context evaluation-context
            (native-extensions/make-engine-archive-request project "x86_64-macos" test-prefs evaluation-context))

          cache-directory (doto (io/file project-path ".internal" "cache" "engine-archives")
                            fs/create-directories!)
          engine-cache (ExtenderClientCache. cache-directory)
          cache-key (.calcKey engine-cache extender-platform sdk-version extender-resources)
          archive-file (io/file project-path "engine.zip")]
      (spit archive-file "cached engine archive")
      (.put engine-cache extender-platform cache-key archive-file)
      (let [engine-descriptor (engine/get-engine project test-prefs "x86_64-macos" nil)]
        (is (true? (:cached engine-descriptor)))
        (is (= {:type :custom :version cache-key} (:id engine-descriptor)))
        (is (= "cached engine archive" (slurp (:engine-archive engine-descriptor))))
        (is (zero? (ui/run-now (g/evaluation-context-count))))))))

;; The queued graph merge must close the snapshot while the remote engine build
;; is still active, allowing resource loads to release their replay data.
(deftest engine-build-releases-materializations-test
  (test-util/with-temp-project-content
    {"/extension/ext.manifest" ["name: test_extension"]
     "/extension/src/main.cpp" ["source"]
     "/first.lua" ["return {}"]
     "/second.lua" ["return {}"]}
    (let [reached-engine-build (atom false)
          loaded-resources (atom [])
          engine-progress
          (reify IProgress
            (isCanceled [_] false)
            (close [_])
            (split [_ _] (throw (UnsupportedOperationException.)))
            (message [_ _]
              (reset! reached-engine-build true)
              (is (zero? (ui/run-now (g/evaluation-context-count))))
              (build/load-build-resources!
                project ["/first.lua" "/second.lua"]
                (fn render-load-progress! [load-progress]
                  (when (= "progress.loading-resource" (:k (progress/message load-progress)))
                    (swap! loaded-resources conj (get-in (progress/message load-progress) [:m "resource"]))
                    (is (zero? (g/evaluation-context-count)))
                    (is (= {} (:materialization-results @g/*the-system*)))
                    (is (= {} @(:shared-materialization-registry-atom @g/*the-system*)))))
                fn/constantly-false)
              (throw (ex-info "Stop before contacting the build server." {}))))]
      (is (thrown-with-msg? ExceptionInfo #"Stop before contacting the build server"
                            (engine/get-engine project (test-util/make-test-prefs) "x86_64-macos" engine-progress)))
      (is @reached-engine-build)
      (is (= ["/first.lua" "/second.lua"] (filterv #{"/first.lua" "/second.lua"} @loaded-resources)))
      (doseq [proj-path ["/first.lua" "/second.lua"]]
        (is (resource-node/loaded? (test-util/resource-node project proj-path))))
      (is (zero? (g/evaluation-context-count))))))

(deftest ^:native-extensions app-manifest-context-test
  (testing "app manifest is synthesized with editor build options"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! "test/resources/empty_project")
            project (test-util/setup-project! workspace)]
        (g/with-auto-evaluation-context evaluation-context
          (let [resources (#'native-extensions/make-extender-resources project "x86_64-macos" evaluation-context)]
            (is (= {"context" expected-editor-build-context}
                   (extender-resource-yaml resources "_app/app.manifest"))))))))
  (testing "configured app manifest is merged with editor build options"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
            project (test-util/setup-project! workspace)
            app-manifest-file (io/file (workspace/project-directory workspace) "game.appmanifest")
            app-manifest (yaml/load (slurp app-manifest-file))]
        (g/with-auto-evaluation-context evaluation-context
          (let [resources (#'native-extensions/make-extender-resources project "x86_64-macos" evaluation-context)]
            (is (= 1 (count (extender-resources resources "_app/app.manifest"))))
            (is (= (assoc app-manifest "context" expected-editor-build-context)
                   (extender-resource-yaml resources "_app/app.manifest"))))))))
  (testing "configured app manifest in flow style is merged as yaml data"
    (with-clean-system
      (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
            project (test-util/setup-project! workspace)
            app-manifest (project/get-resource-node project "/game.appmanifest")]
        (test-util/set-code-editor-source! app-manifest "{\"platforms\": {\"wasm-web\": {\"context\": {}}}}\n")
        (g/with-auto-evaluation-context evaluation-context
          (let [resources (#'native-extensions/make-extender-resources project "wasm-web" evaluation-context)]
            (is (= {"context" expected-editor-build-context
                    "platforms" {"wasm-web" {"context" {}}}}
                   (extender-resource-yaml resources "_app/app.manifest"))))))))
  (doseq [content ["true\n"
                   "null\n"
                   "context: true\n"]]
    (testing (str "configured invalid app manifest is uploaded unchanged: " (string/trim content))
      (with-clean-system
        (let [workspace (test-util/setup-workspace! "test/resources/extension_project")
              project (test-util/setup-project! workspace)
              app-manifest (project/get-resource-node project "/game.appmanifest")]
          (test-util/set-code-editor-source! app-manifest content)
          (g/with-auto-evaluation-context evaluation-context
            (let [resources (#'native-extensions/make-extender-resources project "x86_64-macos" evaluation-context)]
              (is (= content
                     (extender-resource-content (extender-resource resources "_app/app.manifest")))))))))))

;; Verifies that opening a 1.13.1 2D-only manifest upgrades its Bullet3D exclusions
;; and that Cmd+B uploads those exclusions, preventing missing Bullet linker symbols.
(deftest ^:native-extensions legacy-bullet3d-app-manifest-migration-test
  (test-util/with-temp-project-content
    {"/game.project"
     [{:path ["project" "title"]
       :value "Empty project"}
      {:path ["native_extension" "app_manifest"]
       :value "/legacy.appmanifest"}]

     "/legacy.appmanifest"
     ["context:"
      "  excludeLibs: [LinearMath, BulletDynamics, BulletCollision]"
      "  excludeSymbols: []"
      "platforms:"
      "  arm64-osx:"
      "    context:"
      "      excludeLibs: [physics, LinearMath, BulletDynamics, BulletCollision]"
      "      excludeSymbols: []"
      "      libs: [physics_2d_defold]"
      "  x86_64-win32:"
      "    context:"
      "      excludeLibs: [physics, libLinearMath, libBulletDynamics, libBulletCollision]"
      "      excludeSymbols: []"
      "  x86_64-linux:"
      "    context:"
      "      excludeLibs: [BulletDynamics]"]}
    (g/let-ec [manifest-node (project/get-resource-node project "/legacy.appmanifest" evaluation-context)
               manifest (g/node-value manifest-node :manifest evaluation-context)
               extender-resources (#'native-extensions/make-extender-resources project "arm64-macos" evaluation-context)
               upload (extender-resource-yaml extender-resources "_app/app.manifest")
               save-data (g/node-value manifest-node :save-data evaluation-context)]
      (is (true? (:dirty save-data)))
      (doseq [platform [nil :arm64-osx :x86_64-win32]]
        (let [context (if-not platform
                        (:context manifest)
                        (get-in manifest [:platforms platform :context]))
              upload-context (if-not platform
                               (get upload "context")
                               (get-in upload ["platforms" (name platform) "context"]))]
          (is (= 1 (get (frequencies (:excludeLibs context)) "script_bullet3d")))
          (is (= ["ScriptBullet3DExt"] (:excludeSymbols context)))
          (is (contains? (set (get upload-context "excludeLibs")) "script_bullet3d"))
          (is (contains? (set (get upload-context "excludeSymbols")) "ScriptBullet3DExt"))))
      (is (= ["BulletDynamics"]
             (get-in manifest [:platforms :x86_64-linux :context :excludeLibs])))
      (is (= ["physics_2d_defold"]
             (get-in upload ["platforms" "arm64-osx" "context" "libs"]))))))

(defn- blocking-async-build! [project prefs]
  @(app-view/async-build! project :prefs prefs))

(deftest ^:native-extensions async-build-on-build-server
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/trivial_extension")
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

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

(ns integration.live-update-migration-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.live-update-manifest :as manifest]
            [editor.live-update-migration :as migration]
            [editor.live-update-preview :as preview]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [com.dynamo.bob ClassLoaderResourceScanner Platform Progress Project TaskResult]
           [com.dynamo.bob.archive.liveupdate PackagePreview$Result PackageResolver$Package PackageResolver$Plan]
           [com.dynamo.bob.fs DefaultFileSystem]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files]
           [java.util.zip ZipEntry ZipOutputStream]))

(set! *warn-on-reflection* true)

(def ^:private stopped-server
  (http-server/stop! (http-server/start! (web-server/make-dynamic-handler []) 0)))

(defn- offer-migration! [project accept]
  (let [workspace (project/workspace project)]
    (extensions/reload! project :all
                        :prefs (test-util/make-test-prefs)
                        :localization test-util/localization
                        :web-server stopped-server
                        :display-output! (fn [_ message] (print message))
                        :reload-resources! (bound-fn []
                                             (workspace/resource-sync! workspace)
                                             (future/completed nil))
                        :save! (bound-fn []
                                 (test-util/save-project! project)
                                 (future/completed nil))
                        :open-resource! (fn [_ _] (future/completed nil))
                        :fetch-libraries! (fn [] (future/completed [[] true]))
                        :invoke-bob! (fn [_ _] (throw (AssertionError. "Migration must not invoke Bob"))))
    (let [rt (:rt (g/with-auto-evaluation-context evaluation-context
                   (extensions/ext-state project evaluation-context)))
          offer (rt/invoke-immediate-1 rt
                  (rt/bind rt (rt/read "return function(accept)
                                         editor.ui.show_dialog = function() return accept end
                                         local path = editor.bundle.migrate_live_update()
                                         if path then editor.save() end
                                         return path
                                       end")))
          path (rt/->clj rt @(rt/invoke-suspending-1 rt offer (rt/->lua accept)))]
      (when path
        (workspace/file-resource workspace path)))))

(defn- build-preview! [resources contents manifest-path options cancelled?]
  (let [controller (preview/controller)]
    (try
      (preview/build! controller resources contents manifest-path options cancelled?)
      (finally
        @(preview/close! controller)))))

(defn- build-packages!
  ^PackageResolver$Plan [workspace]
  (with-open [bob (Project. (.getContextClassLoader (Thread/currentThread))
                           (DefaultFileSystem.) (str (workspace/project-directory workspace)) "build")]
    (doseq [[k v] {"platform" (.getPair (Platform/getHostPlatform))
                  "variant" "debug" "archive" "true" "liveupdate" "true"}]
      (.setOption bob k v))
    (.mount bob (ClassLoaderResourceScanner.) [])
    (doseq [^TaskResult result (.build bob (Progress/discarding) (into-array String ["build"]))]
      (is (.isOk result) (str result)))
    (.getLiveUpdatePlan bob)))

;; Checked standalone and embedded proxies produce literal, deduplicated rules.
;; Migration saves the setting, avoids existing files, and survives reopening.
(deftest migrate-excluded-proxies
  (let [migrated-project-path
        (test-util/with-scratch-project "test/resources/live_update_project"
          (let [game-project (project/get-resource-node project "/game.project")]
            (test-util/set-setting! game-project ["liveupdate" "manifest"] nil)
            (test-util/save-project! project))
          (doseq [[path value] {"/episode+[1].collection" {:name "episode"}
                               "/embedded.collection" {:name "embedded"}
                               "/unchecked.collection" {:name "unchecked"}
                               "/episodee1.collection" {:name "must_not_match"}
                               "/checked.collectionproxy" {:collection "/episode+[1].collection" :exclude true}
                               "/duplicate.collectionproxy" {:collection "/episode+[1].collection" :exclude true}
                               "/unchecked.collectionproxy" {:collection "/unchecked.collection"}
                               "/main.collection" {:name "main"
                                                    :embedded-instances [{:id "proxies"
                                                                          :data {:components [{:id "checked" :component "/checked.collectionproxy"}
                                                                                              {:id "unchecked" :component "/unchecked.collectionproxy"}]
                                                                                 :embedded-components [{:id "embedded"
                                                                                                        :type "collectionproxy"
                                                                                                        :data {:collection "/embedded.collection" :exclude true}}]}}]}
                               "/liveupdate.lumanifest" {:packages [{:name "untouched"}]}}]
            (test-util/make-resource! workspace path value))
          (workspace/resource-sync! workspace)
          (let [original-files (coll/into-> ["/main.collection" "/checked.collectionproxy" "/duplicate.collectionproxy"
                                            "/unchecked.collectionproxy" "/liveupdate.lumanifest"] {}
                                 (map (fn [path] [path (slurp (workspace/file-resource workspace path))])))
                manifest-resource (offer-migration! project true)
                manifest-node (project/get-resource-node project manifest-resource)
                game-project (project/get-resource-node project "/game.project")]
            (is (= "/liveupdate1.lumanifest" (resource/proj-path manifest-resource)))
            (is (= #{"/episode+[1].collection" "/embedded.collection" "/unchecked.collection"} (g/node-value manifest-node :selected-paths)))
            (is (= ["/embedded.collection" "/episode+[1].collection" "/unchecked.collection"]
                   (mapv :name (get-in (g/node-value manifest-node :save-value) [:packages 0 :rules]))))
            (is (= "liveupdate" (get-in (g/node-value manifest-node :save-value) [:packages 0 :name])))
            (is (= "." (:output-directory (g/node-value manifest-node :save-value))))
            (is (= manifest-resource (test-util/get-setting game-project ["liveupdate" "manifest"])))
            (with-open [reader (io/reader (workspace/file-resource workspace "/game.project"))]
              (is (= "/liveupdate1.lumanifest"
                     (settings-core/get-setting (settings-core/parse-settings reader) ["liveupdate" "manifest"]))))
            (doseq [[path content] original-files]
              (is (= content (slurp (workspace/file-resource workspace path))) path))
            (test-util/clear-cached-save-data! project)
            (is (= #{} (test-util/dirty-proj-paths project)))
            (is (nil? (offer-migration! project true)))
            (let [^PackagePreview$Result result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                                                           (g/node-value workspace :live-update-output-paths))
                                                               {} "/liveupdate1.lumanifest" {} (constantly false))
                  plan (.plan result)]
              (is (= #{"/embedded.collectionc" "/episode+[1].collectionc"}
                     (set (.resources ^PackageResolver$Package (get (.packages plan) "liveupdate")))))
              (is (contains? (.base plan) "/main.collectionc"))
              (is (contains? (.base plan) "/unchecked.collectionc"))))
          (str (workspace/project-directory workspace)))]
    (test-util/with-loaded-project migrated-project-path
      (is (nil? (offer-migration! project true)))
      (is (nil? (project/get-resource-node project "/liveupdate2.lumanifest")))
      (is (= #{} (test-util/dirty-proj-paths project))))))

;; Mixed routes and dependencies shared with unchecked collections retain their
;; offline availability after migration, without modifying proxy resources.
(deftest migration-preserves-unchecked-content
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/set-setting! (project/get-resource-node project "/game.project") ["liveupdate" "manifest"] nil)
    (test-util/make-resource! workspace "/shared.script")
    (test-util/make-resource! workspace "/shared.go" {:components [{:id "script" :component "/shared.script"}]})
    (doseq [name ["included" "excluded"]]
      (test-util/make-resource! workspace (str "/" name ".collection")
                                {:name name :instances [{:id "shared" :prototype "/shared.go"}]}))
    (test-util/make-resource! workspace "/main.collection"
                              {:name "main"
                               :embedded-instances [{:id "proxies"
                                                     :data {:embedded-components
                                                            [{:id "mixed-checked" :type "collectionproxy" :data {:collection "/episode.collection" :exclude true}}
                                                             {:id "mixed-unchecked" :type "collectionproxy" :data {:collection "/episode.collection"}}
                                                             {:id "included" :type "collectionproxy" :data {:collection "/included.collection"}}
                                                             {:id "excluded" :type "collectionproxy" :data {:collection "/excluded.collection" :exclude true}}]}}]})
    (workspace/resource-sync! workspace)
    (let [original (slurp (workspace/file-resource workspace "/main.collection"))
          migrated (offer-migration! project true)
          ^PackagePreview$Result result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                                                   (g/node-value workspace :live-update-output-paths))
                                                       {} (resource/proj-path migrated) {} (constantly false))
          plan (.plan result)]
      (is (= #{"/excluded.collectionc"} (set (.resources ^PackageResolver$Package (get (.packages plan) "liveupdate")))))
      (doseq [path ["/episode.collectionc" "/included.collectionc" "/shared.goc" "/shared.scriptc"]]
        (is (contains? (.base plan) path) path))
      (is (= original (slurp (workspace/file-resource workspace "/main.collection")))))))

;; Unchecked proxies and read-only projects do not trigger migration. An
;; explicitly selected manifest takes precedence, including a missing manifest.
(deftest migration-only-without-manifest
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/proxy.collectionproxy" {:collection "/episode.collection"})
    (workspace/resource-sync! workspace)
    (let [game-project (project/get-resource-node project "/game.project")
          proxy (project/get-resource-node project "/proxy.collectionproxy")]
      (test-util/set-setting! game-project ["liveupdate" "manifest"] nil)
      (is (nil? (offer-migration! project true)))
      (g/set-property! proxy :exclude true)
      (doseq [path ["/packages.lumanifest" "/missing.lumanifest"]]
        (test-util/set-setting! game-project ["liveupdate" "manifest"] (workspace/file-resource workspace path))
        (is (nil? (offer-migration! project true)))
        (is (= path (resource/proj-path (test-util/get-setting game-project ["liveupdate" "manifest"])))))
      (test-util/set-setting! game-project ["liveupdate" "manifest"] nil)
      (let [file (io/file (workspace/file-resource workspace "/game.project"))]
        (try
          (is (.setWritable file false))
          (is (nil? (offer-migration! project true)))
          (finally
            (.setWritable file true))))
      (is (nil? (project/get-resource-node project "/liveupdate.lumanifest"))))))

;; The generated package retains supported legacy provider/output settings and
;; falls back to a valid package name for old filenames the new format rejects.
(deftest migration-provider-and-output-name
  (doseq [[mode filename expected-provider expected-name] [["Zip" "episodes.zip" "zip" "episodes"]
                                                         ["Folder" "levels" "folder" "levels"]
                                                         ["Amazon" "episodes.zip" "zip" "episodes"]
                                                         ["Zip" "nested/legacy.zip" "zip" "liveupdate"]]]
    (test-util/with-scratch-project "test/resources/live_update_project"
      (test-util/make-resource! workspace "/proxy.collectionproxy" {:collection "/episode.collection" :exclude true})
      (spit (workspace/file-resource workspace "/liveupdate.settings")
            (str "[liveupdate]\nmode = " mode "\nzip-filename = " filename "\noutput-folder-name = " filename "\n"
                 "zip-filepath = ${HOME}/zip-output\noutput-directory = folder-output\nzip-compression-level = 8\n"
                 "amazon-credential-profile = deployment\namazon-bucket = packages\namazon-prefix = releases/v1\n"))
      (workspace/resource-sync! workspace)
      (test-util/set-setting! (project/get-resource-node project "/game.project") ["liveupdate" "manifest"] nil)
      (let [settings-before (slurp (workspace/file-resource workspace "/liveupdate.settings"))
            node (project/get-resource-node project (offer-migration! project true))
            package (first (:packages (g/node-value node :save-value)))]
        (is (= expected-provider (get package :provider "zip")))
        (is (= expected-name (:name package)))
        (is (= (if (= "folder" expected-provider) "folder-output" "${HOME}/zip-output")
               (:output-directory (g/node-value node :save-value))))
        (is (= (when (= "zip" expected-provider)
                 [{:provider "zip" :properties [{:name "compression-level" :value "8"}]}])
               (:providers (g/node-value node :save-value))))
        (is (= settings-before (slurp (workspace/file-resource workspace "/liveupdate.settings"))))))))

;; Opening the bundle dialog can migrate configured settings without excluded
;; proxies. Declining has no side effects, and accepting retains the old file.
(deftest offer-settings-migration
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [game-project (project/get-resource-node project "/game.project")
          legacy-resource (workspace/file-resource workspace "/deployment.settings")
          legacy-content "[liveupdate]\nmode = Folder\noutput-folder-name = episodes\noutput-directory = packages\n"
          project-content (slurp (workspace/file-resource workspace "/game.project"))]
      (spit legacy-resource legacy-content)
      (workspace/resource-sync! workspace)
      (test-util/set-setting! game-project ["liveupdate" "manifest"] nil)
      (test-util/set-setting! game-project ["liveupdate" "settings"] legacy-resource)
      (is (= "/liveupdate.lumanifest"
             (:path (g/with-auto-evaluation-context evaluation-context
                      (migration/prepare project evaluation-context)))))
      (is (nil? (project/get-resource-node project "/liveupdate.lumanifest")))
      (is (nil? (offer-migration! project false)))
      (is (nil? (test-util/get-setting game-project ["liveupdate" "manifest"])))
      (is (= project-content (slurp (workspace/file-resource workspace "/game.project"))))
      (let [manifest-resource (offer-migration! project true)
            value (g/node-value (project/get-resource-node project manifest-resource) :save-value)]
        (is (= "folder" (get-in value [:packages 0 :provider])))
        (is (= "episodes" (get-in value [:packages 0 :name])))
        (is (= "packages" (:output-directory value)))
        (is (= legacy-resource (test-util/get-setting game-project ["liveupdate" "settings"])))
        (is (= legacy-content (slurp legacy-resource)))
        (is (nil? (offer-migration! project true)))))))

;; Migrating a previously built project must produce usable packages without
;; overwriting its legacy ZIP/folder or other files occupying candidate names.
(deftest migration-preserves-existing-outputs
  (doseq [[mode extension] [["Zip" ".zip"] ["Folder" ""]]]
    (test-util/with-scratch-project "test/resources/live_update_project"
      (test-util/make-resource! workspace "/main.collection"
                                {:name "main"
                                 :embedded-instances [{:id "proxies"
                                                       :data {:embedded-components [{:id "episode" :type "collectionproxy"
                                                                                     :data {:collection "/episode.collection" :exclude true}}]}}]})
      (spit (workspace/file-resource workspace "/liveupdate.settings")
            (str "[liveupdate]\nmode = " mode "\nzip-filename = episodes.zip\noutput-folder-name = episodes\n"
                 "zip-filepath = packages\noutput-directory = packages\n"))
      (workspace/resource-sync! workspace)
      (test-util/set-setting! (project/get-resource-node project "/game.project") ["liveupdate" "manifest"] nil)
      (test-util/save-project! project)
      (build-packages! workspace)
      (let [directory (io/file (workspace/project-directory workspace) "packages")
            old-output (io/file directory (if (= "Zip" mode) "episodes.zip" "episodes/liveupdate.game.dmanifest"))
            old-content (vec (Files/readAllBytes (.toPath old-output)))
            occupied (io/file directory (str "episodes1" extension))]
        (spit occupied "Existing user file")
        (workspace/resource-sync! workspace)
        (let [node (project/get-resource-node project (offer-migration! project true))]
          (is (= "episodes2" (get-in (g/node-value node :save-value) [:packages 0 :name])))
          (is (= "packages" (:output-directory (g/node-value node :save-value)))))
        (let [plan (build-packages! workspace)]
          (is (= #{"/episode.collectionc"}
                 (set (.resources ^PackageResolver$Package (get (.packages plan) "episodes2"))))))
        (is (.isFile (io/file directory (if (= "Zip" mode) "episodes2.zip" "episodes2/liveupdate.game.dmanifest"))))
        (is (= old-content (vec (Files/readAllBytes (.toPath old-output)))))
        (is (= "Existing user file" (slurp occupied)))))))

;; Library proxies participate in migration without writing to their archive.
(deftest migrate-library-proxy
  (test-util/with-project-default-library-directory
    (test-util/with-scratch-project "test/resources/live_update_project"
      (let [uri (URI/create "file:/liveupdate-migration-test")
            library (test-support/library-file (workspace/project-directory workspace) uri "")]
        (io/make-parents library)
        (with-open [output (ZipOutputStream. (io/output-stream library))]
          (doseq [[path content] [["game.project" "[library]\ninclude_dirs = packages\n"]
                                 ["packages/proxy.collectionproxy" "collection: \"/packages/level.collection\"\nexclude: true\n"]
                                 ["packages/level.collection" "name: \"level\"\n"]]]
            (.putNextEntry output (ZipEntry. ^String path))
            (.write output (.getBytes ^String content StandardCharsets/UTF_8))
            (.closeEntry output)))
        (test-util/set-cached-project-dependencies! workspace [uri])
        (workspace/resource-sync! workspace)
        (test-util/set-setting! (project/get-resource-node project "/game.project") ["liveupdate" "manifest"] nil)
        (let [archive-before (vec (java.nio.file.Files/readAllBytes (.toPath library)))
              manifest-node (project/get-resource-node project (offer-migration! project true))]
          (is (= #{"/packages/level.collection"} (g/node-value manifest-node :selected-paths)))
          (is (= archive-before (vec (java.nio.file.Files/readAllBytes (.toPath library))))))))))

;; Copyright 2020-2025 The Defold Foundation
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

(ns integration.save-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.code.util :as code.util]
            [editor.defold-project :as project]
            [editor.disk :as disk]
            [editor.fs :as fs]
            [editor.git :as git]
            [editor.git-test :refer [with-git]]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [support.test-support :refer [spit-until-new-mtime touch-until-new-mtime with-clean-system]]
            [util.coll :as coll]
            [util.digest :as digest]
            [util.text-util :as text-util])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io File]
           [org.apache.commons.io FileUtils]
           [org.eclipse.jgit.api Git ResetCommand$ResetType]))

(set! *warn-on-reflection* true)

(defn- setup-scratch
  [ws-graph]
  (let [workspace (test-util/setup-scratch-workspace! ws-graph test-util/project-path)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(deftest save-after-delete
  (with-clean-system
    (let [[_workspace project] (setup-scratch world)
          atlas-id (test-util/resource-node project "/switcher/switcher.atlas")]
      (asset-browser/delete [(g/node-value atlas-id :resource)])
      (is (not (g/error? (project/all-save-data project)))))))

(deftest save-after-external-delete
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          atlas-id (test-util/resource-node project "/switcher/switcher.atlas")
          path (resource/abs-path (g/node-value atlas-id :resource))]
      (fs/delete-file! (File. path))
      (workspace/resource-sync! workspace)
      (is (not (g/error? (project/all-save-data project)))))))

(deftest save-after-rename
  (with-clean-system
    (let [[_workspace project] (setup-scratch world)
          atlas-id (test-util/resource-node project "/switcher/switcher.atlas")]
      (asset-browser/rename [(g/node-value atlas-id :resource)] "switcher2" test-util/localization)
      (is (not (g/error? (project/all-save-data project)))))))

(defn- resource-line-endings
  [resource]
  (when (and (resource/exists? resource))
    (text-util/scan-line-endings (io/make-reader resource nil))))

(defn- line-endings-by-resource
  [project]
  (into {}
        (keep (fn [{:keys [resource]}]
                (when-let [line-ending-type (resource-line-endings resource)]
                  [resource line-ending-type])))
        (project/all-save-data project)))

(defn- set-autocrlf!
  [^Git git enabled]
  (-> git .getRepository .getConfig (.setString "core" nil "autocrlf" (if enabled "true" "false")))
  nil)

(defn- clean-checkout! [^Git git]
  (doseq [^File file (-> git .getRepository .getWorkTree .listFiles)]
    (when-not (= ".git" (.getName file))
      (FileUtils/deleteQuietly file)))
  (-> git .reset (.setRef "HEAD") (.setMode ResetCommand$ResetType/HARD) .call)
  nil)

(deftest save-respects-line-endings
  (with-git [project-path (test-util/make-temp-project-copy! test-util/project-path)
             git (git/init project-path)]
    (set-autocrlf! git false)
    (-> git .add (.addFilepattern ".") .call)
    (-> git .commit (.setMessage "init repo") .call)

    (testing "autocrlf false"
      (set-autocrlf! git false)
      (clean-checkout! git)
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              line-endings-before (line-endings-by-resource project)
              {:keys [lf crlf] :or {lf 0 crlf 0}} (frequencies (map second line-endings-before))]
          (is (< 100 lf))
          (is (> 100 crlf))
          (disk/write-save-data-to-disk! (project/dirty-save-data project) nil nil)
          (is (= line-endings-before (line-endings-by-resource project))))))

    (testing "autocrlf true"
      (set-autocrlf! git true)
      (clean-checkout! git)
      (with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              line-endings-before (line-endings-by-resource project)
              {:keys [lf crlf] :or {lf 0 crlf 0}} (frequencies (map second line-endings-before))]
          (is (> 100 lf))
          (is (< 100 crlf))
          (disk/write-save-data-to-disk! (project/dirty-save-data project) nil nil)
          (is (= line-endings-before (line-endings-by-resource project))))))))

(defn- workspace-file
  ^File [workspace proj-path]
  (assert (= \/ (first proj-path)))
  (File. (workspace/project-directory workspace) (subs proj-path 1)))

(defn- slurp-file
  ^String [workspace proj-path]
  (let [f (workspace-file workspace proj-path)]
    (slurp f)))

(defn- spit-file!
  [workspace proj-path content]
  (let [f (workspace-file workspace proj-path)]
    (fs/create-parent-directories! f)
    (spit-until-new-mtime f content)))

(defn- touch-file!
  [workspace proj-path]
  (let [f (workspace-file workspace proj-path)]
    (fs/create-parent-directories! f)
    (touch-until-new-mtime f)))

(def ^:private delete-file! (comp fs/delete-file! workspace-file))

(defn- check-eager-loaded-save-data! [project proj-path expected-save-text expected-save-value]
  (let [workspace (project/workspace project)
        resource (workspace/find-resource workspace proj-path)
        node-id (project/get-resource-node project proj-path)]
    (and (is (= (if (resource/stateful? resource)
                  (test-util/cacheable-save-data-outputs node-id)
                  #{})
                (test-util/cached-save-data-outputs node-id))
             "Save data is cached.")
         (is (= expected-save-value (g/node-value node-id :save-value))
             "Save value reflects expected state.")
         (is (= expected-save-text (slurp-file workspace proj-path))
             "File contents reflect expected state.")
         (let [save-data (g/node-value node-id :save-data)]
           (and (is (false? (:dirty save-data)))
                (is (= resource (:resource save-data)))
                (is (= expected-save-value (:save-value save-data)))
                (is (= expected-save-text (resource-node/save-data-content save-data)))))
         (is (resource-node/loaded? node-id)
             "Node has been registered as loaded.")
         (is (= (resource-node/save-value->source-value expected-save-value (resource/resource-type resource))
                (g/node-value node-id :source-value))
             "Source value reflects expected state.")
         (is (= (digest/string->sha256-hex expected-save-text)
                (get (g/node-value workspace :disk-sha256s-by-node-id) node-id))
             "Disk SHA256 reflects expected state."))))

(defn- check-lazy-loaded-save-data! [project proj-path expected-save-text]
  (let [workspace (project/workspace project)
        resource (workspace/find-resource workspace proj-path)
        node-id (project/get-resource-node project resource)]
    (and (is (= (if (resource/stateful? resource)
                  (test-util/cacheable-save-data-outputs node-id)
                  #{})
                (test-util/cached-save-data-outputs node-id))
             "Save data is cached.")
         (is (nil? (g/node-value node-id :save-value))
             "Save value has not been loaded yet.")
         (is (= expected-save-text (slurp-file workspace proj-path))
             "File contents are as expected.")
         (let [save-data (g/node-value node-id :save-data)]
           (and (is (false? (:dirty save-data)))
                (is (= resource (:resource save-data)))
                (is (nil? (:save-value save-data)))
                (is (nil? (resource-node/save-data-content save-data)))))
         (is (resource-node/loaded? node-id)
             "Node has been registered as loaded (i.e. it is functional, even if lazy-loading happens later).")
         (is (nil? (g/node-value node-id :source-value))
             "Source value has not been loaded yet.")
         (is (nil? (get (g/node-value workspace :disk-sha256s-by-node-id) node-id))
             "Disk SHA256 has not been loaded yet."))))

(deftest async-reload-test
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          external-game-object-text (slurp (workspace/find-resource workspace "/game_object/empty_props.go"))
          external-json-text "{\"item\" : \"Added externally\"}"
          external-lua-text "-- Edited externally"
          external-markdown-text "# Added externally"
          external-html-text "<div>Added externally</div>"]
      (test-util/run-event-loop!
        (fn [exit-event-loop!]
          (let [dirty-save-data-before (project/dirty-save-data project)]

            ;; Modified externally.
            (spit-file! workspace "/game_object/test.go" external-game-object-text)
            (spit-file! workspace "/json/test.json" external-json-text)
            (spit-file! workspace "/script/test_module.lua" external-lua-text)
            (spit-file! workspace "/markdown/test.md" external-markdown-text)
            (spit-file! workspace "/html/test.html" external-markdown-text)

            ;; Added externally.
            (spit-file! workspace "/added_externally.go" external-game-object-text)
            (spit-file! workspace "/added_externally.json" external-json-text)
            (spit-file! workspace "/added_externally.lua" external-lua-text)
            (spit-file! workspace "/added_externally.md" external-markdown-text)
            (spit-file! workspace "/added_externally.html" external-html-text)

            (disk/async-reload! progress/null-render-progress! workspace [] nil
                                (fn [successful?]
                                  (when (is successful?)

                                    (testing "Save data is cached for externally modified and added files."
                                      (letfn [(cached-save-data-outputs [proj-path]
                                                (if-let [node-id (project/get-resource-node project proj-path)]
                                                  (test-util/cached-save-data-outputs node-id)
                                                  :resource-node-not-found))]
                                        (is (= #{:save-data :save-value} (cached-save-data-outputs "/game_object/test.go")))
                                        (is (= #{:save-data} (cached-save-data-outputs "/json/test.json"))) ; The save-value output is not :cached. Lazy loaded.
                                        (is (= #{:save-data} (cached-save-data-outputs "/script/test_module.lua"))) ; The save-value output is not :cached.
                                        (is (= #{:save-data} (cached-save-data-outputs "/markdown/test.md")))
                                        (is (= #{:save-data} (cached-save-data-outputs "/html/test.html")))

                                        (is (= #{:save-data :save-value} (cached-save-data-outputs "/added_externally.go")))
                                        (is (= #{:save-data} (cached-save-data-outputs "/added_externally.json"))) ; The save-value output is not :cached. Lazy loaded.
                                        (is (= #{:save-data} (cached-save-data-outputs "/added_externally.lua"))) ; The save-value output is not :cached.
                                        (is (= #{:save-data} (cached-save-data-outputs "/added_externally.md")))
                                        (is (= #{:save-data} (cached-save-data-outputs "/added_externally.html")))))

                                    (testing "Expectations for reloaded resources."
                                      (let [external-game-object-pb-map (protobuf/read-map-without-defaults GameObject$PrototypeDesc (workspace/find-resource workspace "/game_object/test.go"))
                                            external-lua-lines (code.util/split-lines external-lua-text)
                                            external-markdown-lines (code.util/split-lines external-markdown-text)
                                            external-html-lines (code.util/split-lines external-html-text)]
                                        (is (check-eager-loaded-save-data! project "/game_object/test.go" external-game-object-text external-game-object-pb-map))
                                        (is (check-lazy-loaded-save-data! project "/json/test.json" external-json-text))
                                        (is (check-eager-loaded-save-data! project "/script/test_module.lua" external-lua-text external-lua-lines))
                                        (is (check-eager-loaded-save-data! project "/markdown/test.md" external-markdown-text external-markdown-lines))
                                        (is (check-eager-loaded-save-data! project "/html/test.html" external-markdown-text external-markdown-lines))

                                        (is (check-eager-loaded-save-data! project "/added_externally.go" external-game-object-text external-game-object-pb-map))
                                        (is (check-lazy-loaded-save-data! project "/added_externally.json" external-json-text))
                                        (is (check-eager-loaded-save-data! project "/added_externally.lua" external-lua-text external-lua-lines))
                                        (is (check-eager-loaded-save-data! project "/added_externally.md" external-markdown-text external-markdown-lines))
                                        (is (check-eager-loaded-save-data! project "/added_externally.html" external-html-text external-html-lines))))

                                    (testing "Save data unaffected."
                                      (is (= dirty-save-data-before (project/dirty-save-data project))))

                                    (testing "External modifications are seen by the editor."
                                      (is (= {"script" 1} (g/node-value (project/get-resource-node project "/game_object/test.go") :id-counts)))
                                      (is (= (code.util/split-lines external-json-text) (g/node-value (project/get-resource-node project "/json/test.json") :lines)))
                                      (is (= (code.util/split-lines external-lua-text) (g/node-value (project/get-resource-node project "/script/test_module.lua") :lines)))
                                      (is (= (code.util/split-lines external-markdown-text) (g/node-value (project/get-resource-node project "/markdown/test.md") :lines))))

                                    (testing "Externally added files are seen by the editor."
                                      (is (some? (workspace/find-resource workspace "/added_externally.go")))
                                      (is (some? (project/get-resource-node project "/added_externally.go")))
                                      (is (some? (workspace/find-resource workspace "/added_externally.json")))
                                      (is (some? (project/get-resource-node project "/added_externally.json")))
                                      (is (some? (workspace/find-resource workspace "/added_externally.lua")))
                                      (is (some? (project/get-resource-node project "/added_externally.lua")))
                                      (is (some? (workspace/find-resource workspace "/added_externally.md")))
                                      (is (some? (project/get-resource-node project "/added_externally.md")))
                                      (is (= {"script" 1} (g/node-value (project/get-resource-node project "/added_externally.go") :id-counts)))
                                      (is (= (code.util/split-lines external-json-text) (g/node-value (project/get-resource-node project "/added_externally.json") :lines)))
                                      (is (= (code.util/split-lines external-lua-text) (g/node-value (project/get-resource-node project "/added_externally.lua") :lines)))
                                      (is (= (code.util/split-lines external-markdown-text) (g/node-value (project/get-resource-node project "/added_externally.md") :lines))))

                                    (testing "Can delete externally added files from within the editor."
                                      (delete-file! workspace "/added_externally.go")
                                      (delete-file! workspace "/added_externally.json")
                                      (delete-file! workspace "/added_externally.lua")
                                      (delete-file! workspace "/added_externally.md")
                                      (workspace/resource-sync! workspace)
                                      (is (nil? (workspace/find-resource workspace "/added_externally.go")))
                                      (is (nil? (project/get-resource-node project "/added_externally.go")))
                                      (is (nil? (workspace/find-resource workspace "/added_externally.json")))
                                      (is (nil? (project/get-resource-node project "/added_externally.json")))
                                      (is (nil? (workspace/find-resource workspace "/added_externally.lua")))
                                      (is (nil? (project/get-resource-node project "/added_externally.lua")))
                                      (is (nil? (workspace/find-resource workspace "/added_externally.md")))
                                      (is (nil? (project/get-resource-node project "/added_externally.md")))))

                                  (exit-event-loop!)))))))))

(deftest async-save-test
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          external-game-object-text (slurp (workspace/find-resource workspace "/game_object/empty_props.go"))
          internal-edit-text "-- Edited by us"
          external-json-text "{\"item\" : \"Added externally\"}"
          external-lua-text "-- Modified externally"
          external-markdown-text "# Added externally"]
      (test-util/run-event-loop!
        (fn [exit-event-loop!]

          ;; Edited by us.
          (test-util/set-code-editor-source! (test-util/resource-node project "/script/props.script") internal-edit-text)

          ;; Modified externally.
          (spit-file! workspace "/game_object/test.go" external-game-object-text)
          (spit-file! workspace "/json/test.json" external-json-text)
          (spit-file! workspace "/script/test_module.lua" external-lua-text)
          (spit-file! workspace "/markdown/test.md" external-markdown-text)

          ;; Added externally.
          (spit-file! workspace "/added_externally.go" external-game-object-text)
          (spit-file! workspace "/added_externally.json" external-json-text)
          (spit-file! workspace "/added_externally.lua" external-lua-text)
          (spit-file! workspace "/added_externally.md" external-markdown-text)

          (disk/async-save! progress/null-render-progress! progress/null-render-progress! project/dirty-save-data project nil
                            (fn [successful?]
                              (when (is successful?)

                                (testing "Expectations for our edited resource."
                                  (let [proj-path "/script/props.script"
                                        internal-edit-lines (code.util/split-lines internal-edit-text)]
                                    (check-eager-loaded-save-data! project proj-path internal-edit-text internal-edit-lines)))

                                (testing "No files are still in need of saving."
                                  (let [dirty-proj-paths (into #{} (map (comp resource/proj-path :resource)) (project/dirty-save-data project))]
                                    (is (not (contains? dirty-proj-paths "/script/props.script")))

                                    (is (not (contains? dirty-proj-paths "/game_object/test.go")))
                                    (is (not (contains? dirty-proj-paths "/json/test.json")))
                                    (is (not (contains? dirty-proj-paths "/script/test_module.lua")))
                                    (is (not (contains? dirty-proj-paths "/markdown/test.md")))

                                    (is (not (contains? dirty-proj-paths "/added_externally.go")))
                                    (is (not (contains? dirty-proj-paths "/added_externally.json")))
                                    (is (not (contains? dirty-proj-paths "/added_externally.lua")))
                                    (is (not (contains? dirty-proj-paths "/added_externally.md")))))

                                (testing "Externally modified files are not overwritten by the editor."
                                  (is (= external-game-object-text (slurp-file workspace "/game_object/test.go")))
                                  (is (= external-json-text (slurp-file workspace "/json/test.json")))
                                  (is (= external-lua-text (slurp-file workspace "/script/test_module.lua")))
                                  (is (= external-markdown-text (slurp-file workspace "/markdown/test.md"))))

                                (testing "Externally added files are not overwritten by the editor."
                                  (is (= external-game-object-text (slurp-file workspace "/added_externally.go")))
                                  (is (= external-json-text (slurp-file workspace "/added_externally.json")))
                                  (is (= external-lua-text (slurp-file workspace "/added_externally.lua")))
                                  (is (= external-markdown-text (slurp-file workspace "/added_externally.md")))))

                              (exit-event-loop!))))))))

(deftest save-data-remains-in-cache-test
  (let [cache-size 50
        retained-labels #{:save-data :save-value}]
    (with-clean-system {:cache-size cache-size
                        :cache-retain? project/cache-retain?}
      (let [workspace (test-util/setup-workspace! world)
            project (test-util/setup-project! workspace)
            invalidated-save-data-endpoints-atom (atom #{})
            cacheable-save-data-endpoints (into (sorted-set)
                                                (comp (map first)
                                                      (mapcat (partial test-util/cacheable-save-data-endpoints (g/now))))
                                                (g/sources-of project :save-data))]
        ;; The source-value output will be evicted from the cache for resource
        ;; nodes whose save-data was dirty. This needs to happen, as the
        ;; source-value output should always represent the on-disk state.
        ;; Here, we keep track of (and prevent) disk writes, so we can exclude
        ;; these endpoints from what we expect to be in the cache after the
        ;; save operation concludes.
        (with-redefs [disk/write-save-data-to-disk!
                      (fn mock-write-save-data-to-disk! [save-datas invalidate-counters _opts]
                        (swap! invalidated-save-data-endpoints-atom
                               (fn [invalidated-save-data-endpoints]
                                 (into invalidated-save-data-endpoints
                                       (mapcat (fn [save-data]
                                                 (let [resource (:resource save-data)
                                                       resource-node (test-util/resource-node project resource)]
                                                   (map (partial gt/endpoint resource-node)
                                                        retained-labels))))
                                       save-datas)))
                        (let [save-data-sha256s (mapv resource-node/save-data-sha256 save-datas)]
                          (disk/make-post-save-actions save-datas save-data-sha256s invalidate-counters)))]
          (test-util/run-event-loop!
            (fn [exit-event-loop!]
              (g/clear-system-cache!)
              (disk/async-save!
                progress/null-render-progress! progress/null-render-progress! project/dirty-save-data project nil
                (fn [successful?]
                  (when (is successful?)

                    (testing "Save data values remain in cache even though we've exceeded the cache limit."
                      (let [expected-cached-save-data-endpoints (set/difference cacheable-save-data-endpoints @invalidated-save-data-endpoints-atom)]
                        (is (set/subset? expected-cached-save-data-endpoints (test-util/cached-endpoints)))))

                    (testing "Save data values are always evicted from the cache after their dependencies change."
                      (let [graphics-atlas (test-util/resource-node project "/graphics/atlas.atlas")]
                        (is (contains? (test-util/cached-endpoints) (gt/endpoint graphics-atlas :save-data)))
                        (test-util/prop! graphics-atlas :margin 10)
                        (is (not (contains? (test-util/cached-endpoints) (gt/endpoint graphics-atlas :save-data))))))

                    (testing "Save data values are always evicted from the cache when it is explicitly cleared."
                      (g/clear-system-cache!)
                      (is (empty? (g/cache)))))

                  (exit-event-loop!))))))))))

(deftest edit-during-save-test
  (with-clean-system {:cache-size 50
                      :cache-retain? project/cache-retain?}
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          edited-before-save (test-util/resource-node project "/script/props.script")
          edited-during-save (test-util/resource-node project "/script/test_module.lua")
          written-save-data-by-proj-path-atom (atom {})]
      ;; Prevent disk writes, but also use this opportunity to simulate a user
      ;; edit to one of the saved files from the UI thread while the save is in
      ;; progress. The purpose of this is to verify that the edits that were
      ;; made while saving are unsaved after we wrap up the save process.
      (with-redefs [disk/write-save-data-to-disk!
                    (fn mock-write-save-data-to-disk! [save-datas invalidate-counters _opts]
                      ;; This function runs on a background thread as part of
                      ;; the save process. Simulate a concurrent edit on the UI
                      ;; thread before proceeding with the background thread.
                      (ui/run-now
                        (test-util/set-code-editor-source! edited-during-save "-- Edit of test_module.lua during save."))

                      ;; Store the "written" save-data so we can check it later.
                      (swap! written-save-data-by-proj-path-atom
                             (fn [written-save-data-by-proj-path]
                               (into written-save-data-by-proj-path
                                     (map (coll/pair-fn (comp resource/proj-path :resource)))
                                     save-datas)))

                      ;; Proceed with the save process.
                      (let [save-data-sha256s (mapv resource-node/save-data-sha256 save-datas)]
                        (disk/make-post-save-actions save-datas save-data-sha256s invalidate-counters)))]
        (test-util/run-event-loop!
          (fn [exit-event-loop!]
            (g/clear-system-cache!)

            ;; Make some edits before saving.
            (test-util/set-code-editor-source! edited-before-save "-- Edit of props.script before save.")
            (test-util/set-code-editor-source! edited-during-save "-- Edit of test_module.lua before save.")

            (disk/async-save!
              progress/null-render-progress! progress/null-render-progress! project/dirty-save-data project nil
              (fn [successful?]
                (when (is successful?)
                  (let [cached-endpoints (test-util/cached-endpoints)]

                    (is (contains? cached-endpoints (g/endpoint edited-before-save :save-data))
                        "Save data is cached for the file that remained unmodified while saving.")

                    (is (not (contains? cached-endpoints (g/endpoint edited-during-save :save-data)))
                        "Save data is not cached for the file that was edited while saving.")

                    (testing "The file that was edited before saving."
                      (let [save-data (g/node-value edited-before-save :save-data)
                            resource (:resource save-data)
                            proj-path (resource/proj-path resource)
                            written-save-data (get @written-save-data-by-proj-path-atom proj-path)]
                        (is (false? (:dirty save-data)))
                        (is (= ["-- Edit of props.script before save."] (:save-value save-data)))
                        (is (= ["-- Edit of props.script before save."] (:save-value written-save-data)))))

                    (testing "The file that was edited while saving."
                      (let [save-data (g/node-value edited-during-save :save-data)
                            resource (:resource save-data)
                            proj-path (resource/proj-path resource)
                            written-save-data (get @written-save-data-by-proj-path-atom proj-path)]
                        (is (true? (:dirty save-data)))
                        (is (= ["-- Edit of test_module.lua during save."] (:save-value save-data)))
                        (is (= ["-- Edit of test_module.lua before save."] (:save-value written-save-data)))))))

                (exit-event-loop!)))))))))

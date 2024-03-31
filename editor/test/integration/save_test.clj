;; Copyright 2020-2024 The Defold Foundation
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
            [editor.defold-project :as project]
            [editor.disk :as disk]
            [editor.fs :as fs]
            [editor.git :as git]
            [editor.git-test :refer [with-git]]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [support.test-support :refer [spit-until-new-mtime touch-until-new-mtime with-clean-system]]
            [util.text-util :as text-util])
  (:import [java.io File]
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
      (asset-browser/rename [(g/node-value atlas-id :resource)] "switcher2")
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
          (project/write-save-data-to-disk! (project/dirty-save-data project) nil)
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
          (project/write-save-data-to-disk! (project/dirty-save-data project) nil)
          (is (= line-endings-before (line-endings-by-resource project))))))))

(defn- workspace-file
  ^File [workspace proj-path]
  (assert (= \/ (first proj-path)))
  (File. (workspace/project-path workspace) (subs proj-path 1)))

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

(deftest async-reload-test
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (test-util/run-event-loop!
        (fn [exit-event-loop!]
          (let [dirty-save-data-before (project/dirty-save-data project)]

            ;; Edited externally.
            (touch-file! workspace "/added_externally.md")
            (spit-file! workspace "/script/test_module.lua" "-- Edited externally")

            (disk/async-reload! progress/null-render-progress! workspace [] nil
                                (fn [successful?]
                                  (when (is successful?)

                                    (testing "Save data unaffected."
                                      (is (= dirty-save-data-before (project/dirty-save-data project))))

                                    (testing "External modifications are seen by the editor."
                                      (is (= ["-- Edited externally"] (g/node-value (project/get-resource-node project "/script/test_module.lua") :lines))))

                                    (testing "Externally added files are seen by the editor."
                                      (is (some? (workspace/find-resource workspace "/added_externally.md")))
                                      (is (some? (project/get-resource-node project "/added_externally.md"))))

                                    (testing "Can delete externally added files from within the editor."
                                      (delete-file! workspace "/added_externally.md")
                                      (workspace/resource-sync! workspace)
                                      (is (nil? (workspace/find-resource workspace "/added_externally.md")))
                                      (is (nil? (project/get-resource-node project "/added_externally.md")))))

                                  (exit-event-loop!)))))))))

(deftest async-save-test
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (test-util/run-event-loop!
        (fn [exit-event-loop!]

          ;; Edited by us.
          (test-util/set-code-editor-source! (test-util/resource-node project "/script/props.script") "-- Edited by us")

          ;; Edited externally.
          (touch-file! workspace "/added_externally.md")
          (spit-file! workspace "/script/test_module.lua" "-- Edited externally")

          (disk/async-save! progress/null-render-progress! progress/null-render-progress! project nil
                            (fn [successful?]
                              (when (is successful?)

                                (testing "No files are still in need of saving."
                                  (let [dirty-proj-paths (into #{} (map (comp resource/proj-path :resource)) (project/dirty-save-data project))]
                                    (is (not (contains? dirty-proj-paths "/added_externally.md")))
                                    (is (not (contains? dirty-proj-paths "/script/props.script")))
                                    (is (not (contains? dirty-proj-paths "/script/test_module.lua")))))

                                (testing "Externally modified files are not overwritten by the editor."
                                  (is (= "-- Edited externally" (slurp (workspace/find-resource workspace "/script/test_module.lua")))))

                                (testing "External modifications are seen by the editor."
                                  (is (= ["-- Edited externally"] (g/node-value (project/get-resource-node project "/script/test_module.lua") :lines))))

                                (testing "Externally added files are seen by the editor."
                                  (is (some? (workspace/find-resource workspace "/added_externally.md")))
                                  (is (some? (project/get-resource-node project "/added_externally.md"))))

                                (testing "Can delete externally added files from within the editor."
                                  (delete-file! workspace "/added_externally.md")
                                  (workspace/resource-sync! workspace)
                                  (is (nil? (workspace/find-resource workspace "/added_externally.md")))
                                  (is (nil? (project/get-resource-node project "/added_externally.md")))))

                              (exit-event-loop!))))))))

(deftest save-data-remains-in-cache
  (let [cache-size 50
        retained-labels #{:save-data :source-value}]
    (letfn [(cached-endpoints []
              (into (sorted-set)
                    (keys (g/cache))))]
      (with-clean-system {:cache-size cache-size
                          :cache-retain? project/cache-retain?}
        (let [workspace (test-util/setup-workspace! world)
              project (test-util/setup-project! workspace)
              invalidated-save-data-endpoints-atom (atom #{})
              cacheable-save-data-endpoints (into (sorted-set)
                                                  (mapcat (fn [[node-id]]
                                                            (let [node-type (g/node-type* node-id)
                                                                  cached-outputs (in/cached-outputs node-type)]
                                                              (map (partial gt/endpoint node-id)
                                                                   (set/intersection cached-outputs retained-labels)))))
                                                  (g/sources-of project :save-data))]
          ;; The source-value output will be evicted from the cache for resource
          ;; nodes whose save-data was dirty. This needs to happen, as the
          ;; source-value output should always represent the on-disk state.
          ;; Here, we keep track of (and prevent) disk writes, so we can exclude
          ;; these endpoints from what we expect to be in the cache after the
          ;; save operation concludes.
          (with-redefs [project/write-save-data-to-disk!
                        (fn mock-write-save-data-to-disk! [save-data _opts]
                          (swap! invalidated-save-data-endpoints-atom
                                 (fn [invalidated-save-data-endpoints]
                                   (into invalidated-save-data-endpoints
                                         (mapcat (fn [save-data]
                                                   (let [resource (:resource save-data)
                                                         resource-node (test-util/resource-node project resource)]
                                                     (map (partial gt/endpoint resource-node)
                                                          retained-labels))))
                                         save-data))))]
            (test-util/run-event-loop!
              (fn [exit-event-loop!]
                (g/clear-system-cache!)
                (disk/async-save! progress/null-render-progress! progress/null-render-progress! project nil
                                  (fn [successful?]
                                    (when (is successful?)

                                      (testing "Save data values remain in cache even though we've exceeded the cache limit."
                                        (let [expected-cached-save-data-endpoints (set/difference cacheable-save-data-endpoints @invalidated-save-data-endpoints-atom)]
                                          (is (set/subset? expected-cached-save-data-endpoints (cached-endpoints)))))

                                      (testing "Save data values are always evicted from the cache after their dependencies change."
                                        (let [graphics-atlas (test-util/resource-node project "/graphics/atlas.atlas")]
                                          (is (contains? (cached-endpoints) (gt/endpoint graphics-atlas :save-data)))
                                          (test-util/prop! graphics-atlas :margin 10)
                                          (is (not (contains? (cached-endpoints) (gt/endpoint graphics-atlas :save-data))))))

                                      (testing "Save data values are always evicted from the cache when it is explicitly cleared."
                                        (g/clear-system-cache!)
                                        (is (empty? (g/cache)))))

                                    (exit-event-loop!)))))))))))

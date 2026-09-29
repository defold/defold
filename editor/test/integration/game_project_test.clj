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

(ns integration.game-project-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :refer [spit-until-new-mtime with-clean-system]])
  (:import [java.io File]))

(def ^:dynamic ^String *project-path*)

(defn- create-test-project
  ([]
   (create-test-project "test/resources/reload_project"))
  ([project-path]
   (alter-var-root #'*project-path* (fn [_] (-> (fs/create-temp-directory! "foo")
                                                (.getAbsolutePath))))
   (fs/copy-directory! (io/file project-path) (io/file *project-path*))))

(defn- load-test-project [ws-graph]
  (let [workspace (test-util/setup-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- setup [ws-graph]
  (create-test-project)
  (load-test-project ws-graph))

(defn- file-in-project ^File [^String name] (io/file (io/file *project-path*) name))

(defn- write-file [^String name content]
  (let [f (file-in-project name)]
    (fs/create-file! f)
    (spit-until-new-mtime f content)))

(defn- copy-file [name new-name]
  (fs/copy-file! (file-in-project name) (file-in-project new-name)))

(defn- error? [type v]
  (and (g/error? v) (= type (get-in v [:user-data :type]))))

(defn- no-error? [v]
  (not (g/error? v)))

(defn- title [settings]
  (settings ["project" "title"]))

(defn- ensure-game-project-connections! [project game-project]
  (let [script-intelligence (g/valid-node-value project :script-intelligence)]
    (is (contains? (set (g/targets-of script-intelligence :build-errors))
                   [game-project :build-errors]))
    (is (contains? (set (g/sources-of project :display-profiles))
                   [game-project :display-profiles-data]))
    (is (contains? (set (g/sources-of project :texture-profiles))
                   [game-project :texture-profiles-data]))
    (is (contains? (set (g/sources-of project :use-font-layout))
                   [game-project :use-font-layout]))
    (is (contains? (set (g/sources-of project :settings))
                   [game-project :settings-map]))))

(deftest load-ok-project
  (with-clean-system
    (let [[_workspace project] (setup world)]
      (testing "Settings loaded"
        (let [settings (g/node-value project :settings)
              game-project (project/get-resource-node project "/game.project")]
          (is (= "Side-scroller" (title settings)))
          (ensure-game-project-connections! project game-project))))))

(deftest load-incomplete-project
  (testing "Missing ResourceNodes are shared among all references to it."
    (with-clean-system
      (create-test-project "test/resources/missing_project")
      ;; Create multiple references to the non-existent resources.
      (doseq [path ["missing_collection.collection" ; references "/non-existent.collection"
                    "missing_component.go"          ; references "/non-existent.script"
                    "missing_go.collection"]]       ; references "/non-existent.go"
        (copy-file path (str "duplicate_" path)))
      (let [project (second (log/without-logging (load-test-project world)))
            num-nodes-by-proj-path (frequencies (map resource/proj-path (test-util/project-node-resources project)))]
        (is (= 1 (num-nodes-by-proj-path "/non-existent.collection")))
        (is (= 1 (num-nodes-by-proj-path "/non-existent.script")))
        (is (= 1 (num-nodes-by-proj-path "/non-existent.go")))))))

(deftest load-broken-project
  (with-clean-system
    (create-test-project)
    (write-file "game.project" "bad content")
    (let [[workspace project] (log/without-logging (load-test-project world))
          game-project (project/get-resource-node project "/game.project")]
      (testing "Defaults if can't load"
        (let [settings (g/node-value project :settings)]
          (is (= "unnamed" (title settings)))))
      (testing "Game project node is defective"
        (let [gpn-settings-map (g/node-value game-project :settings-map)]
          (is (error? :invalid-content gpn-settings-map))))
      (testing "Connections"
        (ensure-game-project-connections! project game-project)))))

(deftest break-ok-project
  (with-clean-system
    (let [[workspace project] (setup world)]
      (copy-file "game.project" "game.project.backup")
      (testing "Settings loaded"
        (let [settings (g/node-value project :settings)]
          (is (= "Side-scroller" (title settings)))))
      (testing "Broken file gives defaults & defective node"
        (write-file "game.project" "bad content")
        (log/without-logging (workspace/resource-sync! workspace))
        (let [settings (g/node-value project :settings)
              gpn (project/get-resource-node project "/game.project")
              gpn-settings-map (g/node-value gpn :settings-map)]
          (is (= "unnamed" (title settings)))
          (is (error? :invalid-content gpn-settings-map))
          (ensure-game-project-connections! project gpn))
        (copy-file "game.project.backup" "game.project")
        (workspace/resource-sync! workspace))
      (testing "Restoring gives normal settings"
        (let [settings (g/node-value project :settings)
              gpn (project/get-resource-node project "/game.project")
              gpn-settings-map (g/node-value gpn :settings-map)]
          (is (= "Side-scroller" (title settings)))
          (is (no-error? gpn-settings-map))
          (ensure-game-project-connections! project gpn))))))

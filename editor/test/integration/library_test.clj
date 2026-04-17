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

(ns integration.library-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.game-project :as game-project]
            [editor.game-project-core :as game-project-core]
            [editor.gui :as gui]
            [editor.library :as library]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support :refer [spit-until-new-mtime with-clean-system]]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [com.dynamo.bob.util Library$Archive Library$Problem$Missing Library$Result]
           [java.net URI]
           [org.apache.commons.io FileUtils]))

(def ^:dynamic *project-path* "test/resources/empty_project")

(defn- setup-scratch
  ([ws-graph]
   (setup-scratch ws-graph *project-path*))
  ([ws-graph project-path]
   (let [workspace (test-util/setup-scratch-workspace! ws-graph project-path)
         project (test-util/setup-project! workspace)]
     [workspace project])))

(def ^:private uri-string "file:/scriptlib, file:/imagelib1, file:/imagelib2, file:/bogus")
(def ^:private uris (library/parse-uris uri-string))

(deftest uri-parsing
  (testing "sane uris"
    (is (= uris [(URI. "file:/scriptlib") (URI. "file:/imagelib1") (URI. "file:/imagelib2") (URI. "file:/bogus")]))
    (is (= uris (library/parse-uris "file:/scriptlib,file:/imagelib1,file:/imagelib2,file:/bogus"))))
  (testing "quoted values"
    (is (= uris (library/parse-uris "\"file:/scriptlib\",\"file:/imagelib1\",\"file:/imagelib2\",\"file:/bogus\"")))
    (is (= uris (library/parse-uris "\"file:/scriptlib\", \"file:/imagelib1\", \"file:/imagelib2\", \"file:/bogus\""))))
  (testing "various spacing and commas allowed"
    (is (= uris (library/parse-uris "   file:/scriptlib,   file:/imagelib1\t,file:/imagelib2,  file:/bogus\t")))
    (is (= uris (library/parse-uris " ,, file:/scriptlib ,  ,,  file:/imagelib1,\tfile:/imagelib2 ,,,,  file:/bogus\t,")))
    (is (= uris (library/parse-uris "\tfile:/scriptlib,\tfile:/imagelib1,\tfile:/imagelib2,\tfile:/bogus   ")))
    (is (= uris (library/parse-uris "\r\nfile:/scriptlib,\nfile:/imagelib1,\rfile:/imagelib2,\tfile:/bogus "))))
  (testing "ignore non-uris"
    (is (= uris (library/parse-uris "this, file:/scriptlib, sure, file:/imagelib1, aint, file:/imagelib2, no, file:/bogus, uri")))))

(deftest initial-state
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (testing "initially no library files"
        (let [files (test-support/library-files (workspace/project-directory workspace))]
          (is (= 0 (count files)))))
      (testing "initially missing library state"
        (let [states (library/cached (workspace/project-directory workspace) uris)]
          (is (coll/every? #(instance? Library$Problem$Missing (.problem ^Library$Result %)) states))
          (is (coll/not-any? Library$Result/.archive states)))))))

(deftest libraries-present
  (with-clean-system
    (let [[workspace _project] (log/without-logging (setup-scratch world))
          project-directory (workspace/project-directory workspace)]
      ;; copy to proper place
      (FileUtils/copyDirectory
        (io/file "test/resources/lib_resource_project/.internal/lib")
        (test-support/library-directory project-directory))
      (testing "libraries now present"
        (let [files (test-support/library-files project-directory)]
          (is (= 3 (count files)))))
      (testing "library-state visible"
        (let [state (library/cached project-directory uris)]
          (is (= 4 (count state)))
          (is (= 3 (count (keep Library$Result/.archive state))))))  ; no file for bogus
      (testing "ignore duplicate library uris"
        (let [state (library/cached project-directory (concat uris uris))]
          (is (= 4 (count state)))
          (is (= 3 (count (keep Library$Result/.archive state)))))))))

(defn- write-deps! [game-project deps]
  (let [settings (with-open [game-project-reader (io/reader game-project)]
                   (settings-core/parse-settings game-project-reader))]
    (spit-until-new-mtime game-project
                          (-> settings
                              (settings-core/set-setting ["project" "dependencies"] deps)
                              (settings-core/settings-with-value)
                              (settings-core/settings->str game-project-core/meta-settings :multi-line-list)))))

(deftest open-project
  (with-clean-system
    (test-util/with-ui-run-later-rebound
      (with-open [server (http-server/start! test-util/lib-server-handler)]
        (let [workspace (test-util/setup-scratch-workspace! world "test/resources/test_project")
              uri (test-util/lib-server-uri server "lib_resource_project")
              game-project-res (workspace/resolve-workspace-resource workspace "/game.project")]
          (write-deps! game-project-res uri)
          (let [extensions (extensions/make world)
                project (project/open-project! world extensions workspace game-project-res progress/null-render-progress!)
                ext-gui (test-util/resource-node project "/lib_resource_project/simple.gui")
                int-gui (test-util/resource-node project "/gui/empty.gui")]
            (is (some? ext-gui))
            (is (some? int-gui))
            (let [template-node (gui/add-gui-node! project int-gui (:node-id (test-util/outline int-gui [0])) :type-template 0 nil)]
              (g/set-property! template-node :template {:resource (workspace/resolve-workspace-resource workspace "/lib_resource_project/simple.gui")
                                                        :overrides {}}))
            (let [original (:node-id (test-util/outline ext-gui [0 0]))
                  or (:node-id (test-util/outline int-gui [0 0 0]))]
              (is (= [or] (g/overrides original))))))))))

(defn- fetch-libraries! [workspace library-uris render-fn]
  (->> (library/fetch! (workspace/project-directory workspace) library-uris render-fn)
       (workspace/set-project-dependencies! workspace))
  (workspace/resource-sync! workspace))

(deftest fetch-libraries
  (with-clean-system
    (with-open [server (http-server/start! test-util/lib-server-handler)]
      (let [[workspace project] (log/without-logging (setup-scratch world))
            uri (test-util/lib-server-uri server "lib_resource_project")
            game-project (project/get-resource-node project "/game.project")]
        ;; make sure we don't have library file to begin with
        (is (= 0 (count (project/find-resources project "lib_resource_project/simple.gui"))))
        ;; add dependency, fetch libraries, we should now have library file
        (game-project/set-setting! game-project ["project" "dependencies"] [uri])
        (fetch-libraries! workspace (project/project-dependencies project) identity)
        (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui"))))
        ;; remove dependency again, fetch libraries, we should no longer have the file
        (game-project/set-setting! game-project ["project" "dependencies"] nil)
        (fetch-libraries! workspace (project/project-dependencies project) identity)
        (is (= 0 (count (project/find-resources project "lib_resource_project/simple.gui"))))))))

(deftest fetch-libraries-from-library-archive-with-nesting
  (with-clean-system
    (with-open [server (http-server/start! test-util/lib-server-handler)]
      (let [[workspace project] (log/without-logging (setup-scratch world))
            uri (test-util/lib-server-uri server "lib_resource_project_with_nesting")
            game-project (project/get-resource-node project "/game.project")]
        ;; make sure we don't have library file to begin with
        (is (= 0 (count (project/find-resources project "lib_resource_project/simple.gui"))))
        ;; add dependency, fetch libraries, we should now have library file
        (game-project/set-setting! game-project ["project" "dependencies"] [uri])
        (fetch-libraries! workspace (project/project-dependencies project) identity)
        (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui"))))))))

(deftest fetch-libraries-from-local-extension-dir
  (with-clean-system
    (let [[workspace _project] (log/without-logging (setup-scratch world))
          project-directory (workspace/project-directory workspace)
          original-uri (URI/create "https://example.com/local-extension-dir.zip")
          property-prefix "defold.extension.test-local"
          local-extension-dir (io/file "test/resources/lib_resource_project")
          game-project-resource (workspace/resolve-workspace-resource workspace "/game.project")
          expected-library-file (test-support/library-file project-directory original-uri "")]
      (System/setProperty (str property-prefix ".url") (str original-uri))
      (System/setProperty (str property-prefix ".path") (.getCanonicalPath local-extension-dir))
      (try
        (write-deps! game-project-resource "{{defold.extension.test-local.url}}")
        (let [results (library/fetch! (workspace/project-directory workspace) (project/read-dependencies game-project-resource) progress/null-render-progress!)
              result ^Library$Result (first results)
              archive ^Library$Archive (.archive result)]
          (is (not= original-uri (.uri result)))
          (is (= "localhost" (.getHost ^URI (.uri result))))
          (is (not= (.toPath expected-library-file) (.path archive)))
          (is (not (.isFile expected-library-file)))
          (is (.isFile (.toFile (.path archive)))))
        (finally
          (System/clearProperty (str property-prefix ".url"))
          (System/clearProperty (str property-prefix ".path")))))))

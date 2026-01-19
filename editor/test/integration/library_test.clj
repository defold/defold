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
            [editor.editor-extensions :as extensions]
            [editor.defold-project :as project]
            [editor.game-project :as game-project]
            [editor.game-project-core :as game-project-core]
            [editor.gui :as gui]
            [editor.library :as library]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :refer [spit-until-new-mtime with-clean-system]]
            [util.http-server :as http-server])
  (:import [java.net URI]
           [org.apache.commons.io FileUtils]
           [org.apache.commons.codec.digest DigestUtils]))

(def ^:dynamic *project-path* "test/resources/empty_project")

(defn- setup-scratch
  ([ws-graph]
   (setup-scratch ws-graph *project-path*))
  ([ws-graph project-path]
   (let [workspace (test-util/setup-scratch-workspace! ws-graph project-path)
         project (test-util/setup-project! workspace)]
     [workspace project])))

(def ^:private uri-string "file:/scriptlib, file:/imagelib1, file:/imagelib2, file:/bogus")
(def ^:private uris (library/parse-library-uris uri-string))

(deftest uri-parsing
  (testing "sane uris"
    (is (= uris [(URI. "file:/scriptlib") (URI. "file:/imagelib1") (URI. "file:/imagelib2") (URI. "file:/bogus")]))
    (is (= uris (library/parse-library-uris "file:/scriptlib,file:/imagelib1,file:/imagelib2,file:/bogus"))))
  (testing "quoted values"
    (is (= uris (library/parse-library-uris "\"file:/scriptlib\",\"file:/imagelib1\",\"file:/imagelib2\",\"file:/bogus\"")))
    (is (= uris (library/parse-library-uris "\"file:/scriptlib\", \"file:/imagelib1\", \"file:/imagelib2\", \"file:/bogus\""))))
  (testing "various spacing and commas allowed"
    (is (= uris (library/parse-library-uris "   file:/scriptlib,   file:/imagelib1\t,file:/imagelib2,  file:/bogus\t")))
    (is (= uris (library/parse-library-uris " ,, file:/scriptlib ,  ,,  file:/imagelib1,\tfile:/imagelib2 ,,,,  file:/bogus\t,")))
    (is (= uris (library/parse-library-uris "\tfile:/scriptlib,\tfile:/imagelib1,\tfile:/imagelib2,\tfile:/bogus   ")))
    (is (= uris (library/parse-library-uris "\r\nfile:/scriptlib,\nfile:/imagelib1,\rfile:/imagelib2,\tfile:/bogus "))))
  (testing "ignore non-uris"
    (is (= uris (library/parse-library-uris "this, file:/scriptlib, sure, file:/imagelib1, aint, file:/imagelib2, no, file:/bogus, uri")))))

(deftest initial-state
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (testing "initially no library files"
        (let [files (library/library-files (workspace/project-directory workspace))]
          (is (= 0 (count files)))))
      (testing "initially unknown library state"
        (let [states (library/current-library-state
                      (workspace/project-directory workspace)
                      uris)]
          (is (every? (fn [state] (= (:status state) :unknown)) states))
          (is (not (seq (filter :file states)))))))))

(deftest libraries-present
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (let [project-directory (workspace/project-directory workspace)]
        ;; copy to proper place
        (FileUtils/copyDirectory
         (io/file "test/resources/lib_resource_project/.internal/lib")
         (library/library-directory project-directory))
        (testing "libraries now present"
          (let [files (library/library-files project-directory)]
            (is (= 3 (count files)))))
        (testing "library-state visible"
          (let [state (library/current-library-state project-directory uris)]
            (is (= 4 (count state)))
            (is (= 3 (count (filter :file state))))))  ; no file for bogus
        (testing "ignore duplicate library uris"
          (let [state (library/current-library-state project-directory (concat uris uris))]
            (is (= 4 (count state)))
            (is (= 3 (count (filter :file state))))))))))

(defn dummy-lib-resolver [uri tag]
  (let [file-name (str "lib_resource_project/.internal/lib/" (DigestUtils/sha1Hex (str uri)) "-.zip")]
    {:status :stale
     :stream (some-> file-name io/resource io/input-stream)
     :tag    "tag"
     :size   100}))

(deftest library-update
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (let [project-directory (workspace/project-directory workspace)]
        (let [update-states   (->> (library/current-library-state project-directory uris)
                                   (library/fetch-library-updates dummy-lib-resolver progress/null-render-progress!)
                                   (library/validate-updated-libraries)
                                   (library/install-validated-libraries! project-directory))
              update-statuses (group-by :status update-states)
              files           (library/library-files project-directory)
              states          (library/current-library-state project-directory uris)]
          (is (= 3 (count (update-statuses :up-to-date))))
          (is (= 1 (count (update-statuses :error))))
          (is (= (str (:uri (first (update-statuses :error)))) "file:/bogus"))
          (is (every? (fn [state] (= (:tag state) "tag")) (update-statuses :up-to-date)))
          (is (= 3 (count files)))
          (is (= 4 (count states)))
          (is (every? (fn [state] (= (:tag state) "tag")) (filter :file states))))))))

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

(defn- fetch-validate-install-libraries! [workspace library-uris render-fn]
  (when (workspace/dependencies-reachable? library-uris)
    (->> (workspace/fetch-and-validate-libraries workspace library-uris render-fn)
         (workspace/install-validated-libraries! workspace))
    (workspace/resource-sync! workspace)))

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
        (fetch-validate-install-libraries! workspace (project/project-dependencies project) identity)
        (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui"))))
        ;; remove dependency again, fetch libraries, we should no longer have the file
        (game-project/set-setting! game-project ["project" "dependencies"] nil)
        (fetch-validate-install-libraries! workspace (project/project-dependencies project) identity)
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
        (fetch-validate-install-libraries! workspace (project/project-dependencies project) identity)
        (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui"))))))))

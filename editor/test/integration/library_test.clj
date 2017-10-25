(ns integration.library-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system spit-until-new-mtime]]
            [editor.library :as library]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.game-project :as game-project]
            [editor.gui :as gui]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [java.net URL]
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

(def ^:private url-string "file:/scriptlib file:/imagelib1 file:/imagelib2 file:/bogus")
(def ^:private urls (library/parse-library-urls url-string))

(deftest url-parsing
  (testing "sane urls"
    (is (= urls (list (URL. "file:/scriptlib") (URL. "file:/imagelib1") (URL. "file:/imagelib2") (URL. "file:/bogus"))))
    (is (= (library/parse-library-urls "file:/scriptlib,file:/imagelib1,file:/imagelib2,file:/bogus")
           (list (URL. "file:/scriptlib") (URL. "file:/imagelib1") (URL. "file:/imagelib2") (URL. "file:/bogus")))))
  (testing "various spacing and commas allowed"
    (is (= urls (library/parse-library-urls "   file:/scriptlib   file:/imagelib1\tfile:/imagelib2  file:/bogus\t")))
    (is (= urls (library/parse-library-urls " ,, file:/scriptlib ,  ,,  file:/imagelib1\tfile:/imagelib2 ,,,,  file:/bogus\t,")))
    (is (= urls (library/parse-library-urls "\tfile:/scriptlib\tfile:/imagelib1\tfile:/imagelib2\tfile:/bogus   ")))
    (is (= urls (library/parse-library-urls "\r\nfile:/scriptlib\nfile:/imagelib1\rfile:/imagelib2\tfile:/bogus "))))
  (testing "ignore non-urls"
    (is (= urls (library/parse-library-urls "this file:/scriptlib sure file:/imagelib1 aint file:/imagelib2 no file:/bogus url")))))

(deftest initial-state
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (testing "initially no library files"
        (let [files (library/library-files (workspace/project-path workspace))]
          (is (= 0 (count files)))))
      (testing "initially unknown library state"
        (let [states (library/current-library-state
                      (workspace/project-path workspace)
                      urls)]
          (is (every? (fn [state] (= (:status state) :unknown)) states))
          (is (not (seq (filter :file states)))))))))

(deftest libraries-present
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (let [project-directory (workspace/project-path workspace)]
        ;; copy to proper place
        (FileUtils/copyDirectory
         (io/file "test/resources/lib_resource_project/.internal/lib")
         (library/library-directory project-directory))
        (testing "libraries now present"
          (let [files (library/library-files project-directory)]
            (is (= 3 (count files)))))
        (testing "library-state visible"
          (let [state (library/current-library-state project-directory urls)]
            (is (= 4 (count state)))
            (is (= 3 (count (filter :file state))))))  ; no file for bogus
        (testing "ignore duplicate library urls"
          (let [state (library/current-library-state project-directory (concat urls urls))]
            (is (= 4 (count state)))
            (is (= 3 (count (filter :file state))))))))))

(defn dummy-lib-resolver [url tag]
  (let [file-name (str "lib_resource_project/.internal/lib/" (DigestUtils/sha1Hex (str url)) "-.zip")]
    {:status :stale
     :stream (some-> file-name io/resource io/input-stream)
     :tag    "tag"
     :size   100}))

(deftest library-update
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (let [project-directory (workspace/project-path workspace)]
        (let [update-states   (library/update-libraries! project-directory urls dummy-lib-resolver)
              update-statuses (group-by :status update-states)
              files           (library/library-files project-directory)
              states          (library/current-library-state project-directory urls)]
          (is (= 3 (count (update-statuses :up-to-date))))
          (is (= 1 (count (update-statuses :error))))
          (is (= (str (:url (first (update-statuses :error)))) "file:/bogus"))
          (is (every? (fn [state] (= (:tag state) "tag")) (update-statuses :up-to-date)))
          (is (= 3 (count files)))
          (is (= 4 (count states)))
          (is (every? (fn [state] (= (:tag state) "tag")) (filter :file states))))))))

(defn- write-deps! [game-project deps]
  (let [settings (with-open [game-project-reader (io/reader game-project)]
                   (settings-core/parse-settings game-project-reader))]
    (spit-until-new-mtime game-project (-> settings
                                         (settings-core/set-setting ["project" "dependencies"] deps)
                                         settings-core/settings-with-value
                                         settings-core/settings->str))))

(deftest open-project
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/test_project")
          ^File project-directory (workspace/project-path workspace)
          server (test-util/->lib-server)
          url (test-util/lib-server-url server "lib_resource_project")
          game-project-res (workspace/resolve-workspace-resource workspace "/game.project")]
      (write-deps! game-project-res url)
      (let [project (project/open-project! world workspace game-project-res progress/null-render-progress! nil)
            ext-gui (test-util/resource-node project "/lib_resource_project/simple.gui")
            int-gui (test-util/resource-node project "/gui/empty.gui")]
        (is (some? ext-gui))
        (is (some? int-gui))
        (let [template-node (gui/add-gui-node! project int-gui (:node-id (test-util/outline int-gui [0])) :type-template nil)]
          (g/set-property! template-node :template {:resource (workspace/resolve-workspace-resource workspace "/lib_resource_project/simple.gui")
                                                    :overrides {}}))
        (let [original (:node-id (test-util/outline ext-gui [0 0]))
              or (:node-id (test-util/outline int-gui [0 0 0]))]
          (is (= [or] (g/overrides original)))))
      (test-util/kill-lib-server server))))

(deftest fetch-libraries
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))
          server (test-util/->lib-server)
          url (test-util/lib-server-url server "lib_resource_project")
          game-project (project/get-resource-node project "/game.project")]
      ;; make sure we don't have library file to begin with
      (is (= 0 (count (project/find-resources project "lib_resource_project/simple.gui"))))
      ;; add dependency, fetch libraries, we should now have library file
      (game-project/set-setting! game-project ["project" "dependencies"] url)
      (workspace/fetch-libraries! workspace (project/project-dependencies project) identity (constantly true))
      (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui")))))))

(deftest fetch-libraries-from-library-archive-with-nesting
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))
          server (test-util/->lib-server)
          url (test-util/lib-server-url server "lib_resource_project_with_nesting")
          game-project (project/get-resource-node project "/game.project")]
      ;; make sure we don't have library file to begin with
      (is (= 0 (count (project/find-resources project "lib_resource_project/simple.gui"))))

      ;; add dependency, fetch libraries, we should now have library file
      (game-project/set-setting! game-project ["project" "dependencies"] url)
      (workspace/fetch-libraries! workspace (project/project-dependencies project) identity (constantly true))
      (is (= 1 (count (project/find-resources project "lib_resource_project/simple.gui")))))))

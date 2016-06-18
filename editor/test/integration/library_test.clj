(ns integration.library-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.library :as library]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.progress :as progress]
            [editor.game-project-core :as gpc]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [java.net URL]
           [org.apache.commons.io FileUtils]))

(def ^:dynamic *project-path* "resources/empty_project")

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
         (io/file "resources/lib_resource_project/.internal/lib")
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
  (let [file-name (str "lib_resource_project/.internal/lib/file__" (subs (.getPath url) 1) "-.zip")]
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
  (let [settings (-> (slurp game-project)
                   gpc/string-reader
                   gpc/parse-settings)]
    (spit game-project (->
                         settings
                         (gpc/set-setting {} ["project" "dependencies"] deps)
                         gpc/settings-with-value
                         gpc/settings->str))))

#_(defn- read-dependencies [game-project-resource]
   (-> (slurp game-project-resource)
     gpc/string-reader
     gpc/parse-settings
     (gpc/get-setting ["project" "dependencies"])))

(deftest open-project
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "resources/test_project")
          ^File project-directory (workspace/project-path workspace)
          server (test-util/->lib-server)
          url (test-util/lib-server-url server "lib_resource_project")
          game-project-res (workspace/resolve-workspace-resource workspace "/game.project")]
      (write-deps! game-project-res url)
      (let [project (project/open-project! world workspace game-project-res progress/null-render-progress! nil)
            ext-gui (test-util/resource-node project "/lib_resource_project/simple.gui")
            int-gui (test-util/resource-node project "/gui/external_template.gui")]
        (is (some? ext-gui))
        (is (some? int-gui))
        (let [original (:node-id (test-util/outline ext-gui [0 0]))
              or (:node-id (test-util/outline int-gui [0 0 0]))]
          (is (= [or] (g/overrides original)))))
      #_(let [project (project/make-project world workspace)
                project (project/load-project project (g/node-value project :resources) progress/null-render-progress!)
                ext-gui (test-util/resource-node project "/lib_resource_project/simple.gui")
                int-gui (test-util/resource-node project "/gui/external_template.gui")]
            (prn ext-gui int-gui))
      #_(let [progress (atom (progress/make "Updating dependencies" 3))
             render-progress! progress/null-render-progress!
             game-project-resource game-project-res
             login-fn nil
             graph world]
         (render-progress! @progress)
         (workspace/set-project-dependencies! workspace (read-dependencies game-project-resource))
         (workspace/update-dependencies! workspace (progress/nest-render-progress render-progress! @progress) login-fn)
         (render-progress! (swap! progress progress/advance 1 "Syncing resources"))
         (workspace/resource-sync! workspace false [] (progress/nest-render-progress render-progress! @progress))
         (render-progress! (swap! progress progress/advance 1 "Loading project"))
         (let [project (project/make-project graph workspace)]
           (project/load-project project (g/node-value project :resources) (progress/nest-render-progress render-progress! @progress))))
      #_(do
         (workspace/set-project-dependencies! workspace url)
         (let [deps (workspace/update-dependencies! workspace progress/null-render-progress! nil)
               dep (->> deps
                     (filter #(= (URL. url) (:url %)))
                     first)]
           (is (= :up-to-date (:status dep)))
           (is (.exists ^File (:file dep))))
         (workspace/resource-sync! workspace false [] progress/null-render-progress!)
         (let [project (project/make-project world workspace)
               project (project/load-project project (g/node-value project :resources) progress/null-render-progress!)
               ext-gui (test-util/resource-node project "/lib_resource_project/simple.gui")
               int-gui (test-util/resource-node project "/gui/external_template.gui")]
           (prn ext-gui int-gui)))
      (test-util/kill-lib-server server))))

(open-project)

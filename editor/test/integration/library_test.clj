(ns integration.library-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [support.test-support :refer [with-clean-system]]
            [editor.library :as library]
            [editor.project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [java.net URL]
           [org.apache.commons.io FileUtils]))

(def ^:dynamic *project-path* "resources/empty_project")

(defn- setup-scratch [ws-graph]
  (let [workspace (test-util/setup-scratch-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

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
     :stream (io/input-stream (io/resource file-name))
     :tag "tag"
     }))

(deftest library-update
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (let [project-directory (workspace/project-path workspace)]
        (let [update-states (library/update-libraries! project-directory dummy-lib-resolver urls)
              update-statuses (group-by :status update-states)
              files (library/library-files project-directory)
              states (library/current-library-state project-directory urls)]
          (is (= 3 (count (update-statuses :up-to-date))))
          (is (= 1 (count (update-statuses :error))))
          (is (= (str (:url (first (update-statuses :error)))) "file:/bogus"))
          (is (every? (fn [state] (= (:tag state) "tag")) (update-statuses :up-to-date)))
          (is (= 3 (count files)))
          (is (= 4 (count states)))
          (is (every? (fn [state] (= (:tag state) "tag")) (filter :file states))))))))








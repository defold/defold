(ns integration.resource-watch-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [support.test-support :refer [with-clean-system]]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.library :as library]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [java.net URL]
           [org.apache.commons.io IOUtils]))

(def ^:dynamic *project-path* "test/resources/lib_resource_project")

(def ^:private lib-urls (library/parse-library-urls "file:/scriptlib file:/imagelib1 file:/imagelib2 file:/bogus"))

(def ^:private scriptlib-url (first lib-urls))
(def ^:private imagelib1-url (nth lib-urls 1))
(def ^:private imagelib2-url (nth lib-urls 2))
(def ^:private bogus-url (nth lib-urls 3))

(defn- setup [ws-graph]
  (let [workspace (test-util/setup-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- setup-scratch [ws-graph]
  (let [workspace (test-util/setup-scratch-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- non-builtin-resources [resources]
  (filter #(not (.startsWith (resource/proj-path %) "/builtins")) resources))

(def ^:private directory-resources #{"/" "/game.project" "/main" "/main/main.collection" "/lib_resource_project" "/lib_resource_project/simple.gui"})
(def ^:private scriptlib-resources #{"/scripts" "/scripts/main.script"})
(def ^:private imagelib1-resources #{"/images" "/images/paddle.png" "/images/pow.png"})
(def ^:private imagelib2-resources #{"/images" "/images/ball.png" "/images/pow.png"})

(defn- resource-paths [resources]
  (set (map resource/proj-path resources)))

(defn- workspace-resource-paths [workspace]
  (resource-paths (non-builtin-resources (g/node-value workspace :resource-list))))

(deftest include-parsing
  (testing "sane dirs"
    (is (not (seq (resource-watch/parse-include-dirs ""))))
    (is (= (resource-watch/parse-include-dirs "foo bar baz") (list "foo" "bar" "baz")))
    (is (= (resource-watch/parse-include-dirs "foo,bar,baz") (list "foo" "bar" "baz"))))
  (testing "various spacing and commas allowed"
    (is (= (resource-watch/parse-include-dirs "  foo  bar  baz  ") (list "foo" "bar" "baz")))
    (is (= (resource-watch/parse-include-dirs "  ,, foo , bar ,,, baz  ") (list "foo" "bar" "baz")))
    (is (= (resource-watch/parse-include-dirs "\tfoo\t\tbar\tbaz") (list "foo" "bar" "baz")))
    (is (= (resource-watch/parse-include-dirs "\r\nfoo\rbar\nbaz") (list "foo" "bar" "baz"))))
  (testing "cant specify directory names with spaces :("
    (is (= (resource-watch/parse-include-dirs "im\\ possible") (list "im\\" "possible")))))

(deftest libraries-skipped-by-default
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (is (= (workspace-resource-paths workspace) directory-resources)))))

(deftest only-load-specified-libraries
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib1-resources)))
      (workspace/set-project-dependencies! workspace (str imagelib2-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib2-resources)))
      (workspace/set-project-dependencies! workspace (str scriptlib-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources scriptlib-resources)))
      (workspace/set-project-dependencies! workspace (str imagelib1-url " " scriptlib-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib1-resources scriptlib-resources))))))

(deftest skip-colliding-libraries
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url " " imagelib2-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace)
             (clojure.set/union directory-resources imagelib1-resources))))))

(deftest skip-bad-lib-urls []
  (with-clean-system
    (let [[workspace projec†] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url " " bogus-url))
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace)
             (clojure.set/union directory-resources imagelib1-resources))))))

(deftest resource-sync!-diff
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url))
      (let [il1-diff (workspace/resource-sync! workspace)]
        (is (= (resource-paths (:added il1-diff)) imagelib1-resources))
        (is (empty? (:removed il1-diff)))
        (is (empty? (:changed il1-diff))))
      (workspace/set-project-dependencies! workspace "")
      (let [remove-il1-diff (workspace/resource-sync! workspace)]
        (is (empty? (:added remove-il1-diff)))
        (is (= (resource-paths (:removed remove-il1-diff)) imagelib1-resources))
        (is (empty? (:changed remove-il1-diff))))
      (workspace/set-project-dependencies! workspace (str imagelib1-url))
      (workspace/resource-sync! workspace)
      (let [project-directory (workspace/project-path workspace)]
        ;; this fakes having downloaded a different version of the library
        (fs/move-file! (library/library-file project-directory imagelib1-url "")
                       (library/library-file project-directory imagelib1-url "updated")))
      (let [update-il1-diff (workspace/resource-sync! workspace)]
        (is (empty? (:added update-il1-diff)))
        (is (empty? (:removed update-il1-diff)))
        (is (= (resource-paths (:changed update-il1-diff)) imagelib1-resources))))))

(defn- read-bytes [resource]
  (with-open [is (io/input-stream resource)]
    (vec (IOUtils/toByteArray is))))

(deftest exchange-of-zipresource-updates-corresponding-resource-node
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url))
      (workspace/resource-sync! workspace)
      (let [lib1-pow (project/get-resource-node project "/images/pow.png")
            lib1-pow-resource (g/node-value lib1-pow :resource)]
        (is (not (g/error? lib1-pow-resource)))
        (workspace/set-project-dependencies! workspace (str imagelib2-url))
        (workspace/resource-sync! workspace)
        (let [lib2-pow (project/get-resource-node project "/images/pow.png")
              lib2-pow-resource (g/node-value lib2-pow :resource)]
          (is (= lib1-pow lib2-pow))
          (is (not= lib1-pow-resource lib2-pow-resource))
          (is (not= (read-bytes lib1-pow-resource) (read-bytes lib2-pow-resource))))))))

(deftest delete-of-zipresource-marks-corresponding-resource-node-defective
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace (str imagelib1-url))
      (workspace/resource-sync! workspace)
      (let [lib1-paddle (project/get-resource-node project "/images/paddle.png")]
        (is (not (g/error? (g/node-value lib1-paddle :content))))
        (workspace/set-project-dependencies! workspace (str imagelib2-url))
        (workspace/resource-sync! workspace)
        (is (g/error? (g/node-value lib1-paddle :content))))))) ; removed, should emit errors

(deftest project-with-reserved-directories-can-still-be-loaded
  (binding [*project-path* "test/resources/reserved_files_project"]
    (with-clean-system
      (let [[workspace project] (log/without-logging (setup world))]
        (is (not (= nil (project/get-resource-node project "/present.script"))))
        (is (= nil (project/get-resource-node project "/.internal/hidden_internal.script")))
        (is (= nil (project/get-resource-node project "/builtins/hidden_builtins.script")))
        (is (= nil (project/get-resource-node project "/build")))))))

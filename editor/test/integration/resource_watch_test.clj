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
  (:import [org.apache.commons.io IOUtils]))

(def ^:dynamic *project-path* "test/resources/lib_resource_project")

(def ^:private lib-uris (library/parse-library-uris "file:/scriptlib, file:/imagelib1, file:/imagelib2, file:/bogus"))

(def ^:private scriptlib-uri (first lib-uris))
(def ^:private imagelib1-uri (nth lib-uris 1))
(def ^:private imagelib2-uri (nth lib-uris 2))
(def ^:private bogus-uri (nth lib-uris 3))

(defn- setup [ws-graph]
  (let [workspace (test-util/setup-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- setup-scratch [ws-graph]
  (let [workspace (test-util/setup-scratch-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- internal-resource?
  [resource]
  (let [proj-path (resource/proj-path resource)]
    (or (.startsWith proj-path "/builtins")
        (.startsWith proj-path "/_defold"))))

(def ^:private directory-resources #{"/" "/game.project" "/main" "/main/main.collection" "/lib_resource_project" "/lib_resource_project/simple.gui"})
(def ^:private scriptlib-resources #{"/scripts" "/scripts/main.script"})
(def ^:private imagelib1-resources #{"/images" "/images/paddle.png" "/images/pow.png"})
(def ^:private imagelib2-resources #{"/images" "/images/ball.png" "/images/pow.png"})

(defn- resource-paths [resources]
  (set (map resource/proj-path resources)))

(defn- workspace-resource-paths [workspace]
  (resource-paths (->> (g/node-value workspace :resource-list)
                       (remove internal-resource?))))

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
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib1-resources)))
      (workspace/set-project-dependencies! workspace [{:uri imagelib2-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib2-resources)))
      (workspace/set-project-dependencies! workspace [{:uri scriptlib-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources scriptlib-resources)))
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri} {:uri scriptlib-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace) (set/union directory-resources imagelib1-resources scriptlib-resources))))))

(deftest skip-colliding-libraries
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri} {:uri imagelib2-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace)
             (clojure.set/union directory-resources imagelib1-resources))))))

(deftest skip-bad-lib-uris []
  (with-clean-system
    (let [[workspace projec†] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri} {:uri bogus-uri}])
      (workspace/resource-sync! workspace)
      (is (= (workspace-resource-paths workspace)
             (clojure.set/union directory-resources imagelib1-resources))))))

(deftest resource-sync!-diff
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world))]
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri}])
      (let [il1-diff (workspace/resource-sync! workspace)]
        (is (= (resource-paths (:added il1-diff)) imagelib1-resources))
        (is (empty? (:removed il1-diff)))
        (is (empty? (:changed il1-diff))))
      (workspace/set-project-dependencies! workspace [])
      (let [remove-il1-diff (workspace/resource-sync! workspace)]
        (is (empty? (:added remove-il1-diff)))
        (is (= (resource-paths (:removed remove-il1-diff)) imagelib1-resources))
        (is (empty? (:changed remove-il1-diff))))
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri}])
      (workspace/resource-sync! workspace)
      (let [project-directory (workspace/project-path workspace)]
        ;; this fakes having downloaded a different version of the library
        (fs/move-file! (library/library-file project-directory imagelib1-uri "")
                       (library/library-file project-directory imagelib1-uri "updated")))
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
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri}])
      (workspace/resource-sync! workspace)
      (let [lib1-pow (project/get-resource-node project "/images/pow.png")
            lib1-pow-resource (g/node-value lib1-pow :resource)]
        (is (not (g/error? lib1-pow-resource)))
        (workspace/set-project-dependencies! workspace [{:uri imagelib2-uri}])
        (workspace/resource-sync! workspace)
        (let [lib2-pow (project/get-resource-node project "/images/pow.png")
              lib2-pow-resource (g/node-value lib2-pow :resource)]
          (is (= lib1-pow lib2-pow))
          (is (not= lib1-pow-resource lib2-pow-resource))
          (is (not= (read-bytes lib1-pow-resource) (read-bytes lib2-pow-resource))))))))

(deftest delete-of-zipresource-marks-corresponding-resource-node-defective
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world))]
      (workspace/set-project-dependencies! workspace [{:uri imagelib1-uri}])
      (workspace/resource-sync! workspace)
      (let [lib1-paddle (project/get-resource-node project "/images/paddle.png")]
        (is (not (g/error? (g/node-value lib1-paddle :content))))
        (workspace/set-project-dependencies! workspace [{:uri imagelib2-uri}])
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

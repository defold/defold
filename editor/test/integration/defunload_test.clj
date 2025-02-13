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

(ns integration.defunload-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(defn- loaded-resource-node-id? [node-id]
  (let [resource (resource-node/resource node-id)
        workspace (resource/workspace resource)
        loaded-resource-node-ids (workspace/loaded-resource-node-ids workspace)]
    (contains? loaded-resource-node-ids node-id)))

(defn- loaded-proj-path? [project proj-path]
  (let [resource-node-id (project/get-resource-node project proj-path)]
    (loaded-resource-node-id? resource-node-id)))

(deftest defunload-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path ["/non-editable"])
      (test-util/write-defunload-patterns! project-path ["/editable/unloaded"
                                                         "/non-editable/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          (fs/copy! (io/file "test/resources/images/small.png")
                    (io/file project-path "editable/unloaded.png"))

          (test-util/write-file-resource! workspace
            "/editable/unloaded.tilesource"
            {:image "/editable/unloaded.png"
             :tile-width 8
             :tile-height 8})

          (test-util/write-file-resource! workspace
            "/editable/unloaded.tilemap"
            {:tile-set "/editable/unloaded.tilesource"})

          (test-util/write-file-resource! workspace
            "/editable/unloaded.go"
            {:components
             [{:id "unloaded-tilemap"
               :component "/editable/unloaded.tilemap"}]})

          (test-util/write-file-resource! workspace
            "/editable/unloaded.collection"
            {:name "unloaded-collection"
             :instances
             [{:id "unloaded-game-object-instance"
               :prototype "/editable/unloaded.go"}]})

          (test-util/write-file-resource! workspace
            "/editable/loaded-referencing-unloaded-collection.collection"
            {:name "loaded-referencing-unloaded-collection"
             :collection-instances
             [{:id "unloaded-collection-instance"
               :collection "/editable/unloaded.collection"}]})

          (workspace/resource-sync! workspace)

          (let [project (test-util/setup-project! workspace)
                loaded-proj-path? #(loaded-proj-path? project %)]

            (testing "Matching proj-paths are excluded from loading."
              (is (not (loaded-proj-path? "/editable/unloaded.png")))
              (is (not (loaded-proj-path? "/editable/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/editable/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/editable/unloaded.go")))
              (is (not (loaded-proj-path? "/editable/unloaded.collection")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-collection.collection")))

            ;; Add a loaded file resource that unsafely depends on an unloaded
            ;; resource. The unloaded resource does not :allow-unloaded-use, so
            ;; we must load it and all its dependencies that also do not
            ;; :allow-unloaded-use.
            (test-util/write-file-resource! workspace
              "/editable/loaded-referencing-unloaded-tilesource.tilemap"
              {:tile-set "/editable/unloaded.tilesource"})

            (let [calls (binding [project/report-defunload-issues! (fn/make-call-logger)]
                          (workspace/resource-sync! workspace)
                          (fn/call-logger-calls project/report-defunload-issues!))]

              (testing "Unsafe dependencies on unloaded files are reported."
                (let [[unsafe-dependency-proj-paths-by-referencing-proj-path loaded-supplemental-proj-paths] (first calls)]
                  (is (= 1 (count calls)))
                  (is (= {"/editable/loaded-referencing-unloaded-tilesource.tilemap"
                          #{"/editable/unloaded.tilesource"}}
                         unsafe-dependency-proj-paths-by-referencing-proj-path))
                  (is (= #{"/editable/unloaded.png"
                           "/editable/unloaded.tilesource"}
                         loaded-supplemental-proj-paths)))))

            (testing "Added file resource referencing unloaded unsafe proj-path loads transitive dependencies."
              (is (loaded-proj-path? "/editable/unloaded.png"))
              (is (loaded-proj-path? "/editable/unloaded.tilesource"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilesource.tilemap"))
              (is (not (loaded-proj-path? "/editable/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/editable/unloaded.go")))
              (is (not (loaded-proj-path? "/editable/unloaded.collection")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-collection.collection")))

            ;; Add a loaded file resource that safely depends on an unloaded
            ;; resource. The unloaded resource does :allow-unloaded-use, so we
            ;; should not load it as a result.
            (test-util/write-file-resource! workspace
              "/editable/loaded-referencing-unloaded-go.collection"
              {:name "loaded-referencing-unloaded-go"
               :instances
               [{:id "unloaded-game-object-instance"
                 :prototype "/editable/unloaded.go"}]})

            (let [calls (binding [project/report-defunload-issues! (fn/make-call-logger)]
                          (workspace/resource-sync! workspace)
                          (fn/call-logger-calls project/report-defunload-issues!))]
              (is (= 0 (count calls))
                  "Safe dependencies on unloaded files are not reported."))

            (testing "Added file resource referencing unloaded safe proj-path does not load transitive dependencies."
              (is (loaded-proj-path? "/editable/unloaded.png"))
              (is (loaded-proj-path? "/editable/unloaded.tilesource"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilesource.tilemap"))
              (is (not (loaded-proj-path? "/editable/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/editable/unloaded.go")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-go.collection"))
              (is (not (loaded-proj-path? "/editable/unloaded.collection")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-collection.collection")))

            ;; Add another loaded file resource that unsafely depends on an
            ;; unloaded resource that itself unsafely depends on a defunloaded
            ;; resource that has already been loaded. Check that the already
            ;; loaded resources are not loaded twice as a result.
            (test-util/write-file-resource! workspace
              "/editable/loaded-referencing-unloaded-tilemap.go"
              {:components
               [{:id "unloaded-tilemap"
                 :component "/editable/unloaded.tilemap"}]})

            (let [calls (binding [project/report-defunload-issues! (fn/make-call-logger)]
                          (workspace/resource-sync! workspace)
                          (fn/call-logger-calls project/report-defunload-issues!))]

              (testing "Unsafe dependencies on still-unloaded files are reported."
                (let [[unsafe-dependency-proj-paths-by-referencing-proj-path loaded-supplemental-proj-paths] (first calls)]
                  (is (= 1 (count calls)))
                  (is (= {"/editable/loaded-referencing-unloaded-tilemap.go"
                          #{"/editable/unloaded.tilemap"}}
                         unsafe-dependency-proj-paths-by-referencing-proj-path))
                  (is (= #{"/editable/unloaded.tilemap"}
                         loaded-supplemental-proj-paths)))))

            (testing "Added file resource referencing unloaded unsafe proj-path loads the single still-unloaded transitive dependency."
              (is (loaded-proj-path? "/editable/unloaded.png"))
              (is (loaded-proj-path? "/editable/unloaded.tilesource"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilesource.tilemap"))
              (is (loaded-proj-path? "/editable/unloaded.tilemap"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilemap.go"))
              (is (not (loaded-proj-path? "/editable/unloaded.go")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-go.collection"))
              (is (not (loaded-proj-path? "/editable/unloaded.collection")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-collection.collection")))))))))

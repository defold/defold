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

(ns integration.unload-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(defn- resource-type-has-view-type? [resource-type view-type]
  {:pre [(map? resource-type)
         (keyword? view-type)]}
  (boolean
    (coll/some
      #(= view-type (:id %))
      (:view-types resource-type))))

(deftest unloaded-resource-nodes-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        editable-directory-proj-path (str "/" (name :editable))
        non-editable-directory-proj-path (str "/" (name :non-editable))

        resource-type->proj-path
        (fn resource-type->proj-path [resource-type]
          (format "%s/unloaded.%s"
                  (if (:editable resource-type)
                    editable-directory-proj-path
                    non-editable-directory-proj-path)
                  (:ext resource-type)))]

    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path [non-editable-directory-proj-path])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          ;; Add dependencies to all sanctioned extensions to game.project.
          (test-util/set-libraries! workspace test-util/sanctioned-extension-urls)

          ;; With the extensions added, we can populate the workspace.
          (let [excluded-ext? #{resource/placeholder-resource-type-ext "project"}

                loadable-resource-type-colls-by-editability
                (test-util/distinct-resource-types-by-editability
                  workspace
                  (fn resource-type-predicate [resource-type]
                    (and (:load-fn resource-type)
                         (not (excluded-ext? (:ext resource-type))))))

                loadable-resource-types-by-proj-paths
                (coll/transfer loadable-resource-type-colls-by-editability (sorted-map)
                  (mapcat val)
                  (map (juxt resource-type->proj-path identity)))

                loadable-resource-proj-paths (keys loadable-resource-types-by-proj-paths)]

            ;; Create files for all resource types.
            (doseq [[proj-path resource-type] loadable-resource-types-by-proj-paths]
              (let [absolute-path (str project-path proj-path)
                    file (io/file absolute-path)
                    contents (workspace/template workspace resource-type)]
                (fs/create-file! file contents)))

            ;; Add a .defunload file to exclude these files from loading.
            (let [defunload-file (io/file (str project-path "/.defunload"))
                  defunload-contents (string/join "\n" loadable-resource-proj-paths)]
              (fs/create-file! defunload-file defunload-contents))

            ;; With the workspace populated, we can load the project.
            (workspace/resource-sync! workspace)

            (let [project (test-util/setup-project! workspace)]
              (test-util/clear-cached-save-data! project)
              (g/with-auto-evaluation-context evaluation-context
                (let [basis (:basis evaluation-context)]
                  (doseq [proj-path loadable-resource-proj-paths]
                    (let [resource (workspace/find-resource workspace proj-path evaluation-context)
                          resource-node (project/get-resource-node project resource evaluation-context)
                          resource-type (resource/resource-type resource)]
                      (testing proj-path
                        (is (not (resource/loaded? resource)))
                        (is (= resource (resource-node/resource basis resource-node)))
                        (is (not (g/connected? basis resource-node :save-data project :save-data)))
                        (is (not (g/error? (g/node-value resource-node :_properties evaluation-context))))
                        (if (:editor-openable resource-type)
                          (is (not (resource/openable? resource)))
                          (is (resource/openable? resource)))
                        (when (:allow-unloaded-use resource-type)
                          (is (not (g/error? (g/node-value resource-node :node-outline evaluation-context))))
                          (is (not (g/error? (g/node-value resource-node :build-targets evaluation-context))))
                          (when (resource-type-has-view-type? resource-type :scene)
                            (is (not (g/error? (g/node-value resource-node :scene evaluation-context))))))))))))))))))

(defn- loaded-proj-path? [project proj-path]
  (let [node-id (project/get-resource-node project proj-path)]
    (resource-node/loaded? node-id)))

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
            {:tile-set "/editable/unloaded.tilesource"
             :blend-mode :blend-mode-add})

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
                loaded-proj-path? #(loaded-proj-path? project %)

                call-logged-resource-sync!
                (fn call-logged-resource-sync! []
                  (binding [project/report-defunload-issues! (fn/make-call-logger)
                            project/load-nodes! (fn/make-call-logger project/load-nodes!)]
                    (workspace/resource-sync! workspace)
                    [(fn/call-logger-calls project/report-defunload-issues!)
                     (fn/call-logger-calls project/load-nodes!)]))]

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

            (testing "Added file resource referencing unloaded unsafe proj-path loads transitive dependencies."
              (let [[report-defunload-issues!-calls load-nodes!-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies on unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/editable/loaded-referencing-unloaded-tilesource.tilemap"
                            #{"/editable/unloaded.tilesource"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/editable/unloaded.png"
                             "/editable/unloaded.tilesource"}
                           loaded-undesired-proj-paths))))

                (testing "The added file and its transitive dependencies were loaded."
                  (is (= 1 (count load-nodes!-calls)))
                  (let [[_ _ loaded-node-load-infos] (first load-nodes!-calls)]
                    (is (= #{"/editable/unloaded.png"
                             "/editable/unloaded.tilesource"
                             "/editable/loaded-referencing-unloaded-tilesource.tilemap"}
                           (coll/transfer loaded-node-load-infos (sorted-set)
                             (map (fn [{:keys [resource] :as _node-load-info}]
                                    (resource/proj-path resource)))))))))

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

            (testing "Added file resource referencing unloaded safe proj-path does not load transitive dependencies."
              (let [[report-defunload-issues!-calls load-nodes!-calls] (call-logged-resource-sync!)]

                (testing "Safe dependencies on unloaded files are not reported."
                  (is (= 0 (count report-defunload-issues!-calls))))

                (testing "Only the added file was loaded."
                  (is (= 1 (count load-nodes!-calls)))
                  (let [[_ _ loaded-node-load-infos] (first load-nodes!-calls)]
                    (is (= #{"/editable/loaded-referencing-unloaded-go.collection"}
                           (coll/transfer loaded-node-load-infos (sorted-set)
                             (map (fn [{:keys [resource] :as _node-load-info}]
                                    (resource/proj-path resource)))))))))

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

            (testing "Added file resource referencing unloaded unsafe proj-path loads the single still-unloaded transitive dependency."
              (let [[report-defunload-issues!-calls load-nodes!-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies on still-unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/editable/loaded-referencing-unloaded-tilemap.go"
                            #{"/editable/unloaded.tilemap"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/editable/unloaded.tilemap"}
                           loaded-undesired-proj-paths))))

                (testing "Only the still-unloaded files were loaded."
                  (is (= 1 (count load-nodes!-calls)))
                  (let [[_ _ loaded-node-load-infos] (first load-nodes!-calls)]
                    (is (= #{"/editable/loaded-referencing-unloaded-tilemap.go"
                             "/editable/unloaded.tilemap"}
                           (coll/transfer loaded-node-load-infos (sorted-set)
                             (map (fn [{:keys [resource] :as _node-load-info}]
                                    (resource/proj-path resource)))))))))

              (is (loaded-proj-path? "/editable/unloaded.png"))
              (is (loaded-proj-path? "/editable/unloaded.tilesource"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilesource.tilemap"))
              (is (loaded-proj-path? "/editable/unloaded.tilemap"))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-tilemap.go"))
              (is (not (loaded-proj-path? "/editable/unloaded.go")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-go.collection"))
              (is (not (loaded-proj-path? "/editable/unloaded.collection")))
              (is (loaded-proj-path? "/editable/loaded-referencing-unloaded-collection.collection")))

            ;; Simulate external changes to defunloaded file resources that are
            ;; unsafely referenced by loaded file resources.
            (test-util/write-file-resource! workspace
              "/editable/unloaded.tilesource"
              {:image "/editable/unloaded.png"
               :tile-width 16
               :tile-height 16})

            (test-util/write-file-resource! workspace
              "/editable/unloaded.tilemap"
              {:tile-set "/editable/unloaded.tilesource"
               :blend-mode :blend-mode-mult})

            (testing "Unsafely referenced unloaded files are reloaded after external changes."
              (let [[report-defunload-issues!-calls load-nodes!-calls] (call-logged-resource-sync!)]

                (testing "Reloads of unsafe dependencies are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {} ; None of the reloaded resources were referenced from a loaded resource.
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/editable/unloaded.tilemap"
                             "/editable/unloaded.tilesource"}
                           loaded-undesired-proj-paths))))

                (testing "Only the externally modified files were reloaded."
                  (is (= 1 (count load-nodes!-calls)))
                  (let [[_ _ loaded-node-load-infos] (first load-nodes!-calls)]
                    (is (= #{"/editable/unloaded.tilesource"
                             "/editable/unloaded.tilemap"}
                           (coll/transfer loaded-node-load-infos (sorted-set)
                             (map (fn [{:keys [resource] :as _node-load-info}]
                                    (resource/proj-path resource))))))))))

            ;; Simulate an external change to an existing resource and introduce
            ;; an unsafe reference to an existing defunloaded resource.
            (test-util/write-file-resource! workspace
              "/editable/loaded-referencing-unloaded-go.collection"
              {:name "loaded-referencing-unloaded-go"
               :embedded-instances
               [{:id "embedded-game-object-instance"
                 :data {:components
                        [{:id "unloaded-tilemap"
                          :component "/editable/unloaded.tilemap"}]}}]})

            (testing "Loaded files are reloaded with unsafe dependencies after external changes."
              (let [[report-defunload-issues!-calls load-nodes!-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies introduced by external changes are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/editable/loaded-referencing-unloaded-go.collection"
                            #{"/editable/unloaded.tilemap"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{}
                           loaded-undesired-proj-paths))))

                (testing "Only the externally modified files were reloaded."
                  (is (= 1 (count load-nodes!-calls)))
                  (let [[_ _ loaded-node-load-infos] (first load-nodes!-calls)]
                    (is (= #{"/editable/loaded-referencing-unloaded-go.collection"}
                           (coll/transfer loaded-node-load-infos (sorted-set)
                             (map (fn [{:keys [resource] :as _node-load-info}]
                                    (resource/proj-path resource))))))))))))))))

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

(ns integration.unload-test
  (:require [clojure.java.io :as io]
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
          (format "%s/unloaded/unloaded.%s"
                  (if (:editable resource-type)
                    editable-directory-proj-path
                    non-editable-directory-proj-path)
                  (:ext resource-type)))

        defunload-patterns
        (mapv #(str % "/unloaded")
              [editable-directory-proj-path
               non-editable-directory-proj-path])]

    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path [non-editable-directory-proj-path])
      (test-util/write-defunload-patterns! project-path defunload-patterns)

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
  (let [resource-node-id (project/get-resource-node project proj-path)]
    (assert (some? resource-node-id) (str "Resource not found: " proj-path))
    (resource-node/loaded? resource-node-id)))

(deftest defunload-load-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          (fs/copy! (io/file "test/resources/images/small.png")
                    (io/file project-path "unloaded/unloaded.png"))

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.tilesource"
            {:image "/unloaded/unloaded.png"
             :tile-width 8
             :tile-height 8})

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.tilemap"
            {:tile-set "/unloaded/unloaded.tilesource"
             :blend-mode :blend-mode-add})

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.go"
            {:components
             [{:id "unloaded_tilemap"
               :component "/unloaded/unloaded.tilemap"}]})

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.collection"
            {:name "unloaded_collection"
             :instances
             [{:id "unloaded_game_object_instance"
               :prototype "/unloaded/unloaded.go"}]})

          (test-util/write-file-resource! workspace
            "/loaded_referencing_unloaded_collection.collection"
            {:name "loaded_referencing_unloaded_collection"
             :collection-instances
             [{:id "unloaded_collection_instance"
               :collection "/unloaded/unloaded.collection"}]})

          (workspace/resource-sync! workspace)

          (let [project (test-util/setup-project! workspace)
                loaded-proj-path? #(loaded-proj-path? project %)

                call-logged-resource-sync!
                (fn call-logged-resource-sync! []
                  (binding [project/report-defunload-issues! (fn/make-call-logger)
                            project/node-load-info-tx-data (fn/make-call-logger project/node-load-info-tx-data)]
                    (workspace/resource-sync! workspace)
                    [(fn/call-logger-calls project/report-defunload-issues!)
                     (fn/call-logger-calls project/node-load-info-tx-data)]))]

            (testing "Matching proj-paths are excluded from loading."
              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add a loaded file resource that unsafely depends on an unloaded
            ;; resource. The unloaded resource does not :allow-unloaded-use, so
            ;; we must load it and all its dependencies that also do not
            ;; :allow-unloaded-use.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_tilesource.tilemap"
              {:tile-set "/unloaded/unloaded.tilesource"})

            (testing "Added file resource referencing unloaded unsafe proj-path loads transitive dependencies."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies on unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/loaded_referencing_unloaded_tilesource.tilemap"
                            #{"/unloaded/unloaded.tilesource"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/unloaded/unloaded.png"
                             "/unloaded/unloaded.tilesource"}
                           loaded-undesired-proj-paths))))

                (testing "The added file and its transitive dependencies were loaded."
                  (is (= ["/unloaded/unloaded.png"
                          "/unloaded/unloaded.tilesource"
                          "/loaded_referencing_unloaded_tilesource.tilemap"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add a loaded file resource that safely depends on an unloaded
            ;; resource. The unloaded resource does :allow-unloaded-use, so we
            ;; should not load it as a result.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_go.collection"
              {:name "loaded_referencing_unloaded_go"
               :instances
               [{:id "unloaded_game_object_instance"
                 :prototype "/unloaded/unloaded.go"}]})

            (testing "Added file resource referencing unloaded safe proj-path does not load transitive dependencies."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls] (call-logged-resource-sync!)]

                (testing "Safe dependencies on unloaded files are not reported."
                  (is (= 0 (count report-defunload-issues!-calls))))

                (testing "Only the added file was loaded."
                  (is (= ["/loaded_referencing_unloaded_go.collection"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add another loaded file resource that unsafely depends on an
            ;; unloaded resource that itself unsafely depends on a defunloaded
            ;; resource that has already been loaded. Check that the already
            ;; loaded resources are not loaded twice as a result.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_tilemap.go"
              {:components
               [{:id "unloaded_tilemap"
                 :component "/unloaded/unloaded.tilemap"}]})

            (testing "Added file resource referencing unloaded unsafe proj-path loads the single still-unloaded transitive dependency."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies on still-unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/loaded_referencing_unloaded_tilemap.go"
                            #{"/unloaded/unloaded.tilemap"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/unloaded/unloaded.tilemap"}
                           loaded-undesired-proj-paths))))

                (testing "Only the still-unloaded files were loaded."
                  (is (= ["/unloaded/unloaded.tilemap"
                          "/loaded_referencing_unloaded_tilemap.go"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilemap"))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Simulate external changes to defunloaded file resources that are
            ;; unsafely referenced by loaded file resources.
            (test-util/write-file-resource! workspace
              "/unloaded/unloaded.tilesource"
              {:image "/unloaded/unloaded.png"
               :tile-width 16
               :tile-height 16})

            (test-util/write-file-resource! workspace
              "/unloaded/unloaded.tilemap"
              {:tile-set "/unloaded/unloaded.tilesource"
               :blend-mode :blend-mode-mult})

            (testing "Unsafely referenced unloaded files are reloaded after external changes."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls] (call-logged-resource-sync!)]

                (testing "Reloads of unsafe dependencies are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {} ; None of the reloaded resources were referenced from a loaded resource.
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{"/unloaded/unloaded.tilemap"
                             "/unloaded/unloaded.tilesource"}
                           loaded-undesired-proj-paths))))

                (testing "Only the externally modified files were reloaded."
                  (is (= ["/unloaded/unloaded.tilesource"
                          "/unloaded/unloaded.tilemap"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))

            ;; Simulate an external change to an existing resource and introduce
            ;; an unsafe reference to an existing defunloaded resource.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_go.collection"
              {:name "loaded_referencing_unloaded_go"
               :embedded-instances
               [{:id "embedded_game_object_instance"
                 :data {:components
                        [{:id "unloaded_tilemap"
                          :component "/unloaded/unloaded.tilemap"}]}}]})

            (testing "Loaded files are reloaded with unsafe dependencies after external changes."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls] (call-logged-resource-sync!)]

                (testing "Unsafe dependencies introduced by external changes are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/loaded_referencing_unloaded_go.collection"
                            #{"/unloaded/unloaded.tilemap"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{}
                           loaded-undesired-proj-paths))))

                (testing "Only the externally modified files were reloaded."
                  (is (= ["/loaded_referencing_unloaded_go.collection"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))))))))

(deftest defunload-scene-edit-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          (fs/copy! (io/file "test/resources/images/small.png")
                    (io/file project-path "unloaded/unloaded.png"))

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.tilesource"
            {:image "/unloaded/unloaded.png"
             :tile-width 8
             :tile-height 8})

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.tilemap"
            {:tile-set "/unloaded/unloaded.tilesource"
             :blend-mode :blend-mode-add})

          (test-util/write-file-resource! workspace
            "/first_loaded_referencing_unloaded_tilemap.go"
            {:components
             [{:id "first_reference"
               :component ""}
              {:id "second_reference"
               :component ""}]})

          (test-util/write-file-resource! workspace
            "/second_loaded_referencing_unloaded_tilemap.go"
            {:components
             [{:id "first_reference"
               :component ""}
              {:id "second_reference"
               :component ""}]})

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.go"
            {})

          (test-util/write-file-resource! workspace
            "/loaded_referencing_unloaded_go.collection"
            {:name "loaded_referencing_unloaded_go"
             :instances
             [{:id "first_reference"
               :prototype ""}
              {:id "second_reference"
               :prototype ""}]})

          (workspace/resource-sync! workspace)

          (let [project (test-util/setup-project! workspace)
                loaded-proj-path? #(loaded-proj-path? project %)

                call-logged-transact!
                (fn call-logged-transact! [tx-data]
                  (binding [project/node-load-info-tx-data (fn/make-call-logger project/node-load-info-tx-data)]
                    (g/transact tx-data)
                    (fn/call-logger-calls project/node-load-info-tx-data)))]

            (testing "Matching proj-paths are excluded from loading."
              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (loaded-proj-path? "/first_loaded_referencing_unloaded_tilemap.go"))
              (is (loaded-proj-path? "/second_loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection")))

            (testing "Editing a loaded resource to safely reference unloaded resources will not load the referenced resources."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (-> (project/get-resource-node project "/loaded_referencing_unloaded_go.collection")
                          (test-util/referenced-game-objects)
                          (coll/transfer []
                            (map #(g/set-property % :path {:resource (workspace/find-resource workspace "/unloaded/unloaded.go")})))))]

                (is (= []
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))

              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (loaded-proj-path? "/first_loaded_referencing_unloaded_tilemap.go"))
              (is (loaded-proj-path? "/second_loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection")))

            (testing "Editing a loaded resource to unsafely reference unloaded resources will load transitive dependencies."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (-> (project/get-resource-node project "/first_loaded_referencing_unloaded_tilemap.go")
                          (test-util/referenced-components)
                          (coll/transfer []
                            (map #(g/set-property % :path {:resource (workspace/find-resource workspace "/unloaded/unloaded.tilemap")})))))]

                (is (= ["/unloaded/unloaded.tilemap"
                        "/unloaded/unloaded.tilesource"
                        "/unloaded/unloaded.png"]
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))

              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilemap"))
              (is (loaded-proj-path? "/first_loaded_referencing_unloaded_tilemap.go"))
              (is (loaded-proj-path? "/second_loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))

              (testing "Editing a loaded resource to unsafely reference already loaded resources does not reload the referenced resources."
                (let [node-load-info-tx-data-calls
                      (call-logged-transact!
                        (-> (project/get-resource-node project "/second_loaded_referencing_unloaded_tilemap.go")
                            (test-util/referenced-components)
                            (coll/transfer []
                              (map #(g/set-property % :path {:resource (workspace/find-resource workspace "/unloaded/unloaded.tilemap")})))))]

                  (is (= []
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))))))))

(deftest defunload-script-edit-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          (fs/copy! (io/file "test/resources/images/small.png")
                    (io/file project-path "unloaded/unloaded.png"))

          (test-util/write-file-resource! workspace
            "/loaded_math.lua"
            ["local m = {}"
             "function m.add(a, b)"
             "  return a + b"
             "end"
             "return m"])

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded_math.lua"
            ["local loaded_math = require 'loaded_math'"
             "local m = {}"
             "function m.multiply(a, b)"
             "  local result = 0"
             "  for i = 1, a do"
             "    result = loaded_math.add(result, b)"
             "  end"
             "  return result"
             "end"
             "return m"])

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.lua"
            ["local unloaded_math = require 'unloaded.unloaded_math'"
             "local m = {}"
             "function m.square(a)"
             "  return unloaded_math.multiply(a, a)"
             "end"
             "return m"])

          (test-util/write-file-resource! workspace
            "/loaded_referencing_unloaded_lua.script"
            [])

          (test-util/write-file-resource! workspace
            "/unloaded/unloaded.tilesource"
            {:image "/unloaded/unloaded.png"
             :tile-width 8
             :tile-height 8})

          (test-util/write-file-resource! workspace
            "/loaded_referencing_unloaded_tilesource.script"
            [])

          (workspace/resource-sync! workspace)

          (let [project (test-util/setup-project! workspace)
                loaded-proj-path? #(loaded-proj-path? project %)

                call-logged-transact!
                (fn call-logged-transact! [tx-data]
                  (binding [project/node-load-info-tx-data (fn/make-call-logger project/node-load-info-tx-data)]
                    (g/transact tx-data)
                    (fn/call-logger-calls project/node-load-info-tx-data)))]

            (testing "Matching proj-paths are excluded from loading."
              (is (loaded-proj-path? "/loaded_math.lua"))
              (is (not (loaded-proj-path? "/unloaded/unloaded_math.lua")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.lua")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_lua.script"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.script")))

            (testing "Editing a script to reference unloaded Lua modules will not load transitive dependencies."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (test-util/set-code-editor-lines
                        (project/get-resource-node project "/loaded_referencing_unloaded_lua.script")
                        ["local first_alias = require 'unloaded.unloaded'"
                         "local second_alias = require 'unloaded.unloaded'"]))]

                (is (= []
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))

              (is (loaded-proj-path? "/loaded_math.lua"))
              (is (not (loaded-proj-path? "/unloaded/unloaded_math.lua")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.lua")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_lua.script"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.script"))

              (testing "Editing a script to reference already loaded Lua modules does not reload the referenced Lua modules."
                (let [node-load-info-tx-data-calls
                      (call-logged-transact!
                        (test-util/set-code-editor-lines
                          (project/get-resource-node project "/loaded_referencing_unloaded_lua.script")
                          ["local first_alias = require 'unloaded.unloaded_math'"
                           "local second_alias = require 'unloaded.unloaded_math'"]))]

                  (is (= []
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))

            (testing "Editing a script to reference unloaded resources will load transitive dependencies."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (test-util/set-code-editor-lines
                        (project/get-resource-node project "/loaded_referencing_unloaded_tilesource.script")
                        ["go.property('first_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"
                         "go.property('second_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"]))]

                (is (= ["/unloaded/unloaded.tilesource"
                        "/unloaded/unloaded.png"]
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))

              (is (loaded-proj-path? "/loaded_math.lua"))
              (is (not (loaded-proj-path? "/unloaded/unloaded_math.lua")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.lua")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_lua.script"))
              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.script"))

              (testing "Editing a script to reference already loaded resources does not reload the referenced resources."
                (let [node-load-info-tx-data-calls
                      (call-logged-transact!
                        (test-util/set-code-editor-lines
                          (project/get-resource-node project "/loaded_referencing_unloaded_tilesource.script")
                          ["go.property('first_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"
                           "go.property('second_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"
                           "go.property('third_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"]))]

                  (is (= []
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))))))))

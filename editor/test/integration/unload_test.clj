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
            [editor.lsp :as lsp]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-loading :as resource-loading]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [internal.node :as in]
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
        (let [workspace (test-util/setup-workspace! project-path)]

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
                (coll/into-> loadable-resource-type-colls-by-editability (sorted-map)
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
                (let [{:keys [basis-ref]} evaluation-context]
                  (doseq [proj-path loadable-resource-proj-paths]
                    (let [resource (workspace/find-resource @basis-ref workspace proj-path)
                          resource-node (project/get-resource-node project resource evaluation-context)
                          resource-type (resource/resource-type resource)]
                      (testing proj-path
                        (is (not (resource/loaded? resource)))
                        (is (not (resource-node/loaded? @basis-ref resource-node)))
                        (is (= (not (:allow-unloaded-use resource-type))
                               (in/shell-node? (g/node-by-id @basis-ref resource-node))))
                        (is (= resource (resource-node/resource @basis-ref resource-node)))
                        (is (not (g/connected? @basis-ref resource-node :save-data project :save-data)))
                        (if (:editor-openable resource-type)
                          (is (not (resource/openable? resource)))
                          (is (resource/openable? resource)))
                        (when (:allow-unloaded-use resource-type)
                          (is (not (g/error? (g/node-value resource-node :_properties evaluation-context))))
                          (is (not (g/error? (g/node-value resource-node :node-outline evaluation-context))))
                          (is (not (g/error? (g/node-value resource-node :build-targets evaluation-context))))
                          (when (resource-type-has-view-type? resource-type :scene)
                            (is (not (g/error? (g/node-value resource-node :scene evaluation-context)))))))))))
              (lsp/await (lsp/get-lsp)))))))))

(defn- loaded-proj-path? [project proj-path]
  (let [resource-node-id (project/get-resource-node project proj-path)]
    (assert (some? resource-node-id) (str "Resource not found: " proj-path))
    (resource-node/loaded? resource-node-id)))

;; A tilemap must report its unsafe reference to an excluded tilesource both
;; while the tilesource is a ShellNode and after it has materialized.
(deftest defunload-unsafe-reference-reporting-test
  (doseq [tile-source-materialized [false true]]
    (testing (if tile-source-materialized "Materialized tilesource" "Tilesource ShellNode")
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
            tile-map-proj-path "/level.tilemap"
            tile-source-proj-path "/terrain.tilesource"]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (test-util/write-defunload-patterns! project-path [tile-source-proj-path])
          (test-support/with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)]
              (test-util/write-file-resource! workspace tile-source-proj-path
                {:image "/builtins/graphics/particle_blob.png"
                 :tile-width 8
                 :tile-height 8})
              (test-util/write-file-resource! workspace tile-map-proj-path
                {:tile-set tile-source-proj-path})
              (workspace/resource-sync! workspace)
              (let [project (test-util/setup-project! workspace)
                    tile-source-node-id (test-util/resource-node project tile-source-proj-path)
                    tile-map-node-id (test-util/resource-node project tile-map-proj-path)]
                (binding [project/report-defunload-issues! (fn/make-call-logger)]
                  (when tile-source-materialized
                    (g/with-auto-evaluation-context evaluation-context
                      (g/materialize-node! tile-source-node-id evaluation-context)))
                  (is (= (not tile-source-materialized)
                         (in/shell-node? (g/node-by-id (g/now) tile-source-node-id))))
                  (g/with-auto-evaluation-context evaluation-context
                    (g/materialize-node! tile-map-node-id evaluation-context))
                  (let [tile-map-report [workspace {tile-map-proj-path #{tile-source-proj-path}} #{}]]
                    (is (= (if-not tile-source-materialized
                             [tile-map-report]
                             [[workspace {} #{tile-source-proj-path}] tile-map-report])
                           (fn/call-logger-calls project/report-defunload-issues!))))
                  (is (= tile-source-materialized (resource-node/loaded? tile-source-node-id))))
                (lsp/await (lsp/get-lsp))))))))))

;; Synchronization leaves added resources unloaded. Materializing a consumer
;; preserves excluded dependency shells until their outputs are evaluated.
(deftest defunload-load-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)]

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

          (let [project (test-util/materialize-project! (test-util/setup-project! workspace))
                loaded-proj-path? #(loaded-proj-path? project %)

                call-logged!
                (fn call-logged! [action!]
                  (binding [project/report-defunload-issues! (fn/make-call-logger)
                            project/node-load-info-tx-data (fn/make-call-logger project/node-load-info-tx-data)]
                    (action!)
                    [(fn/call-logger-calls project/report-defunload-issues!)
                     (fn/call-logger-calls project/node-load-info-tx-data)]))

                sync-added-resource!
                (fn sync-added-resource! [proj-path]
                  (testing "Synchronization leaves added resources unloaded without reporting unsafe references."
                    (is (= [[] []] (call-logged! #(workspace/resource-sync! workspace))))
                    (is (not (loaded-proj-path? proj-path)))
                    (is (in/shell-node? (g/node-by-id (g/now) (project/get-resource-node project proj-path))))))

                materialize-resource!
                (fn materialize-resource! [proj-path]
                  (let [node-id (project/get-resource-node project proj-path)]
                    (g/with-auto-evaluation-context evaluation-context
                      (g/materialize-node! node-id evaluation-context))
                    (is (not (g/defective? (g/now) node-id)))))]

            (testing "Loading resource dependencies stops at safe excluded placeholders."
              (is (= [[] []]
                     (call-logged!
                       #(resource-loading/load-resources! project ["/loaded_referencing_unloaded_collection.collection"] [] progress/null-render-progress! fn/constantly-false))))
              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add a tilemap outside .defunload that references an excluded
            ;; tilesource. The tilesource lacks :allow-unloaded-use, so the
            ;; tilemap should report the reference when it loads. Its loader
            ;; only needs to connect to the tilesource, so both the tilesource
            ;; and the image it references can stay unloaded.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_tilesource.tilemap"
              {:tile-set "/unloaded/unloaded.tilesource"})

            (sync-added-resource! "/loaded_referencing_unloaded_tilesource.tilemap")

            (testing "Materializing a resource leaves excluded dependencies as shells."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(materialize-resource! "/loaded_referencing_unloaded_tilesource.tilemap"))]

                (testing "Unsafe dependencies on unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/loaded_referencing_unloaded_tilesource.tilemap"
                            #{"/unloaded/unloaded.tilesource"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{} loaded-undesired-proj-paths))))

                (testing "Only the added resource was loaded."
                  (is (= ["/loaded_referencing_unloaded_tilesource.tilemap"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add a resource that safely depends on an unloaded resource. The
            ;; unloaded resource does :allow-unloaded-use, so we should not
            ;; load it as a result.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_go.collection"
              {:name "loaded_referencing_unloaded_go"
               :instances
               [{:id "unloaded_game_object_instance"
                 :prototype "/unloaded/unloaded.go"}]})

            (sync-added-resource! "/loaded_referencing_unloaded_go.collection")

            (testing "Materializing an added resource referencing an unloaded safe proj-path does not load transitive dependencies."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(materialize-resource! "/loaded_referencing_unloaded_go.collection"))]

                (testing "Safe dependencies on unloaded files are not reported."
                  (is (= 0 (count report-defunload-issues!-calls))))

                (testing "Only the added file was loaded."
                  (is (= ["/loaded_referencing_unloaded_go.collection"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            ;; Add a game object whose component is the excluded tilemap. That
            ;; tilemap refers to the same excluded tilesource as the first
            ;; tilemap. Loading the game object should report its own reference
            ;; to the excluded tilemap. The tilemap, tilesource and image stay
            ;; unloaded until the game object's build targets need them.
            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_tilemap.go"
              {:components
               [{:id "unloaded_tilemap"
                 :component "/unloaded/unloaded.tilemap"}]})

            (sync-added-resource! "/loaded_referencing_unloaded_tilemap.go")

            (testing "Materializing another consumer preserves the excluded dependency chain."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(materialize-resource! "/loaded_referencing_unloaded_tilemap.go"))]

                (testing "Unsafe dependencies on still-unloaded files are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    (is (= {"/loaded_referencing_unloaded_tilemap.go"
                            #{"/unloaded/unloaded.tilemap"}}
                           unsafe-dependency-proj-paths-by-referencing-proj-path))
                    (is (= #{} loaded-undesired-proj-paths))))

                (testing "Only the added resource was loaded."
                  (is (= ["/loaded_referencing_unloaded_tilemap.go"]
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls)))))

              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilesource.tilemap"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.collection")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_collection.collection")))

            (test-util/write-file-resource! workspace
              "/loaded_referencing_unloaded_tilesource.tilemap"
              {:tile-set "/unloaded/unloaded.tilesource"
               :blend-mode :blend-mode-add})

            (test-util/write-file-resource! workspace
              "/unloaded/unloaded.tilesource"
              {:image "/unloaded/unloaded.png"
               :tile-width 4
               :tile-height 4})

            (testing "Reloading a materialized consumer and an excluded dependency preserves the dependency shell."
              (let [[_report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(workspace/resource-sync! workspace))]
                (is (= ["/loaded_referencing_unloaded_tilesource.tilemap"]
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))
              (doseq [proj-path ["/unloaded/unloaded.png"
                                 "/unloaded/unloaded.tilesource"
                                 "/unloaded/unloaded.tilemap"]]
                (is (not (loaded-proj-path? proj-path)))
                (is (in/shell-node? (g/node-by-id (g/now) (project/get-resource-node project proj-path))))))

            (testing "Evaluating a consumer output materializes the dependencies it needs."
              (let [node-id (project/get-resource-node project "/loaded_referencing_unloaded_tilemap.go")

                    [_report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(is (not (g/error? (g/node-value node-id :build-targets)))))]
                (is (= #{"/unloaded/unloaded.png"
                         "/unloaded/unloaded.tilesource"
                         "/unloaded/unloaded.tilemap"}
                       (coll/into-> node-load-info-tx-data-calls #{}
                         (map (comp resource/proj-path :resource first))))))
              (is (loaded-proj-path? "/unloaded/unloaded.png"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
              (is (loaded-proj-path? "/unloaded/unloaded.tilemap")))

            ;; Simulate external edits to the excluded tilesource and tilemap
            ;; after evaluating the game object's build targets materialized
            ;; them. Their consumers now depend on their loaded data, so both
            ;; replacements must pick up the edits even though the paths still
            ;; match .defunload. The tilesource must reload before the tilemap.
            (test-util/write-file-resource! workspace
              "/unloaded/unloaded.tilesource"
              {:image "/unloaded/unloaded.png"
               :tile-width 16
               :tile-height 16})

            (test-util/write-file-resource! workspace
              "/unloaded/unloaded.tilemap"
              {:tile-set "/unloaded/unloaded.tilesource"
               :blend-mode :blend-mode-mult})

            (testing "Materialized excluded resources reload after external changes."
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(workspace/resource-sync! workspace))]

                (testing "Reloads of unsafe dependencies are reported."
                  (is (= 1 (count report-defunload-issues!-calls)))
                  (let [[_ unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths] (first report-defunload-issues!-calls)]
                    ;; Both resources being read are excluded. Their consumers
                    ;; outside .defunload were not reloaded in this operation,
                    ;; so only the excluded loads appear in this report.
                    (is (= {}
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
              (let [[report-defunload-issues!-calls node-load-info-tx-data-calls]
                    (call-logged! #(workspace/resource-sync! workspace))]

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
                               node-load-info-tx-data-calls))))))
            (lsp/await (lsp/get-lsp))))))))

;; Connecting excluded scene resources preserves their shells until output
;; evaluation needs them, and later connections reuse the materialized nodes.
(deftest defunload-scene-edit-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)]

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

          (let [project (test-util/materialize-project! (test-util/setup-project! workspace))
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
                          (coll/into-> []
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

            (testing "Editing a loaded resource to reference excluded dependencies preserves their shells."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (-> (project/get-resource-node project "/first_loaded_referencing_unloaded_tilemap.go")
                          (test-util/referenced-components)
                          (coll/into-> []
                            (map #(g/set-property % :path {:resource (workspace/find-resource workspace "/unloaded/unloaded.tilemap")})))))]

                (is (= []
                       (mapv (comp resource/proj-path :resource first)
                             node-load-info-tx-data-calls))))

              (is (not (loaded-proj-path? "/unloaded/unloaded.png")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilesource")))
              (is (not (loaded-proj-path? "/unloaded/unloaded.tilemap")))
              (is (loaded-proj-path? "/first_loaded_referencing_unloaded_tilemap.go"))
              (is (loaded-proj-path? "/second_loaded_referencing_unloaded_tilemap.go"))
              (is (not (loaded-proj-path? "/unloaded/unloaded.go")))
              (is (loaded-proj-path? "/loaded_referencing_unloaded_go.collection"))

              (testing "Evaluating build targets materializes the excluded dependencies."
                (let [node-id (project/get-resource-node project "/first_loaded_referencing_unloaded_tilemap.go")]
                  (is (not (g/error? (g/node-value node-id :build-targets)))))
                (is (loaded-proj-path? "/unloaded/unloaded.png"))
                (is (loaded-proj-path? "/unloaded/unloaded.tilesource"))
                (is (loaded-proj-path? "/unloaded/unloaded.tilemap")))

              (testing "Editing a loaded resource to unsafely reference already loaded resources does not reload the referenced resources."
                (let [node-load-info-tx-data-calls
                      (call-logged-transact!
                        (-> (project/get-resource-node project "/second_loaded_referencing_unloaded_tilemap.go")
                            (test-util/referenced-components)
                            (coll/into-> []
                              (map #(g/set-property % :path {:resource (workspace/find-resource workspace "/unloaded/unloaded.tilemap")})))))]

                  (is (= []
                         (mapv (comp resource/proj-path :resource first)
                               node-load-info-tx-data-calls))))))
            (lsp/await (lsp/get-lsp))))))))

;; Script resource properties can connect to excluded dependency shells, while
;; evaluating build targets materializes the resources that provide their data.
(deftest defunload-script-edit-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/write-defunload-patterns! project-path ["/unloaded"])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)]

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

          (let [project (test-util/materialize-project! (test-util/setup-project! workspace))
                loaded-proj-path? #(loaded-proj-path? project %)
                script-intelligence (project/script-intelligence project)
                unloaded-lua-node (project/get-resource-node project "/unloaded/unloaded.lua")

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

            (testing "Unloaded resource shells have their global connections."
              (is (contains? (set (g/inputs (g/now) project :breakpoints))
                             (gt/->Arc unloaded-lua-node :breakpoints project :breakpoints)))
              (is (contains? (set (g/inputs (g/now) script-intelligence :required-module-infos))
                             (gt/->Arc unloaded-lua-node :required-module-info script-intelligence :required-module-infos))))

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

            (testing "Editing a script to reference excluded resources preserves their shells."
              (let [node-load-info-tx-data-calls
                    (call-logged-transact!
                      (test-util/set-code-editor-lines
                        (project/get-resource-node project "/loaded_referencing_unloaded_tilesource.script")
                        ["go.property('first_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"
                         "go.property('second_alias', resource.tile_source('/unloaded/unloaded.tilesource'))"]))]

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

              (testing "Evaluating build targets materializes the excluded dependencies."
                (let [node-id (project/get-resource-node project "/loaded_referencing_unloaded_tilesource.script")]
                  (is (not (g/error? (g/node-value node-id :build-targets)))))
                (is (loaded-proj-path? "/unloaded/unloaded.png"))
                (is (loaded-proj-path? "/unloaded/unloaded.tilesource")))

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
                               node-load-info-tx-data-calls))))))

            (lsp/await (lsp/get-lsp))))))))

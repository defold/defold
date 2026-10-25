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

(ns integration.resource-read-test
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.data :as data]
            [editor.defold-project :as project]
            [editor.defold-project-search :as project-search]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support :refer [with-clean-system]]
            [util.coll :as coll :refer [pair]])
  (:import [internal.node NodeImpl ShellNode]
           [java.io InputStream]
           [java.util.concurrent CountDownLatch TimeUnit]))

(set! *warn-on-reflection* true)

(defn- read-node-load-infos-without-graph-queries [read-opts resources]
  (let [resource-count (count resources)
        node-ids (g/take-node-ids resource-count)
        node-id+resource-pairs (mapv pair node-ids resources)]
    (test-util/with-graph-queries-blocked :disallow-unsafe-basis
      (project/read-node-load-infos read-opts node-id+resource-pairs resource-count progress/null-render-progress!))))

(deftest embedded-readers-use-resource-type-snapshot-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path ["/non-editable"])
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              prototype-desc {:embedded-components
                              [{:id "gui"
                                :type "gui"
                                :data {:script "nested.gui_script"
                                       :nodes [{:id "box" :type :type-box}]}}
                               {:id "light"
                                :type "point_light"
                                :data {:data {"intensity" 2.0}}}]}
              cases [["go" prototype-desc [:embedded-components]]
                     ["collection"
                      {:name "test" :embedded-instances [{:id "go" :data prototype-desc}]}
                      [:embedded-instances 0 :data :embedded-components]]]]
          (doseq [directory ["editable" "non-editable"]
                  [ext save-value] cases]
            (test-util/write-file-resource! workspace (str "/" directory "/test." ext) save-value))
          (workspace/resource-sync! workspace)
          (let [basis (g/now)
                read-opts (workspace/make-read-opts basis workspace :include-editor-dependencies true)
                proj-path->resource (:proj-path->resource read-opts)]
            (doseq [directory ["editable" "non-editable"]
                    :let [resources (mapv #(proj-path->resource (str "/" directory "/test." (first %))) cases)
                          node-load-infos (read-node-load-infos-without-graph-queries read-opts resources)]
                    [[ext _ data-path] node-load-info] (mapv pair cases node-load-infos)]
              (testing (str directory "/test." ext)
                (let [components (get-in (:source-value node-load-info) data-path)
                      dependencies (set (:dependency-proj-paths node-load-info))]
                  (is (nil? (:read-error node-load-info)))
                  (is (= :type-box (get-in components [0 :data :nodes 0 :type])))
                  (is (= 2.0 (get-in components [1 :data :data "intensity"])))
                  (is (contains? dependencies (str "/" directory "/nested.gui_script")))
                  (is (contains? dependencies "/builtins/fonts/default.font")))))))))))

;; Registered templates produce truthy source-value entries without graph
;; queries, as required by disk-state initialization.
(deftest templates-read-without-graph-queries-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path ["/non-editable"])
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              basis (g/now)
              {:keys [editable->type-ext->resource-type template-resource-fn]} (workspace/make-read-opts basis workspace)
              cases (coll/into-> editable->type-ext->resource-type []
                      (coll/mapcat
                        (fn [[editable resource-types]]
                          (coll/into-> resource-types :eduction
                            (keep (fn [[ext {:keys [read-fn lazy-loaded] :as resource-type}]]
                                    (when read-fn
                                      (when-let [template (template-resource-fn resource-type false)]
                                        (let [proj-path (str (if editable "/editable/" "/non-editable/") "template." ext)
                                              file (io/file project-path (subs proj-path 1))]
                                          (with-open [^InputStream input-stream (io/input-stream template)]
                                            (test-support/write-until-new-mtime file (.readAllBytes input-stream)))
                                          {:proj-path proj-path :lazy-loaded lazy-loaded})))))))))]
          (workspace/resource-sync! workspace)
          (let [basis (g/now)
                {:keys [proj-path->resource] :as read-opts} (workspace/make-read-opts basis workspace :include-editor-dependencies true)
                resources (mapv #(proj-path->resource (:proj-path %)) cases)
                node-load-infos (read-node-load-infos-without-graph-queries read-opts resources)]
            (is (= (count cases) (count node-load-infos)))
            (doseq [[{:keys [proj-path lazy-loaded]} node-load-info] (mapv pair cases node-load-infos)]
              (testing proj-path
                (is (nil? (:read-error node-load-info)))
                (is (= (not lazy-loaded) (contains? node-load-info :source-value)))
                (when-not lazy-loaded
                  (is (:source-value node-load-info))
                  (is (string? (:disk-sha256 node-load-info))))))))))))

(deftest data-default-template-read-without-graph-queries-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              template-file (io/file project-path "extension.graphfree")
              user-template-file (io/file project-path "templates/default.graphfree")
              source-file (io/file project-path "source.graphfree")]
          (g/transact
            (data/register-data-resource-type workspace
              :ext "graphfree"
              :node-type resource-node/ResourceNode
              :template "/extension.graphfree"
              :dependencies-fn (fn [read-opts owner-resource source-value]
                                 [((:resolve-proj-path-fn read-opts) owner-resource
                                                                     (get-in source-value [:data "dependency"]))])))
          (let [resource-type (workspace/get-resource-type workspace "graphfree")
                write-fn (:write-fn resource-type)
                default-data {"default" "extension" "kept" true "dependency" "dependency.graphfree"}]
            (test-support/write-until-new-mtime template-file (write-fn {:data default-data}))
            (test-support/write-until-new-mtime user-template-file (write-fn {:data {"default" "user"}}))
            (test-support/write-until-new-mtime source-file (write-fn {:data {"value" "read"}}))
            (workspace/resource-sync! workspace)
            (let [basis (g/now)
                  read-opts (workspace/make-read-opts basis workspace)
                  template-resource-fn (:template-resource-fn read-opts)
                  resources [((:proj-path->resource read-opts) "/source.graphfree")]]
              (test-util/with-graph-queries-blocked :disallow-unsafe-basis
                (is (= "/templates/default.graphfree"
                       (resource/proj-path (template-resource-fn resource-type true))))
                (is (= "/extension.graphfree"
                       (resource/proj-path (template-resource-fn resource-type false)))))
              (let [node-load-info (first (read-node-load-infos-without-graph-queries read-opts resources))]
                (is (= {:data (assoc default-data "value" "read")} (:source-value node-load-info)))
                (is (= ["/dependency.graphfree"] (:dependency-proj-paths node-load-info))))
              (test-support/write-until-new-mtime template-file "invalid template")
              (test-support/write-until-new-mtime source-file "")
              (let [node-load-info (first (read-node-load-infos-without-graph-queries read-opts resources))]
                (is (= {:data default-data} (:source-value node-load-info)))
                (is (= ["/dependency.graphfree"] (:dependency-proj-paths node-load-info)))))))))))

(g/defnode PrerequisiteResource
  (inherits resource-node/ResourceNode)
  (property value g/Int (default 0))
  (output result g/Int :cached (g/fnk [value] value)))

(defn- call-with-prerequisite-project [contents prerequisites-fn test-fn]
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              load-fn-calls (atom [])]
          (g/transact
            (workspace/register-resource-type workspace
              :ext "prereq"
              :node-type PrerequisiteResource
              :textual? true
              :auto-connect-save-data? false
              :read-fn (fn [_read-opts _resource input] (edn/read-string (slurp input)))
              :write-fn pr-str
              :prerequisites-fn prerequisites-fn
              :dependencies-fn (fn [_read-opts _resource source-value]
                                 (:dependencies source-value (:prerequisites source-value)))
              :load-fn (fn [_load-opts {:keys [node-id resource source-value]}]
                         (swap! load-fn-calls conj (resource/proj-path resource))
                         (g/set-property node-id :value (:value source-value 1)))))
          (doseq [[proj-path source-value] contents]
            (test-util/write-file-resource! workspace proj-path source-value))
          (workspace/resource-sync! workspace)
          (let [project (test-util/setup-project! workspace)]
            (test-fn project workspace load-fn-calls)))))))

;; Reverse materialization follows prerequisite chains while leaving ordinary
;; dependents and unrelated resources, including broken ones, unmaterialized.
(deftest materialize-resource-referencers-test
  (call-with-prerequisite-project
    {"/target.prereq" {:value 42}
     "/direct.prereq" {:prerequisites ["/target.prereq"]}
     "/transitive.prereq" {:prerequisites ["/direct.prereq"]}
     "/dependency-only.prereq" {:dependencies ["/target.prereq"]}
     "/unrelated.prereq" {:value 1}
     "/broken.prereq" {:fail true}}
    (fn test-prerequisites [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn check-materialized-referencers [project _workspace load-fn-calls]
      (let [target-resource (resource-node/resource (test-util/resource-node project "/target.prereq"))]
        (is (= [] @load-fn-calls))
        (g/with-auto-evaluation-context evaluation-context
          (resource-node/materialize-resource-referencers! project [target-resource] evaluation-context))
        (is (= ["/target.prereq" "/direct.prereq" "/transitive.prereq"] @load-fn-calls))
        (doseq [proj-path ["/dependency-only.prereq" "/unrelated.prereq" "/broken.prereq"]]
          (is (not (resource-node/loaded? (test-util/resource-node project proj-path)))))
        (g/with-auto-evaluation-context evaluation-context
          (resource-node/materialize-resource-referencers! project [target-resource] evaluation-context))
        (is (= ["/target.prereq" "/direct.prereq" "/transitive.prereq"] @load-fn-calls))))))

(g/defnode CachedReadResource
  (inherits resource-node/ResourceNode)
  (property value g/Any)
  (output save-value g/Any (g/fnk [value] value)))

;; Searching shells caches source-value and disk-sha256 without loading nodes or
;; discovering dependencies. Later searches and materialization reuse the read;
;; failed reads leave shells unchanged and are excluded from search results.
(deftest search-caches-shell-reads-test
  (doseq [editable [false true]]
    (testing (str "editable: " editable)
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (when-not editable
            (test-util/set-non-editable-directories! project-path ["/resources"]))
          (with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)
                  reads (atom {})
                  load-calls (atom [])
                  dependency-calls (atom [])
                  prerequisite-calls (atom [])
                  search-errors (atom [])
                  contents {"/resources/valid.searchcache" {:value "needle"}
                            "/resources/empty.searchcache" {}
                            "/resources/blocked.searchcache" {:prerequisites ["/missing.searchcache"]}
                            "/resources/broken.searchcache" {:read-error true}}]
              (g/transact
                (workspace/register-resource-type workspace
                  :ext "searchcache"
                  :editable editable
                  :node-type CachedReadResource
                  :textual? true
                  :auto-connect-save-data? false
                  :read-fn
                  (fn read-search-resource [_read-opts resource input]
                    (swap! reads update (resource/proj-path resource) (fnil inc 0))
                    (let [source-value (edn/read-string (slurp input))]
                      (if-not (:read-error source-value)
                        source-value
                        (throw (ex-info "Failed to read search resource." {})))))
                  :write-fn pr-str
                  :search-fn resource-node/default-ddf-resource-search-fn
                  :dependencies-fn
                  (fn dependencies [_read-opts resource _source-value]
                    (swap! dependency-calls conj (resource/proj-path resource))
                    [])
                  :prerequisites-fn
                  (fn prerequisites [_read-opts resource source-value]
                    (swap! prerequisite-calls conj (resource/proj-path resource))
                    (:prerequisites source-value))
                  :load-fn
                  (fn load-search-resource [_load-opts {:keys [node-id resource source-value]}]
                    (swap! load-calls conj (resource/proj-path resource))
                    (g/set-property node-id :value source-value))))
              (doseq [[proj-path source-value] contents]
                (test-util/write-file-resource! workspace proj-path source-value))
              (workspace/resource-sync! workspace)
              (let [project (test-util/setup-project! workspace)]
                (is (= {} @reads))
                (dotimes [search-index 2]
                  (let [search-data
                        @(project-search/make-search-data-future
                           (fn report-error! [error]
                             (swap! search-errors conj error))
                           project
                           (fn search-resource? [resource]
                             (= "searchcache" (resource/type-ext resource))))]
                    ;; Wait for search to commit its evaluation context.
                    (ui/run-now nil)
                    (is (= (dissoc contents "/resources/broken.searchcache")
                           (into {}
                                 (map (fn [{:keys [resource search-value]}]
                                        (pair (resource/proj-path resource)
                                              search-value)))
                                 search-data)))
                    (doseq [[proj-path source-value] contents]
                      (let [node-id (test-util/resource-node project proj-path)
                            node (g/node-by-id (g/now) node-id)]
                        (is (instance? ShellNode node))
                        (is (not (resource-node/loaded? node-id)))
                        (if (:read-error source-value)
                          (do
                            (is (= (inc search-index) (@reads proj-path)))
                            (is (not (contains? node :source-value)))
                            (is (not (contains? node :disk-sha256))))
                          (do
                            (is (= 1 (@reads proj-path)))
                            (is (= source-value (:source-value node)))
                            (is (= (resource/resource->sha256-hex
                                     (resource-node/resource node-id))
                                   (:disk-sha256 node)))))))
                    (is (= [] @load-calls))
                    (is (= [] @dependency-calls))
                    (is (= [] @prerequisite-calls))
                    (is (= [] @search-errors))))
                (let [node-id (test-util/resource-node project "/resources/valid.searchcache")
                      resource (resource-node/resource node-id)
                      search-value-fn (:search-value-fn (resource/resource-type resource))]
                  (is (= {:value "needle"} (g/node-value node-id :save-value)))
                  (is (= 1 (@reads "/resources/valid.searchcache")))
                  (is (= ["/resources/valid.searchcache"] @load-calls))
                  (is (= (when editable {:value "needle"})
                         (g/node-value node-id :source-value)))
                  (when editable
                    (g/set-property! node-id :value {:value "unsaved"})
                    (g/with-auto-evaluation-context evaluation-context
                      (is (= {:value "unsaved"}
                             (search-value-fn node-id resource evaluation-context))))))
                (let [node-id (test-util/resource-node project "/resources/blocked.searchcache")
                      resource (resource-node/resource node-id)
                      search-value-fn (:search-value-fn (resource/resource-type resource))]
                  (is (g/error-value? (g/node-value node-id :save-value)))
                  (g/with-auto-evaluation-context evaluation-context
                    (is (= (contents "/resources/blocked.searchcache")
                           (search-value-fn node-id resource evaluation-context))))
                  (is (= 1 (@reads "/resources/blocked.searchcache")))
                  (is (= ["/resources/valid.searchcache"] @load-calls)))))))))))

(g/defnode FailingLoadResource
  (inherits CachedReadResource)
  (input children g/Any :array :cascade-delete)
  (property fail-loading g/Bool
            (set (fn fail-loading [_evaluation-context _self _old-value value]
                   (when value
                     (throw (ex-info "Failed while applying load steps." {})))))))

;; A failing load setter must leave a defective NodeImpl without partial
;; properties, child nodes, connections or undo data. Failed non-editable
;; resources retain source-value for fallback search. Successful loads clear it.
(deftest applying-load-steps-failure-test
  (doseq [bulk [false true]]
    (testing (str "bulk: " bulk)
      (let [project-path
            (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (test-util/set-non-editable-directories! project-path ["/resources"])
          (with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)
                  source-value {:value "cached source-value"}
                  reads (atom 0)
                  load-calls (atom 0)
                  partial-child (atom nil)]
              (g/transact
                (workspace/register-resource-type workspace
                  :ext "loadfailure"
                  :editable false
                  :node-type FailingLoadResource
                  :auto-connect-save-data? false
                  :read-fn (fn read-failing-resource [_read-opts _resource input]
                             (swap! reads inc)
                             (edn/read-string (slurp input)))
                  :write-fn pr-str
                  :prerequisites-fn (constantly [])
                  :load-fn
                  (fn load-failing-resource [_load-opts {:keys [node-id]}]
                    (swap! load-calls inc)
                    (g/make-nodes [child CachedReadResource]
                      (g/set-property node-id :value :partial)
                      (g/connect child :save-value node-id :children)
                      (g/callback-ec
                        (fn observe-partial-load [evaluation-context]
                          (let [basis @(:basis-ref evaluation-context)]
                            (is (= source-value (g/node-value node-id :source-value evaluation-context)))
                            (is (= :partial (g/node-value node-id :value evaluation-context)))
                            (is (some? (g/node-by-id basis child)))
                            (is (= child
                                   (g/node-feeding-into
                                     basis node-id :children)))
                            (reset! partial-child child))))
                      (g/set-property node-id :fail-loading true)))))
              (test-util/write-file-resource!
                workspace "/resources/broken.loadfailure" source-value)
              (workspace/resource-sync! workspace)
              (let [project (test-util/setup-project! workspace)
                    node-id (test-util/resource-node project "/resources/broken.loadfailure")
                    resource (resource-node/resource node-id)
                    disk-sha256 (resource/resource->sha256-hex resource)
                    check-failed-load
                    (fn check-failed-load [basis]
                      (is (instance? NodeImpl (g/node-by-id basis node-id)))
                      (is (g/defective? basis node-id))
                      (is (resource-node/loaded? basis node-id))
                      (is (= source-value
                             (g/raw-property-value basis node-id :source-value)))
                      (is (= disk-sha256
                             (g/raw-property-value basis node-id :disk-sha256)))
                      (is (nil? (g/raw-property-value basis node-id :value)))
                      (is (nil? (g/raw-property-value
                                  basis node-id :fail-loading)))
                      (is (coll/empty? (g/inputs basis node-id :children)))
                      (is (some? @partial-child))
                      (is (nil? (g/node-by-id basis @partial-child))))]
                ;; Prerequisite discovery caches the read without loading it.
                (g/with-auto-evaluation-context evaluation-context
                  (resource-node/materialize-resource-referencers!
                    project [resource] evaluation-context))
                (is (instance? ShellNode (g/node-by-id (g/now) node-id)))
                (is (= source-value (g/node-value node-id :source-value)))
                (is (= disk-sha256 (g/node-value node-id :disk-sha256)))
                (is (= 1 @reads))
                (is (zero? @load-calls))
                (g/reset-undo! :undo/global)
                (log/without-logging
                  (if bulk
                    (let [node-load-infos
                          (project/read-nodes (g/now) [[node-id resource]]
                            :force-read true)]
                      (project/load-nodes! project nil node-load-infos
                                           progress/null-render-progress!
                                           nil {}))
                    (g/with-auto-evaluation-context evaluation-context
                      (g/materialize-node! node-id evaluation-context)
                      (check-failed-load @(:basis-ref evaluation-context))
                      (is (instance? ShellNode
                                     (g/node-by-id (g/now) node-id))))))
                (check-failed-load (g/now))
                (let [error (g/node-value node-id :save-value)]
                  (is (g/error-value? error))
                  (is (= "Failed while applying load steps."
                         (:cause (Throwable->map
                                   (get-in error [:user-data :exception]))))))
                (is (= source-value (g/node-value node-id :source-value)))
                (is (= 1 @load-calls))
                (g/with-auto-evaluation-context evaluation-context
                  (is (= source-value
                         ((:search-value-fn (resource/resource-type resource))
                          node-id resource evaluation-context))))
                (is (= (if bulk 2 1) @reads))
                (when bulk
                  (g/undo! :undo/global)
                  (is (nil? (g/raw-property-value (g/now) node-id :value)))
                  (is (nil? (g/node-by-id (g/now) @partial-child)))
                  (g/redo! :undo/global)
                  (check-failed-load (g/now))
                  (is (= 1 @load-calls)))))))))))

;; Scans retain successful reads, including empty collections, on shells for
;; later scans and materialization. Reloads must discard stale data.
(deftest prerequisite-scans-cache-shell-reads-test
  (doseq [editable [false true]
          source-value-fn [nil hash]]
    (testing (str "editable: " editable ", converted source-value: " (some? source-value-fn))
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (when-not editable
            (test-util/set-non-editable-directories! project-path ["/resources"]))
          (with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)
                  reads (atom {})
                  load-infos (atom {})
                  contents {"/resources/target.readcache" {:value 1}
                            "/resources/referencer.readcache" {:prerequisites ["/resources/target.readcache"]}
                            "/resources/unrelated.readcache" {:dependencies ["/resources/target.readcache"]}
                            "/resources/changed.readcache" {:value 1}
                            "/resources/empty-map.readcache" {}
                            "/resources/empty-vector.readcache" []
                            "/resources/read-error.readcache" {:read-error true}
                            "/resources/prerequisite-error.readcache" {:prerequisite-error true}
                            "/resources/missing.readcache" {:prerequisites ["/resources/absent.readcache"]}}]
              (g/transact
                (workspace/register-resource-type workspace
                  :ext "readcache"
                  :editable editable
                  :node-type CachedReadResource
                  :auto-connect-save-data? false
                  :read-fn (fn read-cache-resource [_read-opts resource input]
                             (swap! reads update (resource/proj-path resource) (fnil inc 0))
                             (let [source-value (edn/read-string (slurp input))]
                               (if-not (:read-error source-value)
                                 source-value
                                 (throw (ex-info "Failed to read resource." {})))))
                  :write-fn pr-str
                  :source-value-fn source-value-fn
                  :search-value-fn (fn search-value [node-id _resource evaluation-context]
                                     (g/node-value node-id :save-value evaluation-context))
                  :prerequisites-fn (fn prerequisites [_read-opts _resource source-value]
                                      (if-not (:prerequisite-error source-value)
                                        (:prerequisites source-value)
                                        (throw (ex-info "Failed to discover prerequisites." {}))))
                  :dependencies-fn (fn dependencies [_read-opts _resource source-value]
                                     (:dependencies source-value))
                  :load-fn (fn load-cache-resource [_load-opts {:keys [node-id resource source-value] :as node-load-info}]
                             (swap! load-infos assoc (resource/proj-path resource) node-load-info)
                             (g/set-property node-id :value source-value))))
              (doseq [[proj-path source-value] contents]
                (test-util/write-file-resource! workspace proj-path source-value))
              (workspace/resource-sync! workspace)
              (let [project (test-util/setup-project! workspace)
                    target-resource (test-util/resource workspace "/resources/target.readcache")]
                (is (= {} @reads))
                (dotimes [scan 2]
                  (g/with-auto-evaluation-context evaluation-context
                    (resource-node/materialize-resource-referencers! project [target-resource] evaluation-context))
                  (is (= #{"/resources/target.readcache" "/resources/referencer.readcache"}
                         (into #{} (coll/keys @load-infos))))
                  (doseq [[proj-path source-value] contents]
                    (testing proj-path
                      (let [node-id (test-util/resource-node project proj-path)
                            node (g/node-by-id (g/now) node-id)]
                        (is (= (if (:read-error source-value) (inc scan) 1) (@reads proj-path)))
                        (when-not (contains? @load-infos proj-path)
                          (is (not (resource-node/loaded? node-id)))
                          (is (= (not (:read-error source-value)) (contains? node :source-value)))
                          (when-not (:read-error source-value)
                            (is (= source-value (:source-value node)))
                            (is (= (resource/resource->sha256-hex (resource-node/resource node-id))
                                   (:disk-sha256 node)))))))))
                (testing "Timestamp changes preserve the shell and its cached read"
                  (let [node-id (test-util/resource-node project "/resources/unrelated.readcache")]
                    (test-support/touch-until-new-mtime (io/as-file (resource-node/resource node-id)))
                    (workspace/resource-sync! workspace)
                    (is (= node-id (test-util/resource-node project "/resources/unrelated.readcache")))
                    (is (not (resource-node/loaded? node-id)))
                    (is (= 1 (@reads "/resources/unrelated.readcache")))))
                (testing "Materialization reconstructs load info and converts disk state"
                  (doseq [[proj-path source-value] contents
                          :when (and (not (:read-error source-value))
                                     (not= "/resources/changed.readcache" proj-path))]
                    (let [node-id (test-util/resource-node project proj-path)
                          blocked (or (:prerequisite-error source-value)
                                      (= "/resources/missing.readcache" proj-path))]
                      (if blocked
                        (is (g/error-value? (g/node-value node-id :save-value)))
                        (do
                          (is (= source-value (g/node-value node-id :save-value)))
                          (is (= source-value (:source-value (@load-infos proj-path))))
                          (is (= (:dependencies source-value) (:dependency-proj-paths (@load-infos proj-path))))
                          (is (= (:prerequisites source-value) (:prerequisite-proj-paths (@load-infos proj-path))))))
                      (is (= (when (or editable blocked)
                               (if-not source-value-fn
                                 source-value
                                 (source-value-fn source-value)))
                             (g/node-value node-id :source-value)))
                      (is (= 1 (@reads proj-path))))))
                (testing "Changed content replaces the cached source-value"
                  (let [node-id (test-util/resource-node project "/resources/changed.readcache")]
                    (is (not (resource-node/loaded? node-id)))
                    (test-util/write-file-resource! workspace "/resources/changed.readcache" {:value 2})
                    (workspace/resource-sync! workspace)
                    (is (not= node-id (test-util/resource-node project "/resources/changed.readcache")))
                    (is (= {:value 2} (g/node-value (test-util/resource-node project "/resources/changed.readcache") :save-value)))
                    (is (= 2 (@reads "/resources/changed.readcache")))))))))))))

;; Loaded resources report current prerequisites, so unsaved reference edits
;; determine which unmaterialized containers are discovered.
(deftest materialize-resource-referencers-uses-live-references-test
  (test-util/with-temp-project-content
    {"/original.script" []
     "/replacement.script" []
     "/referencer.go" {:components [{:id "script" :component "/original.script"}]}
     "/container.collection" {:name "container"
                              :instances [{:id "referencer" :prototype "/referencer.go"}]}}
    (let [component-node-id (test-util/resource-outline-node-id project "/referencer.go" "script")
          container-node-id (test-util/resource-node project "/container.collection")
          original-resource (test-util/resource workspace "/original.script")
          replacement-resource (test-util/resource workspace "/replacement.script")]
      (test-util/prop! component-node-id :path {:resource replacement-resource :overrides []})
      (is (not (resource-node/loaded? container-node-id)))
      (g/with-auto-evaluation-context evaluation-context
        (resource-node/materialize-resource-referencers! project [original-resource] evaluation-context))
      (is (not (resource-node/loaded? container-node-id)))
      (g/with-auto-evaluation-context evaluation-context
        (resource-node/materialize-resource-referencers! project [replacement-resource] evaluation-context))
      (is (resource-node/loaded? container-node-id)))))

;; Prerequisite discovery failures must prevent both the resource and its
;; dependents from loading.
(deftest prerequisite-read-failure-blocks-dependent-load-operation-test
  (call-with-prerequisite-project
    {"/broken.prereq" {:fail true}
     "/dependent.prereq" {:prerequisites ["/broken.prereq"]}
     "/independent.prereq" {:value 42}}
    (fn [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn [project workspace load-fn-calls]
      (let [broken (test-util/resource-node project "/broken.prereq")
            dependent (test-util/resource-node project "/dependent.prereq")
            independent (test-util/resource-node project "/independent.prereq")]
        (is (g/error-value? (g/node-value dependent :result)))
        (is (= {:fail true} (g/node-value broken :source-value)))
        (is (= {:prerequisites ["/broken.prereq"]} (g/node-value dependent :source-value)))
        (is (= [] @load-fn-calls))
        (let [basis (g/now)
              read-opts (workspace/make-read-opts basis workspace)
              resource (resource-node/resource basis broken)
              node-load-info (project/read-node-load-info read-opts broken resource)]
          (is (g/error-value? (:read-error node-load-info)))
          (is (= {:fail true} (:source-value node-load-info)))
          (is (= (resource/resource->sha256-hex resource) (:disk-sha256 node-load-info)))
          (is (not (contains? node-load-info :prerequisite-proj-paths))))
        (is (= 42 (g/node-value independent :result)))
        (is (= ["/independent.prereq"] @load-fn-calls))))))

;; Successful reads retain their hashes so timestamp-only changes keep blocked
;; nodes.
(deftest blocked-resource-unchanged-content-retains-node-test
  (call-with-prerequisite-project
    {"/broken.prereq" {:fail true}
     "/dependent.prereq" {:prerequisites ["/broken.prereq"]}}
    (fn [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn [project workspace load-fn-calls]
      (is (g/error-value? (g/node-value (test-util/resource-node project "/dependent.prereq") :result)))
      (doseq [proj-path ["/dependent.prereq" "/broken.prereq"]]
        (let [node-id (test-util/resource-node project proj-path)
              resource (resource-node/resource node-id)]
          (is (= (resource/resource->sha256-hex resource) (g/node-value node-id :disk-sha256)))
          (test-support/touch-until-new-mtime (io/as-file resource))
          (workspace/resource-sync! workspace)
          (is (= node-id (test-util/resource-node project proj-path)))
          (is (g/error-value? (g/node-value node-id :result)))))
      (is (= [] @load-fn-calls)))))

;; Resource cycles become read errors with paths, including when different
;; threads own the promises.
(deftest cyclic-resource-prerequisites-test
  (doseq [concurrent [false true]]
    (let [entered (when concurrent (CountDownLatch. 2))]
      (call-with-prerequisite-project
        {"/a.prereq" {:prerequisites ["/b.prereq"]}
         "/b.prereq" {:prerequisites ["/a.prereq"]}}
        (fn [_read-opts _resource source-value]
          (when entered
            (.countDown ^CountDownLatch entered)
            (when-not (.await ^CountDownLatch entered 10 TimeUnit/SECONDS)
              (throw (ex-info "Timed out starting prerequisite reads." {}))))
          (:prerequisites source-value))
        (fn [project _workspace load-fn-calls]
          (let [node-ids (mapv #(test-util/resource-node project %) ["/a.prereq" "/b.prereq"])
                evaluation-contexts (mapv (fn [_] (g/make-evaluation-context)) node-ids)
                results (when concurrent
                          (mapv (fn [node-id evaluation-context]
                                  (future (g/node-value node-id :result evaluation-context)))
                                node-ids
                                evaluation-contexts))]
            (try
              (doseq [[index node-id] (map-indexed vector node-ids)]
                (is (g/error-value? (if concurrent
                                      (deref (results index) 10000 ::timeout)
                                      (g/node-value node-id :result (evaluation-contexts index))))))
              (doseq [evaluation-context evaluation-contexts]
                (g/update-system-from-evaluation-context! evaluation-context))
              (is (= [] @load-fn-calls))
              (doseq [[node-id prerequisite] (mapv vector node-ids ["/b.prereq" "/a.prereq"])]
                (is (= {:prerequisites [prerequisite]} (g/node-value node-id :source-value))))
              (let [exceptions (mapv #(get-in (g/node-value % :result) [:user-data :exception]) node-ids)
                    cycle-exception (coll/first-where #(= :prerequisite-cycle (:ex-type (ex-data %))) exceptions)]
                (is (some? cycle-exception))
                (is (re-find #"/a.prereq" (ex-message cycle-exception)))
                (is (re-find #"/b.prereq" (ex-message cycle-exception))))
              (finally
                (doseq [result results]
                  (future-cancel result))))))))))

;; Embedded prerequisite discovery failures must reach the owner instead of
;; becoming warnings.
(deftest embedded-prerequisite-read-failure-blocks-load-operation-test
  (call-with-prerequisite-project
    {"/embedded.go" {:embedded-components [{:id "broken" :type "prereq" :data {:fail true}}]}}
    (fn [_read-opts _resource _source-value]
      (throw (ex-info "Failed to discover embedded prerequisites." {})))
    (fn [project _workspace load-fn-calls]
      (let [node-id (test-util/resource-node project "/embedded.go")
            save-value (g/node-value node-id :save-value)
            source-value (g/node-value node-id :source-value)]
        (is (= "broken" (get-in source-value [:embedded-components 0 :id])))
        (is (g/error-value? save-value))
        (is (= [] @load-fn-calls))
        (is (= "broken"
               (:embedded-component-id (ex-data (get-in save-value [:user-data :exception])))))))))

;; Bulk reloads must reject cycles and failed prerequisites before invoking any
;; affected loader.
(deftest bulk-read-prerequisite-errors-test
  (call-with-prerequisite-project
    {"/a.prereq" {:prerequisites ["/b.prereq"]}
     "/b.prereq" {:prerequisites ["/a.prereq"]}
     "/dependent.prereq" {:prerequisites ["/a.prereq"]}
     "/broken.prereq" {:fail true}
     "/broken-dependent.prereq" {:prerequisites ["/broken.prereq"]}
     "/independent.prereq" {:value 42}}
    (fn [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn [project _workspace load-fn-calls]
      (g/let-ec [{:keys [basis-ref]} evaluation-context
                 old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path evaluation-context)

                 new-node-id+resource-pairs
                 (coll/into-> old-node-ids-by-proj-path []
                   (filter (fn [[proj-path _]]
                             (.endsWith ^String proj-path ".prereq")))
                   (map (fn [[_ node-id]]
                          (let [resource (resource-node/resource @basis-ref node-id)]
                            (pair node-id resource)))))

                 node-load-infos (project/read-nodes @basis-ref new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)
                 node-load-infos-by-proj-path (coll/pair-map-by (comp resource/proj-path :resource) node-load-infos)]
        (doseq [[proj-path node-load-info] node-load-infos-by-proj-path]
          (if (= "/independent.prereq" proj-path)
            (is (nil? (:read-error node-load-info)))
            (do
              (is (g/error-value? (:read-error node-load-info)))
              (is (= (edn/read-string (slurp (:resource node-load-info))) (:source-value node-load-info)))
              (is (= (resource/resource->sha256-hex (:resource node-load-info)) (:disk-sha256 node-load-info))))))
        (project/load-nodes! project nil node-load-infos progress/null-render-progress! nil {})
        (is (= ["/independent.prereq"] @load-fn-calls))
        (is (= 42 (g/node-value (old-node-ids-by-proj-path "/independent.prereq") :result)))
        (let [basis (g/now)
              dependent (old-node-ids-by-proj-path "/broken-dependent.prereq")
              dependent-resource (resource-node/resource basis dependent)
              new-node-id+resource-pairs [(pair dependent dependent-resource)]
              ;; The broken prerequisite is now loaded, so it is outside this batch.
              node-load-infos (project/read-nodes basis new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)]
          (is (= 1 (count node-load-infos)))
          (is (g/error-value? (:read-error (first node-load-infos)))))))))

;; Prerequisites can be editor-only dependencies and must precede consumers in
;; bulk load operations.
(deftest bulk-read-prerequisite-order-test
  (call-with-prerequisite-project
    {"/a.prereq" {:prerequisites ["/b.prereq" "/c.prereq"] :dependencies []}
     "/b.prereq" {:prerequisites ["/d.prereq"] :dependencies []}
     "/c.prereq" {:prerequisites ["/d.prereq"] :dependencies []}
     "/d.prereq" {:value 42}}
    (fn [_read-opts _resource source-value] (:prerequisites source-value))
    (fn [project _workspace load-fn-calls]
      (g/let-ec [{:keys [basis-ref]} evaluation-context
                 old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path evaluation-context)

                 new-node-id+resource-pairs
                 (mapv (fn [proj-path]
                         (let [node-id (old-node-ids-by-proj-path proj-path)
                               resource (resource-node/resource @basis-ref node-id)]
                           (pair node-id resource)))
                       ["/a.prereq" "/b.prereq" "/c.prereq" "/d.prereq"])

                 node-load-infos (project/read-nodes @basis-ref new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)
                 order (coll/into-> node-load-infos {}
                         (map-indexed (fn [index info]
                                        (pair (resource/proj-path (:resource info)) index))))]
        (is (coll/every? #(nil? (:read-error %)) node-load-infos))
        (is (= 4 (count node-load-infos)))
        (is (< (order "/d.prereq") (order "/b.prereq") (order "/a.prereq")))
        (is (< (order "/d.prereq") (order "/c.prereq") (order "/a.prereq")))
        (project/load-nodes! project nil node-load-infos progress/null-render-progress! nil {})
        (is (= 4 (count @load-fn-calls)))))))

;; Missing paths must block both materialization and bulk reload paths and
;; recover unchanged transitive dependents when created.
(deftest missing-prerequisite-recovery-test
  (doseq [bulk [false true]]
    (call-with-prerequisite-project
      {"/root.prereq" {:prerequisites ["/middle.prereq"] :value 5}
       "/middle.prereq" {:prerequisites ["/missing.prereq"] :value 2}}
      (fn [_read-opts _resource source-value] (:prerequisites source-value))
      (fn [project workspace load-fn-calls]
        (let [old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path)
              root (old-node-ids-by-proj-path "/root.prereq")
              middle (old-node-ids-by-proj-path "/middle.prereq")]
          (when bulk
            (let [basis (g/now)
                  root-resource (resource-node/resource basis root)
                  new-node-id+resource-pairs [(pair root root-resource)]
                  node-load-infos (project/read-nodes basis new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)]
              (project/load-nodes! project nil node-load-infos progress/null-render-progress! nil {})))
          (let [error (g/node-value root :result)]
            (is (g/error-value? error))
            (is (coll/any? #(and (= :file-not-found (get-in % [:user-data :type]))
                                 (= "/missing.prereq" (some-> (get-in % [:user-data :resource]) resource/proj-path)))
                           (into [] (coll/tree-xf :causes :causes) [error]))))
          (is (= {:prerequisites ["/middle.prereq"] :value 5} (g/node-value root :source-value)))
          (is (= {:prerequisites ["/missing.prereq"] :value 2} (g/node-value middle :source-value)))
          (is (= [] @load-fn-calls))
          (is (nil? (get (g/node-value project :nodes-by-resource-path) "/missing.prereq")))

          (test-util/write-file-resource! workspace "/unrelated.prereq" {:value 9})
          (workspace/resource-sync! workspace)
          (is (= root (test-util/resource-node project "/root.prereq")))
          (is (= middle (test-util/resource-node project "/middle.prereq")))

          (reset! load-fn-calls [])
          (test-util/write-file-resource! workspace "/missing.prereq" {:value 42})
          (workspace/resource-sync! workspace)
          (let [new-root (test-util/resource-node project "/root.prereq")
                new-middle (test-util/resource-node project "/middle.prereq")]
            (is (not= root new-root))
            (is (not= middle new-middle))
            (is (= 5 (g/node-value new-root :result)))
            (is (= 2 (g/node-value new-middle :result)))
            (is (= {"/missing.prereq" 1 "/middle.prereq" 1 "/root.prereq" 1}
                   (frequencies @load-fn-calls)))))))))

;; Blocked files retain source-value for fallback search even when non-editable.
;; Resource types that store hashes must supply their own :search-value-fn.
(deftest blocked-search-source-values-test
  (doseq [source-value-fn [nil hash]]
    (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
      (with-open [_ (test-util/make-directory-deleter project-path)]
        (test-util/set-non-editable-directories! project-path ["/non-editable"])
        (with-clean-system
          (let [workspace (test-util/setup-workspace! project-path)]
            (g/transact
              (workspace/register-resource-type workspace
                :ext "searchvalue"
                :node-type resource-node/ResourceNode
                :textual? true
                :read-fn (fn [_read-opts _resource input] (slurp input))
                :load-fn (fn load-search-value [_load-opts _node-load-info] nil)
                :write-fn identity
                :source-value-fn source-value-fn
                :search-value-fn (when source-value-fn
                                   (fn [node-id _resource evaluation-context]
                                     (g/node-value node-id :save-value evaluation-context)))
                :prerequisites-fn (fn [_read-opts _resource _source-value] ["/missing.prereq"])))
            (doseq [directory ["editable" "non-editable"]]
              (test-util/write-file-resource! workspace (str "/" directory "/blocked.searchvalue") "needle"))
            (workspace/resource-sync! workspace)
            (let [project (test-util/setup-project! workspace)]
              (doseq [directory ["editable" "non-editable"]]
                (let [node-id (test-util/resource-node project (str "/" directory "/blocked.searchvalue"))
                      resource (resource-node/resource node-id)
                      save-error (g/node-value node-id :save-value)
                      source-value (g/node-value node-id :source-value)
                      search-value (g/with-auto-evaluation-context evaluation-context
                                     ((:search-value-fn (resource/resource-type resource)) node-id resource evaluation-context))]
                  (is (= (= "editable" directory) (resource/editable? resource)))
                  (is (g/error-value? save-error))
                  (is (= ((or source-value-fn identity) "needle") source-value))
                  (is (= (if source-value-fn save-error source-value) search-value)))))))))))

;; Prerequisite checks must honor supplied resources in both materialization and
;; bulk reload paths, even without files on disk.
(deftest supplied-prerequisite-resources-test
  (doseq [bulk [false true]]
    (call-with-prerequisite-project
      {}
      (fn [_read-opts _resource source-value] (:prerequisites source-value))
      (fn [_project workspace load-fn-calls]
        (let [project-directory (workspace/project-directory workspace)
              resources (mapv (fn [[filename contents]]
                                (test-util/make-fake-file-resource
                                  workspace
                                  (.getPath project-directory)
                                  (io/file project-directory filename)
                                  (.getBytes (pr-str contents) "UTF-8")))
                              [["dependent.prereq" {:prerequisites ["/prerequisite.prereq"] :value 5}]
                               ["prerequisite.prereq" {:value 42}]])
              project (test-util/setup-project! workspace resources)
              old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path)
              dependent (old-node-ids-by-proj-path "/dependent.prereq")]
          (when bulk
            (let [basis (g/now)

                  new-node-id+resource-pairs
                  (mapv (fn [resource]
                          (let [proj-path (resource/proj-path resource)
                                node-id (old-node-ids-by-proj-path proj-path)]
                            (pair node-id resource)))
                        resources)

                  node-load-infos (project/read-nodes basis new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)]
              (project/load-nodes! project nil node-load-infos progress/null-render-progress! nil {})))
          (is (= 5 (g/node-value dependent :result)))
          (is (= ["/prerequisite.prereq" "/dependent.prereq"] @load-fn-calls)))))))

;; Failed prerequisite metadata must survive discovery errors and cycles so
;; either can be repaired.
(deftest failed-prerequisite-recovery-test
  (doseq [failure [:discovery :cycle]
          bulk [false true]]
    (call-with-prerequisite-project
      {"/root.prereq" {:prerequisites ["/a.prereq"]}
       "/a.prereq" {:prerequisites ["/b.prereq"]}
       "/b.prereq" (case failure
                     :discovery {:fail true}
                     :cycle {:prerequisites ["/a.prereq"]})}
      (fn [_read-opts _resource source-value]
        (if-not (:fail source-value)
          (:prerequisites source-value)
          (throw (ex-info "Failed to discover prerequisites." {}))))
      (fn [project workspace load-fn-calls]
        (when bulk
          (g/let-ec [{:keys [basis-ref]} evaluation-context
                     root (project/get-resource-node project "/root.prereq" evaluation-context)
                     root-resource (resource-node/resource @basis-ref root)
                     old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path evaluation-context)
                     new-node-id+resource-pairs [(pair root root-resource)]
                     node-load-infos (project/read-nodes @basis-ref new-node-id+resource-pairs :old-node-ids-by-proj-path old-node-ids-by-proj-path)]
            (project/load-nodes! project nil node-load-infos progress/null-render-progress! nil {})))
        (is (g/error-value? (g/node-value (test-util/resource-node project "/root.prereq") :result)))
        (is (= [] @load-fn-calls))
        (test-util/write-file-resource! workspace "/b.prereq" {:value 42})
        (workspace/resource-sync! workspace)
        (is (= 1 (g/node-value (test-util/resource-node project "/root.prereq") :result)))
        (is (= {"/b.prereq" 1 "/a.prereq" 1 "/root.prereq" 1} (frequencies @load-fn-calls)))))))

;; Loading a missing game object must not produce a save value that silently
;; drops its overrides.
(deftest missing-game-object-retains-overrides-on-recovery-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              overrides [{:id "script" :properties [{:id "speed" :value "42.0" :type :property-type-number}]}]]
          (test-util/write-file-resource! workspace "/main/main.collection"
            {:name "main" :instances [{:id "missing" :prototype "/missing.go" :component-properties overrides}]})
          (workspace/resource-sync! workspace)
          (let [project (test-util/setup-project! workspace)
                main (test-util/resource-node project "/main/main.collection")]
            (is (g/error-value? (g/node-value main :save-value)))
            (is (g/error-value? (g/node-value main :save-data)))
            (test-util/write-file-resource! workspace "/missing.go"
              {:components [{:id "script" :component "/component.script"}]})
            (test-util/write-file-resource! workspace "/component.script" ["go.property(\"speed\", 1)" ""])
            (workspace/resource-sync! workspace)
            (let [main (test-util/resource-node project "/main/main.collection")
                  save-value (g/node-value main :save-value)]
              (is (not (g/error? save-value)))
              (is (= overrides (get-in save-value [:instances 0 :component-properties])))
              (is (false? (g/node-value main :dirty))))))))))

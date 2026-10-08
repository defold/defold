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
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support :refer [with-clean-system]]
            [util.coll :as coll])
  (:import [java.io InputStream]
           [java.util.concurrent CountDownLatch TimeUnit]))

(set! *warn-on-reflection* true)

(defn- read-node-load-infos-without-graph-queries [read-opts resources]
  (let [resource-count (count resources)
        node-ids (g/take-node-ids resource-count)
        node-id+resource-pairs (mapv coll/pair node-ids resources)]
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
                    [[ext _ data-path] node-load-info] (mapv coll/pair cases node-load-infos)]
              (testing (str directory "/test." ext)
                (let [components (get-in (:source-value node-load-info) data-path)
                      dependencies (set (:dependency-proj-paths node-load-info))]
                  (is (nil? (:read-error node-load-info)))
                  (is (= :type-box (get-in components [0 :data :nodes 0 :type])))
                  (is (= 2.0 (get-in components [1 :data :data "intensity"])))
                  (is (contains? dependencies (str "/" directory "/nested.gui_script")))
                  (is (contains? dependencies "/builtins/fonts/default.font")))))))))))

(deftest templates-read-without-graph-queries-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path ["/non-editable"])
      (with-clean-system
        (let [workspace (test-util/setup-workspace! project-path)
              basis (g/now)
              read-opts (workspace/make-read-opts basis workspace)
              cases (coll/into-> (:editable->type-ext->resource-type read-opts) []
                      (coll/mapcat
                        (fn [[editable resource-types]]
                          (coll/into-> resource-types :eduction
                            (keep (fn [[ext {:keys [read-fn lazy-loaded] :as resource-type}]]
                                    (when read-fn
                                      (when-let [template ((:template-resource-fn read-opts) resource-type false)]
                                        (let [proj-path (str (if editable "/editable/" "/non-editable/") "template." ext)
                                              file (io/file project-path (subs proj-path 1))]
                                          (with-open [^InputStream input-stream (io/input-stream template)]
                                            (test-support/write-until-new-mtime file (.readAllBytes input-stream)))
                                          {:proj-path proj-path :lazy-loaded lazy-loaded})))))))))]
          (workspace/resource-sync! workspace)
          (let [basis (g/now)
                read-opts (workspace/make-read-opts basis workspace :include-editor-dependencies true)
                resources (mapv #((:proj-path->resource read-opts) (:proj-path %)) cases)
                node-load-infos (read-node-load-infos-without-graph-queries read-opts resources)]
            (is (= (count cases) (count node-load-infos)))
            (doseq [[{:keys [proj-path lazy-loaded]} node-load-info] (mapv coll/pair cases node-load-infos)]
              (testing proj-path
                (is (nil? (:read-error node-load-info)))
                (is (= (not lazy-loaded) (contains? node-load-info :source-value)))
                (when-not lazy-loaded
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
              load-calls (atom [])]
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
                         (swap! load-calls conj (resource/proj-path resource))
                         (g/set-property node-id :value (:value source-value 1)))))
          (doseq [[proj-path source-value] contents]
            (test-util/write-file-resource! workspace proj-path source-value))
          (workspace/resource-sync! workspace)
          (let [project (test-util/setup-project! workspace)]
            (test-fn project workspace load-calls)))))))

;; Prerequisite discovery failures must prevent both the resource and its dependents from loading.
(deftest prerequisite-read-failure-blocks-dependent-load-test
  (call-with-prerequisite-project
    {"/broken.prereq" {:fail true}
     "/dependent.prereq" {:prerequisites ["/broken.prereq"]}
     "/independent.prereq" {:value 42}}
    (fn [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn [project workspace load-calls]
      (let [broken (test-util/resource-node project "/broken.prereq")
            dependent (test-util/resource-node project "/dependent.prereq")
            independent (test-util/resource-node project "/independent.prereq")]
        (is (g/error-value? (g/node-value dependent :result)))
        (is (= {:fail true} (g/node-value broken :source-value)))
        (is (= {:prerequisites ["/broken.prereq"]} (g/node-value dependent :source-value)))
        (is (= [] @load-calls))
        (let [basis (g/now)
              read-opts (workspace/make-read-opts basis workspace)
              resource (resource-node/resource basis broken)
              node-load-info (project/read-node-load-info read-opts broken resource)]
          (is (g/error-value? (:read-error node-load-info)))
          (is (= {:fail true} (:source-value node-load-info)))
          (is (= (resource/resource->sha256-hex resource) (:disk-sha256 node-load-info)))
          (is (not (contains? node-load-info :prerequisite-proj-paths))))
        (is (= 42 (g/node-value independent :result)))
        (is (= ["/independent.prereq"] @load-calls))))))

;; Successful reads retain their hashes so timestamp-only changes keep blocked nodes.
(deftest blocked-resource-unchanged-content-retains-node-test
  (call-with-prerequisite-project
    {"/broken.prereq" {:fail true}
     "/dependent.prereq" {:prerequisites ["/broken.prereq"]}}
    (fn [_read-opts _resource source-value]
      (if (:fail source-value)
        (throw (ex-info "Failed to discover prerequisites." {}))
        (:prerequisites source-value)))
    (fn [project workspace load-calls]
      (is (g/error-value? (g/node-value (test-util/resource-node project "/dependent.prereq") :result)))
      (doseq [proj-path ["/dependent.prereq" "/broken.prereq"]]
        (let [node-id (test-util/resource-node project proj-path)
              resource (resource-node/resource node-id)]
          (is (= (resource/resource->sha256-hex resource) (g/node-value node-id :disk-sha256)))
          (test-support/touch-until-new-mtime (io/as-file resource))
          (workspace/resource-sync! workspace)
          (is (= node-id (test-util/resource-node project proj-path)))
          (is (g/error-value? (g/node-value node-id :result)))))
      (is (= [] @load-calls)))))

;; Resource cycles become read errors with paths, including when different threads own the promises.
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
        (fn [project _workspace load-calls]
          (let [node-ids (mapv #(test-util/resource-node project %) ["/a.prereq" "/b.prereq"])
                contexts (mapv (fn [_] (g/make-evaluation-context)) node-ids)
                results (when concurrent
                          (mapv (fn [node-id context]
                                  (future (g/node-value node-id :result context)))
                                node-ids contexts))]
            (try
              (doseq [[index node-id] (map-indexed vector node-ids)]
                (is (g/error-value? (if concurrent
                                      (deref (results index) 10000 ::timeout)
                                      (g/node-value node-id :result (contexts index))))))
              (doseq [context contexts]
                (g/update-system-from-evaluation-context! context))
              (is (= [] @load-calls))
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

;; Embedded prerequisite discovery failures must reach the owner instead of becoming warnings.
(deftest embedded-prerequisite-read-failure-blocks-load-test
  (call-with-prerequisite-project
    {"/embedded.go" {:embedded-components [{:id "broken" :type "prereq" :data {:fail true}}]}}
    (fn [_read-opts _resource _source-value]
      (throw (ex-info "Failed to discover embedded prerequisites." {})))
    (fn [project _workspace load-calls]
      (let [node-id (test-util/resource-node project "/embedded.go")
            save-value (g/node-value node-id :save-value)
            source-value (g/node-value node-id :source-value)]
        (is (= "broken" (get-in source-value [:embedded-components 0 :id])))
        (is (g/error-value? save-value))
        (is (= [] @load-calls))
        (is (= "broken"
               (:embedded-component-id (ex-data (get-in save-value [:user-data :exception])))))))))

;; Bulk reloads must reject cycles and failed prerequisites before invoking any affected loader.
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
    (fn [project _workspace load-calls]
      (let [nodes-by-resource-path (g/node-value project :nodes-by-resource-path)
            basis (g/now)
            pairs (coll/into-> nodes-by-resource-path []
                    (filter (fn [[proj-path _]] (.endsWith ^String proj-path ".prereq")))
                    (map (fn [[_ node-id]] (coll/pair node-id (resource-node/resource basis node-id)))))
            infos (project/read-nodes pairs :old-node-ids-by-proj-path nodes-by-resource-path)
            infos-by-path (coll/pair-map-by (comp resource/proj-path :resource) infos)]
        (doseq [[proj-path info] infos-by-path]
          (if (= "/independent.prereq" proj-path)
            (is (nil? (:read-error info)))
            (do
              (is (g/error-value? (:read-error info)))
              (is (= (edn/read-string (slurp (:resource info))) (:source-value info)))
              (is (= (resource/resource->sha256-hex (:resource info)) (:disk-sha256 info))))))
        (project/load-nodes! project nil infos progress/null-render-progress! nil {})
        (is (= ["/independent.prereq"] @load-calls))
        (is (= 42 (g/node-value (nodes-by-resource-path "/independent.prereq") :result)))
        (let [dependent (nodes-by-resource-path "/broken-dependent.prereq")
              ;; The broken prerequisite is now loaded, so it is outside this batch.
              infos (project/read-nodes [(coll/pair dependent (resource-node/resource (g/now) dependent))]
                      :old-node-ids-by-proj-path nodes-by-resource-path)]
          (is (= 1 (count infos)))
          (is (g/error-value? (:read-error (first infos)))))))))

;; Prerequisites can be editor-only dependencies and must precede consumers in bulk loads.
(deftest bulk-read-prerequisite-order-test
  (call-with-prerequisite-project
    {"/a.prereq" {:prerequisites ["/b.prereq" "/c.prereq"] :dependencies []}
     "/b.prereq" {:prerequisites ["/d.prereq"] :dependencies []}
     "/c.prereq" {:prerequisites ["/d.prereq"] :dependencies []}
     "/d.prereq" {:value 42}}
    (fn [_read-opts _resource source-value] (:prerequisites source-value))
    (fn [project _workspace load-calls]
      (let [node-ids (g/node-value project :nodes-by-resource-path)
            basis (g/now)
            pairs (mapv (fn [path]
                          (let [node-id (node-ids path)]
                            (coll/pair node-id (resource-node/resource basis node-id))))
                        ["/a.prereq" "/b.prereq" "/c.prereq" "/d.prereq"])
            infos (project/read-nodes pairs :old-node-ids-by-proj-path node-ids)
            order (into {} (map-indexed (fn [index info]
                                          (coll/pair (resource/proj-path (:resource info)) index))) infos)]
        (is (coll/every? #(nil? (:read-error %)) infos))
        (is (= 4 (count infos)))
        (is (< (order "/d.prereq") (order "/b.prereq") (order "/a.prereq")))
        (is (< (order "/d.prereq") (order "/c.prereq") (order "/a.prereq")))
        (project/load-nodes! project nil infos progress/null-render-progress! nil {})
        (is (= 4 (count @load-calls)))))))

;; Missing paths must block both load paths and recover unchanged transitive dependents when created.
(deftest missing-prerequisite-recovery-test
  (doseq [bulk [false true]]
    (call-with-prerequisite-project
      {"/root.prereq" {:prerequisites ["/middle.prereq"] :value 5}
       "/middle.prereq" {:prerequisites ["/missing.prereq"] :value 2}}
      (fn [_read-opts _resource source-value] (:prerequisites source-value))
      (fn [project workspace load-calls]
        (let [node-ids (g/node-value project :nodes-by-resource-path)
              root (node-ids "/root.prereq")
              middle (node-ids "/middle.prereq")]
          (when bulk
            (let [infos (project/read-nodes [(coll/pair root (resource-node/resource root))]
                          :old-node-ids-by-proj-path node-ids)]
              (project/load-nodes! project nil infos progress/null-render-progress! nil {})))
          (let [error (g/node-value root :result)]
            (is (g/error-value? error))
            (is (coll/any? #(and (= :file-not-found (get-in % [:user-data :type]))
                                 (= "/missing.prereq" (some-> (get-in % [:user-data :resource]) resource/proj-path)))
                           (into [] (coll/tree-xf :causes :causes) [error]))))
          (is (= {:prerequisites ["/middle.prereq"] :value 5} (g/node-value root :source-value)))
          (is (= {:prerequisites ["/missing.prereq"] :value 2} (g/node-value middle :source-value)))
          (is (= [] @load-calls))
          (is (nil? (get (g/node-value project :nodes-by-resource-path) "/missing.prereq")))

          (test-util/write-file-resource! workspace "/unrelated.prereq" {:value 9})
          (workspace/resource-sync! workspace)
          (is (= root (test-util/resource-node project "/root.prereq")))
          (is (= middle (test-util/resource-node project "/middle.prereq")))

          (reset! load-calls [])
          (test-util/write-file-resource! workspace "/missing.prereq" {:value 42})
          (workspace/resource-sync! workspace)
          (let [new-root (test-util/resource-node project "/root.prereq")
                new-middle (test-util/resource-node project "/middle.prereq")]
            (is (not= root new-root))
            (is (not= middle new-middle))
            (is (= 5 (g/node-value new-root :result)))
            (is (= 2 (g/node-value new-middle :result)))
            (is (= {"/missing.prereq" 1 "/middle.prereq" 1 "/root.prereq" 1}
                   (frequencies @load-calls)))))))))

;; Blocked files retain searchable source even when non-editable, but hashes must not become search data.
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
                :stateless? false
                :textual? true
                :read-fn (fn [_read-opts _resource input] (slurp input))
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

;; Prerequisite checks must honor supplied resources in both load paths, even without files on disk.
(deftest supplied-prerequisite-resources-test
  (doseq [bulk [false true]]
    (call-with-prerequisite-project
      {}
      (fn [_read-opts _resource source-value] (:prerequisites source-value))
      (fn [_project workspace load-calls]
        (let [project-directory (workspace/project-directory workspace)
              resources (mapv (fn [[filename contents]]
                                (test-util/make-fake-file-resource workspace (.getPath project-directory)
                                                                   (io/file project-directory filename)
                                                                   (.getBytes (pr-str contents) "UTF-8")))
                              [["dependent.prereq" {:prerequisites ["/prerequisite.prereq"] :value 5}]
                               ["prerequisite.prereq" {:value 42}]])
              project (test-util/setup-project! workspace resources)
              node-ids (g/node-value project :nodes-by-resource-path)
              dependent (node-ids "/dependent.prereq")]
          (when bulk
            (let [infos (project/read-nodes (mapv #(coll/pair (node-ids (resource/proj-path %)) %) resources)
                          :old-node-ids-by-proj-path node-ids)]
              (project/load-nodes! project nil infos progress/null-render-progress! nil {})))
          (is (= 5 (g/node-value dependent :result)))
          (is (= ["/prerequisite.prereq" "/dependent.prereq"] @load-calls)))))))

;; Failed prerequisite metadata must survive discovery errors and cycles so either can be repaired.
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
      (fn [project workspace load-calls]
        (when bulk
          (let [root (test-util/resource-node project "/root.prereq")
                infos (project/read-nodes [(coll/pair root (resource-node/resource root))]
                        :old-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path))]
            (project/load-nodes! project nil infos progress/null-render-progress! nil {})))
        (is (g/error-value? (g/node-value (test-util/resource-node project "/root.prereq") :result)))
        (is (= [] @load-calls))
        (test-util/write-file-resource! workspace "/b.prereq" {:value 42})
        (workspace/resource-sync! workspace)
        (is (= 1 (g/node-value (test-util/resource-node project "/root.prereq") :result)))
        (is (= {"/b.prereq" 1 "/a.prereq" 1 "/root.prereq" 1} (frequencies @load-calls)))))))

;; Loading a missing game object must not produce a save value that silently drops its overrides.
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

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
        (is (g/error-value? (g/node-value broken :source-value)))
        (is (g/error-value? (g/node-value dependent :source-value)))
        (is (= [] @load-calls))
        (let [basis (g/now)
              read-opts (workspace/make-read-opts basis workspace)
              resource (resource-node/resource basis broken)
              node-load-info (project/read-node-load-info read-opts broken resource)]
          (is (g/error-value? (:read-error node-load-info)))
          (is (not (contains? node-load-info :source-value)))
          (is (not (contains? node-load-info :prerequisite-proj-paths))))
        (is (= 42 (g/node-value independent :result)))
        (is (= ["/independent.prereq"] @load-calls))))))

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
              (let [exceptions (mapv #(get-in (g/node-value % :source-value) [:user-data :exception]) node-ids)
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
        (is (g/error-value? source-value))
        (is (g/error-value? save-value))
        (is (= [] @load-calls))
        (is (= "broken"
               (:embedded-component-id (ex-data (get-in source-value [:user-data :exception])))))))))

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
              (is (not (contains? info :source-value))))))
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

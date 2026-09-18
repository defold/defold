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
  (:require [clojure.java.io :as io]
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
  (:import [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn- unexpected-graph-query [& _args]
  (throw (AssertionError. "Graph queries are not allowed from this context.")))

(defmacro ^:private without-graph-queries [& body]
  `(with-redefs [g/now unexpected-graph-query
                 g/unsafe-basis unexpected-graph-query
                 g/node-value unexpected-graph-query
                 g/make-evaluation-context unexpected-graph-query]
     ~@body))

(defn- read-node-load-infos-without-graph-queries [read-opts resources]
  (let [node-id+resource-pairs (mapv coll/pair (g/take-node-ids (count resources)) resources)]
    (without-graph-queries
      (project/read-node-load-infos read-opts node-id+resource-pairs (count resources) progress/null-render-progress!))))

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
                read-opts (workspace/make-read-opts basis workspace :include-editor-dependencies true)]
            (doseq [directory ["editable" "non-editable"]
                    :let [resources (mapv #((:proj-path->resource read-opts) (str "/" directory "/test." (first %))) cases)
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
              (without-graph-queries
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

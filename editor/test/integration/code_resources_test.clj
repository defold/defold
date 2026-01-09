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

(ns integration.code-resources-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.resource :as code.resource]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(defn- all-file-resources
  ([workspace]
   (g/with-auto-evaluation-context evaluation-context
     (all-file-resources workspace evaluation-context)))
  ([workspace evaluation-context]
   (->> (g/node-value workspace :resource-list evaluation-context)
        (filter (fn [resource]
                  (and (resource/file-resource? resource)
                       (= :file (resource/source-type resource)))))
        (sort-by resource/proj-path)
        (vec))))

(defn- save-tracked-resources [project]
  (let [basis (g/now)]
    (->> (g/sources-of basis project :save-data)
         (map (fn [[resource-node-id]]
                (resource-node/resource basis resource-node-id)))
         (sort-by resource/proj-path)
         (vec))))

(deftest lazy-loaded-resources-tracked-by-save-system
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [editable-lazy-loaded-file-proj-paths
          (into (sorted-set)
                (comp (filter resource/editable?)
                      (filter #(:lazy-loaded (resource/resource-type %)))
                      (map resource/proj-path))
                (all-file-resources workspace))

          save-tracked-proj-paths
          (into (sorted-set)
                (map resource/proj-path)
                (save-tracked-resources project))]

      (is (= editable-lazy-loaded-file-proj-paths
             (set/intersection editable-lazy-loaded-file-proj-paths save-tracked-proj-paths))))))

(defn- set-code-resource-node-lines! [lines-by-code-resource-node-id]
  (g/transact
    (for [[node-id lines] lines-by-code-resource-node-id]
      (test-util/set-code-editor-lines node-id lines))))

(deftest code-resources-dirty-test
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/reload_unchanged_project")
          project (test-util/setup-project! workspace)
          project-graph-id (g/node-id->graph-id project)

          editable-code-resource-node-ids
          (g/with-auto-evaluation-context evaluation-context
            (let [basis (:basis evaluation-context)]
              (into []
                    (comp (filter resource/editable?)
                          (map #(project/get-resource-node project % evaluation-context))
                          (filter #(g/node-instance? basis code.resource/CodeEditorResourceNode %)))
                    (all-file-resources workspace evaluation-context))))

          original-lines-by-code-resource-node-id
          (into {}
                (map (fn [node-id]
                       (let [lines (test-util/code-editor-lines node-id)]
                         [node-id lines])))
                editable-code-resource-node-ids)

          edited-lines-by-code-resource-node-id
          (into {}
                (map (fn [[node-id lines]]
                       [node-id (conj lines "")]))
                original-lines-by-code-resource-node-id)

          node-dirty? #(g/node-value % :dirty)
          all-dirty? #(every? node-dirty? editable-code-resource-node-ids)
          all-clean? #(not-any? node-dirty? editable-code-resource-node-ids)]

      (is (all-clean?) "Clean after loading.")
      (set-code-resource-node-lines! edited-lines-by-code-resource-node-id)
      (is (all-dirty?) "Dirty after edit.")
      (g/undo! project-graph-id)
      (is (all-clean?) "Clean after edit -> undo.")
      (g/redo! project-graph-id)
      (is (all-dirty?) "Dirty after edit -> undo -> redo.")
      (test-util/save-project! project)
      (is (all-clean?) "Clean after edit -> save.")
      (g/undo! project-graph-id)
      (is (all-dirty?) "Dirty after edit -> save -> undo.")
      (g/redo! project-graph-id)
      (is (all-clean?) "Clean after edit -> save -> undo -> redo.")
      (set-code-resource-node-lines! original-lines-by-code-resource-node-id)
      (is (all-dirty?) "Dirty after edit -> save -> paste-original"))))

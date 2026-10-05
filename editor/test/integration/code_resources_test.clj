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
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.digest :as digest])
  (:import [clojure.lang ExceptionInfo]))

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
    (->> (g/inputs basis project :save-data)
         (map (fn [arc]
                (resource-node/resource basis (gt/source-id arc))))
         (sort-by resource/proj-path)
         (vec))))

(deftest lazy-loaded-resources-tracked-after-materialization
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [editable-lazy-loaded-file-proj-paths
          (coll/into-> (all-file-resources workspace) (sorted-set)
            (filter resource/editable?)
            (filter #(:lazy-loaded (resource/resource-type %)))
            (map resource/proj-path))

          tracked-proj-paths
          (fn tracked-proj-paths []
            (coll/into-> (save-tracked-resources project) #{}
              (map resource/proj-path)))]

      (g/node-value project :save-data)
      (is (coll/empty? (set/intersection editable-lazy-loaded-file-proj-paths (tracked-proj-paths))))
      (doseq [proj-path editable-lazy-loaded-file-proj-paths]
        (let [node-id (test-util/resource-node project proj-path)]
          (is (not (resource-node/loaded? (g/now) node-id)))
          (g/with-auto-evaluation-context evaluation-context
            (g/materialize-node! node-id evaluation-context))
          (is (contains? (tracked-proj-paths) proj-path))))
      (is (= editable-lazy-loaded-file-proj-paths
             (set/intersection editable-lazy-loaded-file-proj-paths (tracked-proj-paths)))))))

(defn- set-code-resource-node-lines! [lines-by-code-resource-node-id]
  (g/transact
    (for [[node-id lines] lines-by-code-resource-node-id]
      (test-util/set-code-editor-lines node-id lines))))

(deftest code-resources-dirty-test
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/reload_unchanged_project")
          project (test-util/setup-project! workspace)

          editable-code-resource-node-ids
          (g/with-auto-evaluation-context evaluation-context
            (let [basis (g/ec-basis evaluation-context)]
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
      (g/undo! :undo/global)
      (is (all-clean?) "Clean after edit -> undo.")
      (g/redo! :undo/global)
      (is (all-dirty?) "Dirty after edit -> undo -> redo.")
      (test-util/save-project! project)
      (is (all-clean?) "Clean after edit -> save.")
      (g/undo! :undo/global)
      (is (all-dirty?) "Dirty after edit -> save -> undo.")
      (g/redo! :undo/global)
      (is (all-clean?) "Clean after edit -> save -> undo -> redo.")
      (set-code-resource-node-lines! original-lines-by-code-resource-node-id)
      (is (all-dirty?) "Dirty after edit -> save -> paste-original"))))

(deftest lazy-disk-state-transaction-test
  ;; Verifies that lazy disk baselines are transactional, guarding against dry
  ;; runs and failed edits changing the live resource or previously captured
  ;; graph state.
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [node-id (test-util/resource-node project "/editable/json.json")
          original-lines (test-util/code-editor-lines node-id)
          edited-lines (conj original-lines "")
          resource (resource-node/resource node-id)
          disk-sha256 (resource/resource->sha256-hex resource)
          evaluation-context (g/make-evaluation-context)
          original-save-data (g/node-value node-id :save-data evaluation-context)]

      (testing "Generating and dry-running the first edit leaves the live baseline unset."
        (let [{:keys [basis]}
              (g/transact {:dry-run true}
                (test-util/set-code-editor-lines node-id edited-lines))]
          (is (= original-lines (g/raw-property-value basis node-id :unmodified-lines)))
          (is (= (hash original-lines) (g/raw-property-value basis node-id :source-value)))
          (is (= disk-sha256 (g/raw-property-value basis node-id :disk-sha256)))
          (is (nil? (g/node-value node-id :unmodified-lines)))
          (is (nil? (g/node-value node-id :source-value)))
          (is (nil? (g/node-value node-id :disk-sha256)))
          (is (= original-save-data (g/node-value node-id :save-data)))))

      (testing "A failed edit does not initialize the live disk baseline."
        (is (thrown-with-msg?
              ExceptionInfo #"Abort disk-state test"
              (g/transact
                [(test-util/set-code-editor-lines node-id edited-lines)
                 (g/update-property
                   node-id :modified-lines
                   (fn [_lines]
                     (throw (ex-info "Abort disk-state test" {}))))])))
        (is (nil? (g/node-value node-id :unmodified-lines)))
        (is (nil? (g/node-value node-id :source-value)))
        (is (nil? (g/node-value node-id :disk-sha256))))

      (testing "Committing the edit initializes the baseline and invalidates cached save data."
        (g/transact
          (test-util/set-code-editor-lines node-id edited-lines))
        (is (= original-lines (g/node-value node-id :unmodified-lines)))
        (is (= (hash original-lines) (g/node-value node-id :source-value)))
        (is (= disk-sha256 (g/node-value node-id :disk-sha256)))
        (is (= edited-lines (:save-value (g/node-value node-id :save-data))))
        (is (true? (g/node-value node-id :dirty))))

      (testing "The earlier evaluation context still observes the uninitialized baseline."
        (is (nil? (g/node-value node-id :unmodified-lines evaluation-context)))
        (is (nil? (g/node-value node-id :source-value evaluation-context)))
        (is (nil? (g/node-value node-id :disk-sha256 evaluation-context)))
        (is (= original-save-data (g/node-value node-id :save-data evaluation-context)))))))

(deftest lazy-disk-state-save-and-undo-test
  ;; Verifies that loading invalidates cached save data without adding undo
  ;; entries, and saving preserves the original text needed to undo the first
  ;; edit.
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/reload_unchanged_project")
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/editable/json.json")
          original-lines (test-util/code-editor-lines node-id)
          edited-lines (conj original-lines "")]
      (is (nil? (:save-value (g/node-value node-id :save-data))))
      (g/reset-undo! :undo/global)
      (g/with-auto-evaluation-context evaluation-context
        (code.resource/ensure-loaded! node-id evaluation-context))
      (is (zero? (g/undo-stack-count :undo/global)))
      (is (= original-lines (:save-value (g/node-value node-id :save-data))))
      (test-util/set-code-editor-lines! node-id edited-lines)
      (test-util/save-project! project)

      (let [saved-disk-sha256 (resource/resource->sha256-hex (resource-node/resource node-id))]
        (is (= 1 (g/undo-stack-count :undo/global)))
        (is (= original-lines (g/node-value node-id :unmodified-lines)))
        (is (= (hash edited-lines) (g/node-value node-id :source-value)))
        (is (= saved-disk-sha256 (g/node-value node-id :disk-sha256)))
        (is (false? (g/node-value node-id :dirty)))

        (g/undo! :undo/global)
        (is (= original-lines (test-util/code-editor-lines node-id)))
        (is (= (hash edited-lines) (g/node-value node-id :source-value)))
        (is (= saved-disk-sha256 (g/node-value node-id :disk-sha256)))
        (is (true? (g/node-value node-id :dirty)))

        (g/redo! :undo/global)
        (is (= edited-lines (test-util/code-editor-lines node-id)))
        (is (false? (g/node-value node-id :dirty)))))))

(deftest resource-disk-baseline-properties-test
  ;; Verifies hash invalidation and snapshot isolation for resources without a
  ;; save value, and preserves disk-baseline access on defective or missing
  ;; nodes.
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [resource (resource-node/resource (test-util/resource-node project "/editable/json.json"))
          [node-id] (g/tx-nodes-added
                      (g/transact {:undoable false}
                        (g/make-node resource-node/ResourceNode :resource resource)))
          initial-sha256 (digest/string->sha256-hex "initial")
          saved-sha256 (digest/string->sha256-hex "saved")]
      (g/transact {:undoable false}
        (resource-node/set-disk-sha256 node-id initial-sha256))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= initial-sha256 (g/node-value node-id :sha256)))
        (is (contains? (g/cache) (g/endpoint node-id :sha256)))
        (is (= initial-sha256 (g/node-value node-id :sha256 evaluation-context)))
        (g/transact {:undoable false}
          (resource-node/set-disk-sha256 node-id saved-sha256))
        (is (not (contains? (g/cache) (g/endpoint node-id :sha256))))
        (is (= saved-sha256 (g/node-value node-id :sha256)))
        (is (= initial-sha256 (g/node-value node-id :sha256 evaluation-context)))
        (g/transact
          [(g/set-property node-id :loaded true)
           ;; False is a valid source-value, distinct from an uninitialized nil
           ;; baseline, and must survive undo and the node becoming defective.
           (resource-node/merge-source-values [[node-id false]])])
        (g/undo! :undo/global)
        (is (false? (g/node-value node-id :loaded)))
        (is (= saved-sha256 (g/node-value node-id :sha256)))
        (is (false? (g/node-value node-id :source-value))))

      (g/transact {:undoable false}
        (g/mark-defective node-id :defective))
      (is (false? (g/node-value node-id :source-value)))
      (is (= saved-sha256 (g/node-value node-id :disk-sha256)))

      (let [read-error (resource-io/invalid-content-error node-id :source-value :fatal resource (ex-info "Invalid content" {}))]
        (g/transact
          (resource-node/merge-source-values [[node-id read-error]]))
        (is (= read-error (g/node-value node-id :source-value))))

      (g/transact {:undoable false}
        (g/set-property node-id :resource (workspace/resolve-workspace-resource workspace "/missing.json")))
      (is (resource-io/file-not-found-error? (g/node-value node-id :source-value))))))

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
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.resource :as code.resource]
            [editor.defold-project :as project]
            [editor.defold-project-search :as project-search]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.digest :as digest])
  (:import [clojure.lang ExceptionInfo]
           [internal.node NodeImpl ShellNode]))

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

;; Save tracking starts when an editable code resource materializes.
(deftest code-resources-tracked-after-materialization
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [editable-code-proj-paths
          (coll/into-> (all-file-resources workspace) (sorted-set)
            (filter resource/editable?)
            (filter #(code.resource/code-resource-type? (resource/resource-type %)))
            (filter resource/textual?)
            (map resource/proj-path))

          tracked-proj-paths
          (fn tracked-proj-paths []
            (coll/into-> (save-tracked-resources project) #{}
              (map resource/proj-path)))]

      (g/node-value project :save-data)
      (is (coll/empty? (set/intersection editable-code-proj-paths (tracked-proj-paths))))
      (doseq [proj-path editable-code-proj-paths]
        (let [node-id (test-util/resource-node project proj-path)]
          (is (not (resource-node/loaded? (g/now) node-id)))
          (g/with-auto-evaluation-context evaluation-context
            (g/materialize-node! node-id evaluation-context))
          (is (contains? (tracked-proj-paths) proj-path))))
      (is (= editable-code-proj-paths
             (set/intersection editable-code-proj-paths (tracked-proj-paths)))))))

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
            (let [{:keys [basis-ref]} evaluation-context]
              (into []
                    (comp (filter resource/editable?)
                          (map #(project/get-resource-node project % evaluation-context))
                          (filter #(g/node-instance? @basis-ref code.resource/CodeEditorResourceNode %)))
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

;; An indentation change is undoable and leaves the saved text clean.
(deftest code-indent-type-edit-test
  (test-util/with-temp-project-content
    {"/example.lua" "if true then\n    print(1)\nend"}
    (let [node-id (test-util/resource-node project "/example.lua")
          lines (g/node-value node-id :lines)]
      (is (= :four-spaces (g/node-value node-id :indent-type)))
      (g/set-property! node-id :indent-type :two-spaces)
      (is (= :two-spaces (g/node-value node-id :indent-type)))
      (is (= lines (g/node-value node-id :save-value)))
      (is (false? (g/node-value node-id :dirty)))
      (g/undo! :undo/global)
      (is (= :four-spaces (g/node-value node-id :indent-type))))))

;; Code search retains raw lines and disk-sha256 on shells without materializing
;; them, including for library files. Searches, loading, opening, and edits
;; reuse the read. Loading adds no undo entry, and saving preserves the
;; original text needed to undo the first edit.
(deftest code-search-caches-shell-reads-test
  (doseq [editable [false true]]
    (testing (str "editable: " editable)
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (when-not editable
            (test-util/set-non-editable-directories! project-path ["/resources"]))
          (with-clean-system
            (let [workspace (test-util/setup-workspace! project-path)
                  proj-path "/resources/searched.codecache"
                  lines ["if true then" "    print('needle')" "end"]]
              (g/transact
                (code.resource/register-code-resource-type workspace
                  :ext "codecache"
                  :editable editable
                  :node-type code.resource/CodeEditorResourceNode))
              (test-util/write-file-resource! workspace proj-path lines)
              (workspace/resource-sync! workspace)
              (let [project (test-util/setup-project! workspace)
                    node-id (test-util/resource-node project proj-path)
                    resource (resource-node/resource node-id)
                    disk-sha256 (resource/resource->sha256-hex resource)
                    search-fn (:search-fn (resource/resource-type resource))
                    file (io/file project-path (subs proj-path 1))]
                (g/reset-undo! :undo/global)
                (testing "Failed reads leave the shell available for another attempt"
                  (io/delete-file file)
                  (g/with-auto-evaluation-context evaluation-context
                    (is (g/error-value? (project-search/search-value node-id resource evaluation-context))))
                  (let [node (g/node-by-id (g/now) node-id)]
                    (is (instance? ShellNode node))
                    (is (not (contains? node :source-value)))
                    (is (not (contains? node :disk-sha256)))))
                (test-util/write-file-resource! workspace proj-path lines)
                (let [search-value (g/with-auto-evaluation-context evaluation-context
                                     (project-search/search-value node-id resource evaluation-context))
                      node (g/node-by-id (g/now) node-id)]
                  (is (= lines search-value))
                  (is (instance? ShellNode node))
                  (is (= lines (:source-value node)))
                  (is (= disk-sha256 (:disk-sha256 node)))
                  ;; Removing the file exposes redundant disk reads.
                  (io/delete-file file)
                  (g/with-auto-evaluation-context evaluation-context
                    (is (= search-value (project-search/search-value node-id resource evaluation-context))))
                  (doseq [search-string ["needle" "print"]]
                    (is (= ["    print('needle')"]
                           (mapv :text (search-fn search-value (search-fn search-string)))))))
                (is (instance? ShellNode (g/node-by-id (g/now) node-id)))
                (g/with-auto-evaluation-context evaluation-context
                  (g/materialize-node! node-id evaluation-context))
                (is (instance? NodeImpl (g/node-by-id (g/now) node-id)))
                (is (= lines (g/node-value node-id :lines)))
                (is (= :four-spaces (g/node-value node-id :indent-type)))
                (is (= (when editable lines) (g/node-value node-id :source-value)))
                (is (= disk-sha256 (g/node-value node-id :disk-sha256)))
                (is (= lines (g/node-value node-id :save-value)))
                (is (false? (g/node-value node-id :dirty)))
                (is (zero? (g/undo-stack-count :undo/global)))
                (when editable
                  (let [edited-lines (conj lines "-- unsaved")]
                    (test-util/set-code-editor-lines! node-id edited-lines)
                    (is (true? (g/node-value node-id :dirty)))
                    (g/with-auto-evaluation-context evaluation-context
                      (is (= edited-lines (project-search/search-value node-id resource evaluation-context))))
                    (test-util/write-file-resource! workspace proj-path lines)
                    (test-util/save-project! project)
                    (is (false? (g/node-value node-id :dirty)))
                    (is (= 1 (g/undo-stack-count :undo/global)))
                    (g/undo! :undo/global)
                    (is (= lines (g/node-value node-id :save-value)))
                    (is (= edited-lines (g/node-value node-id :source-value)))
                    (is (true? (g/node-value node-id :dirty)))
                    (g/redo! :undo/global)
                    (is (= edited-lines (g/node-value node-id :save-value)))
                    (is (false? (g/node-value node-id :dirty)))))))))))))

;; A tentative first edit reads its baseline within the transaction. Discarding
;; or aborting the transaction must leave the live resource as an unread shell.
;; Committing an edit must also preserve the shell in previously captured bases.
(deftest first-code-edit-is-transactional-test
  (test-util/with-temp-project-content {"/example.json" "[]"}
    (let [node-id (test-util/resource-node project "/example.json")
          resource (resource-node/resource node-id)
          disk-sha256 (resource/resource->sha256-hex resource)
          initial-basis (g/now)
          edit (g/set-property node-id :lines ["[1]"])]
      (let [{:keys [basis]} (g/transact {:dry-run true} edit)]
        (is (= ["[]"] (g/raw-property-value basis node-id :source-value)))
        (is (= disk-sha256 (g/raw-property-value basis node-id :disk-sha256)))
        (is (= ["[1]"] (g/raw-property-value basis node-id :lines)))
        (is (g/shell-node? (g/node-by-id node-id))))
      (is (thrown-with-msg? ExceptionInfo #"Abort first edit"
                            (g/transact
                              [edit
                               (g/callback (fn abort-edit [] (throw (ex-info "Abort first edit" {}))))])))
      (is (g/shell-node? (g/node-by-id node-id)))
      (is (nil? (g/node-value node-id :source-value)))
      (g/transact edit)
      (is (= ["[]"] (g/node-value node-id :source-value)))
      (is (= disk-sha256 (g/node-value node-id :disk-sha256)))
      (is (= ["[1]"] (:save-value (g/node-value node-id :save-data))))
      (is (true? (g/node-value node-id :dirty)))
      (is (g/shell-node? (g/node-by-id initial-basis node-id)))
      (is (nil? (g/raw-property-value initial-basis node-id :source-value))))))

;; Equal hashes for different lines must still produce a dirty edit. Saving
;; advances the disk baseline independently of undoing and redoing that edit.
(deftest code-save-and-undo-with-colliding-line-hashes-test
  (test-util/with-temp-project-content {"/example.cpp" "Aa"}
    (let [node-id (test-util/resource-node project "/example.cpp")
          resource (resource-node/resource node-id)]
      (is (= (hash ["Aa"]) (hash ["BB"])))
      (g/reset-undo! :undo/global)
      (g/set-property! node-id :lines ["BB"])
      (is (true? (g/node-value node-id :dirty)))
      (test-util/save-project! project)
      (is (= "BB" (slurp resource)))
      (is (false? (g/node-value node-id :dirty)))
      (is (= 1 (g/undo-stack-count :undo/global)))
      (g/undo! :undo/global)
      (is (= ["Aa"] (g/node-value node-id :lines)))
      (is (= ["BB"] (g/node-value node-id :source-value)))
      (is (= (resource/resource->sha256-hex resource) (g/node-value node-id :disk-sha256)))
      (is (true? (g/node-value node-id :dirty)))
      (g/redo! :undo/global)
      (is (= ["BB"] (g/node-value node-id :lines)))
      (is (false? (g/node-value node-id :dirty))))))

;; Project search reads unknown text files from their shells. Only text
;; placeholders acquire editor lines and enter save tracking on materialization.
(deftest placeholder-code-content-test
  (test-util/with-temp-project-content {"/text.unknown" "text" "/binary.unknown" (byte-array 2048)}
    (let [text-node (test-util/resource-node project "/text.unknown")
          binary-node (test-util/resource-node project "/binary.unknown")

          search-data
          (test-util/with-ui-run-later-rebound
            @(project-search/make-search-data-future
               #(throw %)
               project
               #(= "unknown" (resource/type-ext %))))]
      (is (= [{:resource (resource-node/resource text-node) :search-value ["text"]}] search-data))
      (is (g/shell-node? (g/node-by-id text-node)))
      (is (g/shell-node? (g/node-by-id binary-node)))
      (is (= ["text"] (g/node-value text-node :save-value)))
      (is (nil? (g/node-value binary-node :save-value)))
      (is (nil? (g/node-value binary-node :source-value)))
      (is (nil? (g/node-value binary-node :disk-sha256)))
      (is (g/connected? (g/now) text-node :save-data project :save-data))
      (is (not (g/connected? (g/now) binary-node :save-data project :save-data))))))

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

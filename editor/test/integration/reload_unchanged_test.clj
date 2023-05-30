;; Copyright 2020-2023 The Defold Foundation
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

(ns integration.reload-unchanged-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.resource :as code.resource]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.resource-update :as resource-update]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [touch-until-new-mtime valid-node-value with-clean-system]]))

(set! *warn-on-reflection* true)

(defn- folder-file-resource? [resource]
  (and (resource/file-resource? resource)
       (= :folder (resource/source-type resource))))

(defn- all-file-resources [workspace]
  (->> (workspace/resolve-workspace-resource workspace "/")
       (tree-seq folder-file-resource? resource/children)
       (filter (fn [resource]
                 (and (= :file (resource/source-type resource))
                      (not (resource/internal? resource)))))))

(defn- touch-resources! [resources]
  (doseq [resource resources]
    (touch-until-new-mtime (io/as-file resource)))
  (when-some [workspace (some resource/workspace resources)]
    (workspace/resource-sync! workspace)))

(defn resources->proj-paths [resources]
  (->> resources
       (map resource/proj-path)
       (sort)
       (vec)))

(defn resource-node-ids->proj-paths [resource-node-ids]
  (->> resource-node-ids
       (map resource-node/resource)
       (resources->proj-paths)))

(defmulti ^:private edit-resource-node
  (fn [node-id]
    (if (g/node-instance? code.resource/CodeEditorResourceNode node-id)
      :code
      (resource/type-ext (resource-node/resource node-id)))))

(defmethod edit-resource-node :code [node-id]
  (test-util/update-code-editor-lines node-id conj ""))

(defmethod edit-resource-node "atlas" [node-id]
  (g/set-property node-id :margin 4))

(defmethod edit-resource-node "collection" [node-id]
  (g/delete-node (test-util/first-subnode-of-type node-id collection/InstanceNode)))

(defmethod edit-resource-node "font" [node-id]
  (g/set-property node-id :render-mode :mode-multi-layer))

(defmethod edit-resource-node "go" [node-id]
  (g/delete-node (test-util/first-subnode-of-type node-id game-object/ComponentNode)))

(defmethod edit-resource-node "input_binding" [node-id]
  (g/update-property node-id :pb assoc-in [:mouse-trigger 0 :input] :mouse-button-2))

(defmethod edit-resource-node "project" [node-id]
  (test-util/set-setting node-id ["project" "title"] "New Title"))

(defmethod edit-resource-node "shared_editor_settings" [node-id]
  (test-util/set-setting node-id ["performance" "cache_capacity"] 12345))

(defmethod edit-resource-node "sprite" [node-id]
  (g/set-property node-id :blend-mode :blend-mode-mult))

(deftest keep-existing-nodes-test
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/reload_unchanged_project")
          project (test-util/setup-project! workspace)
          resource-change-plans-atom (atom [])
          file-resources (all-file-resources workspace)

          stateful-file-resources
          (into #{}
                (remove #(:stateless? (resource/resource-type %)))
                file-resources)

          ;; We expect the outputs on all stateless resource nodes to be
          ;; invalidated. It would be nice to only do this if their contents
          ;; have changed, but there is no way for us to tell if they've been
          ;; modified or not, since we don't keep any state for them at all.
          stateless-resource-node-ids
          (sort
            (eduction
              (remove stateful-file-resources)
              (map #(project/get-resource-node project %))
              file-resources))

          expect-reloaded
          (fn expect-reloaded [reloaded-proj-paths]
            {:kept (filterv (complement (set reloaded-proj-paths))
                            (resources->proj-paths stateful-file-resources))
             :reloaded reloaded-proj-paths
             :invalidated (resource-node-ids->proj-paths stateless-resource-node-ids)})

          resource-changes
          (fn resource-changes [resource-change-plan]
            {:kept (resources->proj-paths (:kept resource-change-plan))
             :reloaded (resources->proj-paths (:new resource-change-plan))
             :invalidated (resource-node-ids->proj-paths (:invalidate-outputs resource-change-plan))})]

      ;; Add a resource listener, so we can get the resulting resource change plans.
      (workspace/add-resource-listener!
        workspace 1
        (reify resource/ResourceListener
          (handle-changes [_this changes _render-progress!]
            (let [nodes-by-resource-path (valid-node-value project :nodes-by-resource-path)
                  resource-change-plan (resource-update/resource-change-plan nodes-by-resource-path changes)]
              (swap! resource-change-plans-atom conj resource-change-plan)))))

      (testing "Nodes aren't reloaded unless the contents of the file has changed."
        ;; Touch all the files in the project.
        (touch-resources! (all-file-resources workspace))

        ;; Ensure we have no significant changes to the graph in the resource change plan.
        (when (is (= 1 (count @resource-change-plans-atom)))
          ;; These files are all lazy-loaded, so they will have no on-disk state
          ;; to compare against yet. Thus, we expect their nodes to be replaced.
          (is (= (expect-reloaded ["/.gitattributes"
                                   "/.gitignore"
                                   "/editable/json.json"
                                   "/editable/script_api.script_api"
                                   "/editable/xml.xml"
                                   "/non-editable/json.json"
                                   "/non-editable/script_api.script_api"
                                   "/non-editable/xml.xml"])
                 (resource-changes (@resource-change-plans-atom 0))))))

      (testing "Unsaved edits aren't lost unless the file changed on disk."
        (let [editable-resource-node-ids
              (mapv first (g/sources-of project :save-data))

              editable-resources
              (->> editable-resource-node-ids
                   (map resource-node/resource)
                   (sort-by resource/proj-path))]

          ;; Perform unsaved edits on all editable files in the project.
          (g/transact
            (into []
                  (mapcat edit-resource-node)
                  editable-resource-node-ids))

          ;; Sanity-check: Ensure all editable files are now considered dirty.
          ;; If this fails, our tests here aren't covering everything.
          (let [editable-proj-paths (into (sorted-set)
                                          (map resource/proj-path)
                                          editable-resources)
                dirty-proj-paths (into (sorted-set)
                                       (map (comp resource/proj-path :resource))
                                       (project/dirty-save-data project))]
            (is (= #{} (set/difference editable-proj-paths dirty-proj-paths))))

          ;; Touch all the files in the project.
          (touch-resources! (all-file-resources workspace))

          ;; Ensure we have no significant changes to the graph in the resource change plan.
          (when (is (= 2 (count @resource-change-plans-atom)))
            ;; Remember, these files are all lazy-loaded, so unless they have
            ;; been edited, they will have no on-disk state to compare against.
            ;; Since we've not made edits to any of the non-editable files, they
            ;; remain in the list, but the edited lazy-loaded files do not.
            (is (= (expect-reloaded ["/non-editable/json.json"
                                     "/non-editable/script_api.script_api"
                                     "/non-editable/xml.xml"])
                   (resource-changes (@resource-change-plans-atom 1))))))))))

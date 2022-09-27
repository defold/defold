;; Copyright 2020-2022 The Defold Foundation
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

(ns integration.spine-extension-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build-errors-view :as build-errors-view]
            [editor.collection-data :as collection-data]
            [editor.defold-project :as project]
            [editor.diff-view :as diff-view]
            [editor.game-project :as game-project]
            [editor.gui :as gui]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(defonce ^:private extension-spine-url "https://github.com/defold/extension-spine/archive/main.zip")

(def ^:private error-item-open-info-without-opts (comp pop build-errors-view/error-item-open-info))

(defn- save-data-content-by-proj-path [project]
  (into {}
        (map (juxt (comp resource/proj-path :resource)
                   :content))
        (project/all-save-data project)))

(defn- diff-lines-range [{:keys [begin end]} text]
  (into []
        (comp (drop begin)
              (take (- end begin)))
        (string/split-lines text)))

(defn- diff-save-data-content-by-proj-path [save-data-content-by-proj-path-before save-data-content-by-proj-path-after]
  (into {}
        (keep (fn [[proj-path save-data-content-after]]
                (let [save-data-content-before (save-data-content-by-proj-path-before proj-path)
                      differences (filter (comp (partial not= :nop) :type)
                                          (diff-view/diff save-data-content-before save-data-content-after))]
                  (when-not (empty? differences)
                    [proj-path (mapv (fn [difference]
                                       (case (:type difference)
                                         :replace (-> difference
                                                      (update :left diff-lines-range save-data-content-before)
                                                      (update :right diff-lines-range save-data-content-after))
                                         difference))
                                     differences)]))))
        save-data-content-by-proj-path-after))

(deftest legacy-spine-project-user-migration-test
  ;; Clear custom gui scene loaders to ensure a clean test.
  (gui/clear-custom-gui-scene-loaders-for-tests!)

  ;; Load the unmigrated project to check that the editor won't corrupt it. Then
  ;; add a dependency to the extension-spine library and reload the project.
  (let [migrated-game-project-content
        (test-util/with-loaded-project "test/resources/spine_migration_project" :logging-suppressed true
          (let [main-collection-resource (workspace/find-resource workspace "/main/main.collection")
                main-gui-resource (workspace/find-resource workspace "/main/main.gui")
                main-collection (test-util/resource-node project main-collection-resource)
                main-gui (test-util/resource-node project main-gui-resource)]
            (testing "Without the extension, resources with embedded Spine data cannot be edited."
              (is (resource-node/defective? main-collection))
              (is (resource-node/defective? main-gui)))
            (testing "Without the extension, resources with embedded Spine data aren't connected to the save-data system, and cannot be corrupted by save."
              (is (not-any? #{main-collection-resource main-gui-resource}
                            (map :resource (project/all-save-data project)))))
            (testing "Without the extension, resources with embedded Spine data report build errors due to invalid content."
              (letfn [(invalid-content-error? [error-resource-path error-value]
                        (is (g/error? error-value))
                        (is (= :invalid-content (-> error-value :user-data :type)))
                        (let [error-tree (build-errors-view/build-resource-tree error-value)
                              error-item-of-parent-resource (first (:children error-tree))
                              error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                          (is (= :resource (:type error-item-of-parent-resource)))
                          (is (= (str "The file '" error-resource-path "' could not be loaded.")
                                 (:message error-item-of-faulty-node)))))]
                (is (invalid-content-error? "/main/main.collection" (test-util/build-error! main-collection)))
                (is (invalid-content-error? "/main/main.gui" (test-util/build-error! main-gui))))))
          ;; Before unloading the project, generate the content for a migrated
          ;; game.project file that adds a dependency to extension-spine.
          (let [dependencies-setting-path ["project" "dependencies"]
                game-project (test-util/resource-node project "/game.project")
                old-dependencies (game-project/get-setting game-project dependencies-setting-path)
                new-dependencies (conj (vec old-dependencies) extension-spine-url)]
            (game-project/set-setting! game-project dependencies-setting-path new-dependencies)
            (let [migrated-game-project-save-data (g/node-value game-project :save-data)]
              (is (not (g/error? migrated-game-project-save-data)))
              (:content migrated-game-project-save-data))))]
    (testing "Manual migration steps."
      (test-support/with-clean-system
        (let [workspace (test-util/setup-scratch-workspace! world "test/resources/spine_migration_project")]
          ;; Add a dependency to extension-spine to game.project
          (let [game-project-file (io/as-file (workspace/find-resource workspace "/game.project"))]
            (test-support/spit-until-new-mtime game-project-file migrated-game-project-content)
            (test-util/fetch-libraries! workspace))

          ;; With the extension added, we can load the project.
          (let [project (test-util/setup-project! workspace)
                main-collection-resource (workspace/find-resource workspace "/main/main.collection")
                main-gui-resource (workspace/find-resource workspace "/main/main.gui")
                main-collection (test-util/resource-node project main-collection-resource)
                main-gui (test-util/resource-node project main-gui-resource)]
            ;; Verify that resources with embedded Spine data can be edited once
            ;; the extension has been added to game.project, but the project
            ;; should still report build errors for legacy Spine JSON data that
            ;; is referenced by Spine Scenes.
            (testing "With the extension, resources with embedded Spine data can be edited."
              (is (not (resource-node/defective? main-collection)))
              (is (not (resource-node/defective? main-gui)))
              (is (not (g/error? (g/node-value main-collection :node-outline))))
              (is (not (g/error? (g/node-value main-collection :save-data))))
              (is (not (g/error? (g/node-value main-gui :node-outline))))
              (is (not (g/error? (g/node-value main-gui :save-data)))))
            (testing "With the extension, resources with embedded Spine data are connected to the save-data system."
              (let [save-data-connected-resource? (into #{}
                                                        (map :resource)
                                                        (project/all-save-data project))]
                (is (every? save-data-connected-resource?
                            [main-collection-resource main-gui-resource]))))
            (testing "With the extension, resources with embedded Spine data report build errors for references to unmigrated spine json files."
              (letfn [(unmigrated-spine-json-error? [project error-resource-path error-value]
                        (is (g/error? error-value))
                        (let [workspace (project/workspace project)
                              error-resource (workspace/find-resource workspace error-resource-path)
                              error-resource-node (test-util/resource-node project error-resource)
                              error-tree (build-errors-view/build-resource-tree error-value)
                              error-item-of-parent-resource (first (:children error-tree))
                              error-item-of-faulty-node (first (:children error-item-of-parent-resource))
                              error-message-of-faulty-node (:message error-item-of-faulty-node)]
                          (is (re-matches #"Spine Json file '.*' doesn't end with '\.spinejson'" error-message-of-faulty-node))
                          (is (= [error-resource error-resource-node]
                                 (error-item-open-info-without-opts error-item-of-parent-resource)))
                          (is (= [error-resource error-resource-node]
                                 (error-item-open-info-without-opts error-item-of-faulty-node)))))]
                (is (unmigrated-spine-json-error? project "/assets/spineboy.spinescene" (test-util/build-error! main-collection)))
                (is (unmigrated-spine-json-error? project "/assets/spineboy.spinescene" (test-util/build-error! main-gui)))))

            ;; Perform the manual migration steps. In our case, the test project
            ;; already contains updated Spine JSON data that has been exported
            ;; from an up-to-date version of Spine, so we just have to update
            ;; our references to point to the new Spine JSON.
            (testing "Updating references to the new version of the Spine JSON."
              (let [save-data-content-by-proj-path-before (save-data-content-by-proj-path project)
                    new-spine-json-resource (workspace/find-resource workspace "/assets/spineboy/spineboy.spinejson")
                    new-spine-atlas-resource (workspace/find-resource workspace "/assets/spineboy/spineboy.atlas")
                    new-spine-material-resource (workspace/find-resource workspace "/defold-spine/assets/spine.material")
                    old-spine-scene (test-util/resource-node project "/assets/spineboy.spinescene")
                    embedded-spine-model (:node-id (test-util/outline main-collection [0 1]))]
                (test-util/prop! old-spine-scene :spine-json new-spine-json-resource)
                (test-util/prop! old-spine-scene :atlas new-spine-atlas-resource)
                (test-util/prop! embedded-spine-model :material new-spine-material-resource)

                (testing "Updating references to the new version of the Spine JSON fixes all errors."
                  (is (not (g/error? (g/node-value main-collection :scene))))
                  (is (not (g/error? (g/node-value main-gui :scene))))
                  (is (not (g/error? (test-util/build-error! main-collection))))
                  (is (not (g/error? (test-util/build-error! main-gui)))))

                (testing "Collection properties are retained post-update."
                  (let [ext->resource-type (workspace/get-resource-type-map workspace)
                        main-collection-save-value (g/node-value main-collection :save-value)
                        main-collection-data (collection-data/string-decode-collection-data ext->resource-type main-collection-save-value)
                        embedded-spine-model-data (get-in main-collection-data [:embedded-instances 0 :data :embedded-components 0 :data])]
                    (is (= "/assets/spineboy.spinescene" (:spine-scene embedded-spine-model-data)))
                    (is (= "idle" (:default-animation embedded-spine-model-data)))
                    (is (= "" (:skin embedded-spine-model-data)))
                    (is (= :blend-mode-alpha (:blend-mode embedded-spine-model-data)))
                    (is (= "/defold-spine/assets/spine.material" (:material embedded-spine-model-data)))))

                (testing "Gui properties are retained post-update."
                  (let [main-gui-save-value (g/node-value main-gui :save-value)
                        main-gui-nodes (:nodes main-gui-save-value)
                        spine-gui-node (first main-gui-nodes)]
                    (is (not (contains? main-gui-save-value :spine-scenes)))
                    (is (= [{:name "spineboy"
                             :path "/assets/spineboy.spinescene"}]
                           (:resources main-gui-save-value)))
                    (is (= 1 (count main-gui-nodes)))
                    (is (= [701.0 0.0 0.0 1.0] (:position spine-gui-node)))
                    (is (= [0.0 0.0 0.0 1.0] (:rotation spine-gui-node)))
                    (is (= [1.0 1.0 1.0 1.0] (:scale spine-gui-node)))
                    (is (= [1.0 1.0 0.0 1.0] (:size spine-gui-node)))
                    (is (= [1.0 1.0 1.0 1.0] (:color spine-gui-node)))
                    (is (= :type-custom (:type spine-gui-node)))
                    (is (= 405028931 (:custom-type spine-gui-node)))
                    (is (= :blend-mode-alpha (:blend-mode spine-gui-node)))
                    (is (= "spine" (:id spine-gui-node)))
                    (is (= :xanchor-none (:xanchor spine-gui-node)))
                    (is (= :yanchor-none (:yanchor spine-gui-node)))
                    (is (= :pivot-center (:pivot spine-gui-node)))
                    (is (= :adjust-mode-fit (:adjust-mode spine-gui-node)))
                    (is (= "" (:layer spine-gui-node)))
                    (is (= true (:inherit-alpha spine-gui-node)))
                    (is (= :clipping-mode-none (:clipping-mode spine-gui-node)))
                    (is (= true (:clipping-visible spine-gui-node)))
                    (is (= false (:clipping-inverted spine-gui-node)))
                    (is (= 1.0 (:alpha spine-gui-node)))
                    (is (= false (:template-node-child spine-gui-node)))
                    (is (= :size-mode-auto (:size-mode spine-gui-node)))
                    (is (= "spineboy" (:spine-scene spine-gui-node)))
                    (is (= "walk" (:spine-default-animation spine-gui-node)))
                    (is (= "" (:spine-skin spine-gui-node)))
                    (is (empty? (:overridden-fields spine-gui-node)))))

                (testing "Expected save-data differences post-update."
                  (let [save-data-content-by-proj-path-after (save-data-content-by-proj-path project)
                        save-data-diffs-by-proj-path (diff-save-data-content-by-proj-path save-data-content-by-proj-path-before save-data-content-by-proj-path-after)]
                    (is (= {"/assets/spineboy.spinescene"
                            [{:type  :replace
                              :left  ["spine_json: \"/assets/spineboy.json\""
                                      "atlas: \"/assets/spineboy.atlas\""]
                              :right ["spine_json: \"/assets/spineboy/spineboy.spinejson\""
                                      "atlas: \"/assets/spineboy/spineboy.atlas\""]}]

                            "/main/main.collection"
                            [{:type  :replace
                              :left  ["  \"material: \\\\\\\"/builtins/materials/spine.material\\\\\\\"\\\\n\""]
                              :right ["  \"material: \\\\\\\"/defold-spine/assets/spine.material\\\\\\\"\\\\n\""]}]}
                           save-data-diffs-by-proj-path))))))))))))

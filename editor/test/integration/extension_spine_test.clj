;; Copyright 2020-2025 The Defold Foundation
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

(ns integration.extension-spine-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build-errors-view :as build-errors-view]
            [editor.defold-project :as project]
            [editor.game-project :as game-project]
            [editor.gui :as gui]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [integration.gui-test :as gui-test]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :refer [pair]]
            [util.diff :as diff]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/spine_project")

(def ^:private migration-project-path "test/resources/spine_migration_project")

(def ^:private extension-spine-url (settings-core/inject-jvm-properties "{{defold.extension.spine.url}}"))

(def ^:private error-item-open-info-without-opts (comp pop :args build-errors-view/error-item-open-info))

(defn- outline-info [{:keys [children label read-only]}]
  (cond-> {:label label}
          read-only (assoc :read-only true)
          (not-empty children) (assoc :children (mapv outline-info children))))

(defn- node-outline-info [node-id]
  (outline-info (g/valid-node-value node-id :node-outline)))

(defn- save-data-content-by-proj-path [project]
  (into {}
        (map (juxt (comp resource/proj-path :resource)
                   resource-node/save-data-content))
        (project/all-save-data project)))

(defn- diff-save-data-content-by-proj-path [save-data-content-by-proj-path-before save-data-content-by-proj-path-after]
  (into {}
        (keep (fn [[proj-path save-data-content-after]]
                (let [save-data-content-before (save-data-content-by-proj-path-before proj-path)
                      diff-lines (diff/make-diff-output-lines save-data-content-before save-data-content-after 0)]
                  (when-not (empty? diff-lines)
                    (pair proj-path diff-lines)))))
        save-data-content-by-proj-path-after))

(deftest registered-resource-types-test
  (test-util/with-loaded-project project-path
    (is (= #{} (test-util/protobuf-resource-exts-that-read-defaults workspace)))))

(deftest dirty-save-data-test
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)
    (is (= #{} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/assets/spineboy/spineboy.spinescene")
    (is (= #{"/assets/spineboy/spineboy.spinescene"} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/main/spineboy.spinemodel")
    (is (= #{"/assets/spineboy/spineboy.spinescene" "/main/spineboy.spinemodel"} (test-util/dirty-proj-paths project)))))

(deftest spinescene-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/assets/spineboy/spineboy.spinescene")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Spine Scene"
                :children [{:label "root"
                            :read-only true
                            :children [{:label "hip"
                                        :read-only true
                                        :children [{:label "aim-constraint-target"
                                                    :read-only true}
                                                   {:label "rear-thigh"
                                                    :read-only true
                                                    :children [{:label "rear-shin"
                                                                :read-only true
                                                                :children [{:label "rear-foot"
                                                                            :read-only true
                                                                            :children [{:label "back-foot-tip"
                                                                                        :read-only true}]}]}]}
                                                   {:label "torso"
                                                    :read-only true
                                                    :children [{:label "torso2"
                                                                :read-only true
                                                                :children [{:label "torso3"
                                                                            :read-only true
                                                                            :children [{:label "front-shoulder"
                                                                                        :read-only true
                                                                                        :children [{:label "front-upper-arm"
                                                                                                    :read-only true
                                                                                                    :children [{:label "front-bracer"
                                                                                                                :read-only true
                                                                                                                :children [{:label "front-fist"
                                                                                                                            :read-only true}]}]}]}
                                                                                       {:label "back-shoulder"
                                                                                        :read-only true
                                                                                        :children [{:label "rear-upper-arm"
                                                                                                    :read-only true
                                                                                                    :children [{:label "rear-bracer"
                                                                                                                :read-only true
                                                                                                                :children [{:label "gun"
                                                                                                                            :read-only true
                                                                                                                            :children [{:label "gun-tip"
                                                                                                                                        :read-only true}]}
                                                                                                                           {:label "muzzle"
                                                                                                                            :read-only true
                                                                                                                            :children [{:label "muzzle-ring"
                                                                                                                                        :read-only true}
                                                                                                                                       {:label "muzzle-ring2"
                                                                                                                                        :read-only true}
                                                                                                                                       {:label "muzzle-ring3"
                                                                                                                                        :read-only true}
                                                                                                                                       {:label "muzzle-ring4"
                                                                                                                                        :read-only true}]}]}]}]}
                                                                                       {:label "neck"
                                                                                        :read-only true
                                                                                        :children [{:label "head"
                                                                                                    :read-only true
                                                                                                    :children [{:label "hair1"
                                                                                                                :read-only true
                                                                                                                :children [{:label "hair2"
                                                                                                                            :read-only true}]}
                                                                                                               {:label "hair3"
                                                                                                                :read-only true
                                                                                                                :children [{:label "hair4"
                                                                                                                            :read-only true}]}
                                                                                                               {:label "head-control"
                                                                                                                :read-only true}]}]}]}]}]}
                                                   {:label "front-thigh"
                                                    :read-only true
                                                    :children [{:label "front-shin"
                                                                :read-only true
                                                                :children [{:label "front-foot"
                                                                            :read-only true
                                                                            :children [{:label "front-foot-tip"
                                                                                        :read-only true}]}]}]}]}
                                       {:label "crosshair"
                                        :read-only true}
                                       {:label "rear-foot-target"
                                        :read-only true
                                        :children [{:label "rear-leg-target"
                                                    :read-only true}]}
                                       {:label "board-ik"
                                        :read-only true}
                                       {:label "clipping"
                                        :read-only true}
                                       {:label "hoverboard-controller"
                                        :read-only true
                                        :children [{:label "exhaust1"
                                                    :read-only true}
                                                   {:label "exhaust2"
                                                    :read-only true}
                                                   {:label "exhaust3"
                                                    :read-only true}
                                                   {:label "hoverboard-thruster-front"
                                                    :read-only true
                                                    :children [{:label "hoverglow-front"
                                                                :read-only true}]}
                                                   {:label "hoverboard-thruster-rear"
                                                    :read-only true
                                                    :children [{:label "hoverglow-rear"
                                                                :read-only true}]}
                                                   {:label "side-glow1"
                                                    :read-only true}
                                                   {:label "side-glow2"
                                                    :read-only true}]}
                                       {:label "portal-root"
                                        :read-only true
                                        :children [{:label "flare1"
                                                    :read-only true}
                                                   {:label "flare10"
                                                    :read-only true}
                                                   {:label "flare2"
                                                    :read-only true}
                                                   {:label "flare3"
                                                    :read-only true}
                                                   {:label "flare4"
                                                    :read-only true}
                                                   {:label "flare5"
                                                    :read-only true}
                                                   {:label "flare6"
                                                    :read-only true}
                                                   {:label "flare7"
                                                    :read-only true}
                                                   {:label "flare8"
                                                    :read-only true}
                                                   {:label "flare9"
                                                    :read-only true}
                                                   {:label "portal"
                                                    :read-only true}
                                                   {:label "portal-shade"
                                                    :read-only true}
                                                   {:label "portal-streaks1"
                                                    :read-only true}
                                                   {:label "portal-streaks2"
                                                    :read-only true}]}
                                       {:label "front-foot-target"
                                        :read-only true
                                        :children [{:label "front-leg-target"
                                                    :read-only true}]}]}]}
               (node-outline-info node-id))))

      (testing "scene"
        (is (not (g/error? (g/node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:atlas "/assets/spineboy/spineboy.atlas"
                :spine-json "/assets/spineboy/spineboy.spinejson"}
               (g/node-value node-id :save-value)))))))

(deftest spinemodel-outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/main/spineboy.spinemodel")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets)))))

      (testing "node-outline"
        (is (= {:label "Spine Model"}
               (node-outline-info node-id))))

      (testing "scene"
        (is (not (g/error? (g/node-value node-id :scene)))))

      (testing "save-value"
        (is (= {:default-animation "idle"
                :material "/defold-spine/assets/spine.material"
                :skin "" ; Required protobuf field.
                :spine-scene "/assets/spineboy/spineboy.spinescene"}
               (g/node-value node-id :save-value)))))))

(deftest collection-usage-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

(deftest legacy-spine-project-user-migration-test
  ;; Clear custom gui scene loaders to ensure a clean test.
  (gui/clear-custom-gui-scene-loaders-and-node-types-for-tests!)

  ;; Load the unmigrated project to check that the editor won't corrupt it. Then
  ;; add a dependency to the extension-spine library and reload the project.
  (let [migrated-game-project-content
        (test-util/with-loaded-project migration-project-path :logging-suppressed true
          (let [main-collection-resource (workspace/find-resource workspace "/main/main.collection")
                main-gui-resource (workspace/find-resource workspace "/main/main.gui")
                main-collection (test-util/resource-node project main-collection-resource)
                main-gui (test-util/resource-node project main-gui-resource)]
            (testing "Without the extension, resources with embedded Spine data cannot be edited."
              (is (g/defective? main-collection))
              (is (g/defective? main-gui)))
            (testing "Without the extension, resources with embedded Spine data aren't connected to the save-data system, and cannot be corrupted by save."
              (is (not-any? #{main-collection-resource main-gui-resource}
                            (map :resource (project/all-save-data project)))))
            (testing "Without the extension, resources with embedded Spine data report build errors due to invalid content."
              (letfn [(invalid-content-error? [error-resource-path error-value]
                        (is (g/error? error-value))
                        (is (= :invalid-content (-> error-value :causes first :user-data :type)))
                        (let [error-tree (build-errors-view/build-resource-tree error-value)
                              error-item-of-parent-resource (first (:children error-tree))
                              error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                          (is (= :resource (:type error-item-of-parent-resource)))
                          (is (string/starts-with?
                                (:message error-item-of-faulty-node)
                                (str "The file '" error-resource-path "' could not be loaded")))))]
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
              (resource-node/save-data-content migrated-game-project-save-data))))]
    (testing "Manual migration steps."
      (test-support/with-clean-system
        (let [workspace (test-util/setup-scratch-workspace! world migration-project-path)]
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
              (is (not (g/defective? main-collection)))
              (is (not (g/defective? main-gui)))
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
                  (let [main-collection-desc (g/node-value main-collection :save-value)
                        embedded-spine-model-data (get-in main-collection-desc [:embedded-instances 0 :data :embedded-components 0 :data])]
                    (is (= "/assets/spineboy.spinescene" (:spine-scene embedded-spine-model-data)))
                    (is (= "idle" (:default-animation embedded-spine-model-data)))
                    (is (= "" (:skin embedded-spine-model-data)))
                    (is (not (contains? embedded-spine-model-data :blend-mode)))
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
                    (is (= [701.0 0.0 0.0] (take 3 (:position spine-gui-node))))
                    (is (not (contains? spine-gui-node :rotation)))
                    (is (not (contains? spine-gui-node :scale)))
                    (is (not (contains? spine-gui-node :size)))
                    (is (not (contains? spine-gui-node :color)))
                    (is (= :type-custom (:type spine-gui-node)))
                    (is (= 405028931 (:custom-type spine-gui-node)))
                    (is (not (contains? spine-gui-node :blend-mode)))
                    (is (= "spine" (:id spine-gui-node)))
                    (is (not (contains? spine-gui-node :xanchor)))
                    (is (not (contains? spine-gui-node :yanchor)))
                    (is (not (contains? spine-gui-node :pivot)))
                    (is (not (contains? spine-gui-node :adjust-mode)))
                    (is (not (contains? spine-gui-node :layer)))
                    (is (= true (:inherit-alpha spine-gui-node)))
                    (is (not (contains? spine-gui-node :clipping-mode)))
                    (is (not (contains? spine-gui-node :clipping-visible)))
                    (is (not (contains? spine-gui-node :clipping-inverted)))
                    (is (not (contains? spine-gui-node :alpha)))
                    (is (not (contains? spine-gui-node :template-node-child)))
                    (is (= :size-mode-auto (:size-mode spine-gui-node)))
                    (is (= "spineboy" (:spine-scene spine-gui-node)))
                    (is (= "walk" (:spine-default-animation spine-gui-node)))
                    (is (not (contains? spine-gui-node :spine-skin)))
                    (is (empty? (:overridden-fields spine-gui-node)))))

                (testing "Expected save-data differences post-update."
                  (let [save-data-content-by-proj-path-after (save-data-content-by-proj-path project)
                        save-data-diffs-by-proj-path (diff-save-data-content-by-proj-path save-data-content-by-proj-path-before save-data-content-by-proj-path-after)]
                    (is (= {"/assets/spineboy.spinescene"
                            ["1   - spine_json: \"/assets/spineboy.json\""
                             "2   - atlas: \"/assets/spineboy.atlas\""
                             "  1 + spine_json: \"/assets/spineboy/spineboy.spinejson\""
                             "  2 + atlas: \"/assets/spineboy/spineboy.atlas\""]

                            "/main/main.collection"
                            ["15    -   \"material: \\\\\\\"/builtins/materials/spine.material\\\\\\\"\\\\n\""
                             "   15 +   \"material: \\\\\\\"/defold-spine/assets/spine.material\\\\\\\"\\\\n\""]}

                           save-data-diffs-by-proj-path))))))))))))

(defn- built-scene-desc [gui-scene-node-id]
  (-> gui-scene-node-id
      (g/valid-node-value :build-targets)
      (get-in [0 :user-data :pb])))

(deftest layout-node-desc-includes-size-mode-test
  (test-util/with-loaded-project project-path
    (let [gui-scene (test-util/resource-node project "/main/spineboy.gui")]

      (testing "Built Spine NodeDescs have :size-mode-auto."
        (let [built-scene-desc (built-scene-desc gui-scene)
              built-node-desc (get-in built-scene-desc [:nodes 0])]
          (is (= :size-mode-auto (:size-mode built-node-desc)))
          (is (not (contains? built-scene-desc :layouts)))))

      ;; Add a Portrait layout to the gui scene.
      (gui-test/add-layout! project app-view gui-scene "Portrait")

      (testing "Newly added layout exists, but contains no override NodeDescs."
        (let [built-scene-desc (built-scene-desc gui-scene)
              built-layout-desc (get-in built-scene-desc [:layouts 0])]
          (is (= "Portrait" (:name built-layout-desc)))
          (is (not (contains? built-layout-desc :nodes)))))

      ;; Override the default animation on the SpineNode.
      (let [spine-node (test-util/outline-node-id gui-scene (localization/message "outline.gui.nodes") "spineboy")]
        (gui-test/with-visible-layout! gui-scene "Portrait"
          (test-util/prop! spine-node :spine-default-animation "jump")))

      (testing "After overriding a property, the override NodeDesc includes all properties from the default layout."
        (let [built-scene-desc (built-scene-desc gui-scene)
              built-node-desc (get-in built-scene-desc [:nodes 0])
              built-layout-desc (get-in built-scene-desc [:layouts 0])
              built-node-desc-for-layout (get-in built-layout-desc [:nodes 0])]
          (is (= (assoc built-node-desc :spine-default-animation "jump")
                 built-node-desc-for-layout)))))))

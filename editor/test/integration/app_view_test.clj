(ns integration.app-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.fs :as fs]
            [editor.git :as git]
            [editor.defold-project :as project]
            [editor.progress :as progress]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [spit-until-new-mtime with-clean-system]])
  (:import [java.io File]))

(deftest open-editor
  (testing "Opening editor only alters undo history by selection"
           (test-util/with-loaded-project
             (let [proj-graph (g/node-id->graph-id project)
                   _          (is (not (g/has-undo? proj-graph)))
                   [atlas-node view] (test-util/open-scene-view! project app-view "/switcher/fish.atlas" 128 128)]
               ;; One history entry for selection
               (is (g/has-undo? proj-graph))
               (g/undo! proj-graph)
               (is (not (g/has-undo? proj-graph)))))))

(deftest select-test
  (testing "asserts that all node-ids are non-nil"
    (test-util/with-loaded-project
      (is (thrown? AssertionError (app-view/select app-view [nil])))))
  (testing "preserves selection order"
    (test-util/with-loaded-project
      (let [root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (are [s] (do (app-view/select! app-view s)
                   (= s (g/node-value app-view :selected-node-ids)))
          [sprite-0 sprite-1]
          [sprite-1 sprite-0]))))
  (testing "ensures selected nodes are distinct, preserving order"
    (test-util/with-loaded-project
      (let [root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (are [in-s out-s] (do (app-view/select! app-view in-s)
                            (= out-s (g/node-value app-view :selected-node-ids)))
          [sprite-0 sprite-1 sprite-0 sprite-1] [sprite-0 sprite-1]
          [sprite-0 sprite-0 sprite-1 sprite-1] [sprite-0 sprite-1]
          [sprite-1 sprite-0 sprite-1 sprite-0] [sprite-1 sprite-0]))))
  (testing "selection for different 'tabs'"
    (test-util/with-loaded-project
      (let [root-node-0 (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")]
        (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))
        (let [root-node-1 (test-util/open-tab! project app-view "/logic/hierarchy.collection")]
          (is (= [root-node-1] (g/node-value app-view :selected-node-ids)))
          (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
          (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))))))
  (testing "selection removed with tabs"
    (test-util/with-loaded-project
      (let [root-node-0 (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            has-selection? (fn [path] (let [node-id (project/get-resource-node project path)]
                                        (contains? (g/node-value project :selected-node-ids-by-resource-node) node-id)))]
        (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))
        (is (has-selection? "/logic/two_atlas_sprites.collection"))
        (app-view/select! app-view [root-node-0])
        (is (has-selection? "/logic/two_atlas_sprites.collection"))
        (let [root-node-1 (test-util/open-tab! project app-view "/logic/hierarchy.collection")]
          (is (= [root-node-1] (g/node-value app-view :selected-node-ids)))
          (is (and (has-selection? "/logic/two_atlas_sprites.collection")
                (has-selection? "/logic/hierarchy.collection")))
          (app-view/select! app-view [root-node-1])
          (is (and (has-selection? "/logic/two_atlas_sprites.collection")
                (has-selection? "/logic/hierarchy.collection")))
          (test-util/close-tab! project app-view "/logic/hierarchy.collection")
          ;; Selection lingers when tab is closed
          (is (and (has-selection? "/logic/two_atlas_sprites.collection")
                (has-selection? "/logic/hierarchy.collection")))
          ;; New selection to clean out the lingering data from the previous tab
          (app-view/select! app-view [root-node-0])
          (is (has-selection? "/logic/two_atlas_sprites.collection")))))))

(defn- init-git [path]
  (let [git (git/init path)]
    ;; Add project content
    (-> git (.add) (.addFilepattern ".") (.call))
    (-> git (.commit) (.setMessage "init repo") (.call))
    git))

(defn- edit-and-save! [workspace atlas margin]
  (g/set-property! atlas :margin margin)
  (let [save-data (g/node-value atlas :save-data)]
    (spit-until-new-mtime (:resource save-data) (:content save-data))
    (workspace/resource-sync! workspace [])))

(defn- revert-all! [workspace git]
  (let [status (git/unified-status git)
        moved-files (->> status
                      (filter #(= (:change-type %) :rename))
                      (mapv #(vector (test-util/file workspace (:new-path %)) (test-util/file workspace (:old-path %)))))]
    (git/revert git (mapv (fn [status] (or (:new-path status) (:old-path status))) status))
    (workspace/resource-sync! workspace moved-files)))

(deftest revert-rename-of-opened-file
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/reload_project")
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view! project)
          atlas-path "/atlas/single.atlas"
          atlas-res (workspace/resolve-workspace-resource workspace atlas-path)
          atlas-path2 "/atlas/single2.atlas"
          atlas-res2 (workspace/resolve-workspace-resource workspace atlas-path2)
          [atlas scene-view] (test-util/open-scene-view! project app-view atlas-path 64 64)
          proj-path (.getAbsolutePath (workspace/project-path workspace))
          git (init-git proj-path)
          atlas-outline (fn [path] (test-util/outline (g/node-value app-view :active-resource-node) path))]
      (is (= atlas-res (g/node-value app-view :active-resource)))
      (test-util/move-file! workspace atlas-path atlas-path2)
      (is (= atlas-res2 (g/node-value app-view :active-resource)))
      (revert-all! workspace git)
      (is (= atlas-res (g/node-value app-view :active-resource)))
      (app-view/select! app-view [(:node-id (atlas-outline [0]))])
      (edit-and-save! workspace atlas 1)
      ;; Ensure reload has happened
      (is (not= atlas (g/node-value app-view :active-resource-node)))
      (is (= atlas-res (g/node-value app-view :active-resource)))
      ;; The external reload should have 'changed' selection back to the new root node
      (is (test-util/selected? app-view (g/node-value app-view :active-resource-node)))
      (revert-all! workspace git)
      (is (= atlas-res (g/node-value app-view :active-resource))))))

(deftest rename-directory-handles-all-files-in-directory
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/small_project")
          project (test-util/setup-project! workspace)
          game-project (test-util/resource-node project "/game.project")
          main-dir (workspace/find-resource workspace "/main")]
      (is main-dir)
      (let [evaluation-context (g/make-evaluation-context)
            old-artifact-map (workspace/artifact-map workspace)
            build-results (project/build! project game-project evaluation-context nil old-artifact-map progress/null-render-progress!)]
        (g/update-cache-from-evaluation-context! evaluation-context)
        (is (seq (:artifacts build-results)))
        (is (not (g/error? (:error build-results))))
        (workspace/artifact-map! workspace (:artifact-map build-results)))
      (asset-browser/rename main-dir "/blahonga")
      (is (nil? (workspace/find-resource workspace "/main")))
      (is (workspace/find-resource workspace "/blahonga"))
      (let [old-artifact-map (workspace/artifact-map workspace)
            build-results (project/build! project game-project (g/make-evaluation-context) nil old-artifact-map progress/null-render-progress!)]
        (is (seq (:artifacts build-results)))
        (is (not (g/error? (:error build-results))))
        (workspace/artifact-map! workspace (:artifact-map build-results))))))

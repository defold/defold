(ns integration.app-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest open-editor
  (testing "Opening editor only alters undo history by selection"
           (with-clean-system
             (let [[workspace project app-view] (test-util/setup! world)
                   proj-graph (g/node-id->graph-id project)
                   _          (is (not (g/has-undo? proj-graph)))
                   [atlas-node view] (test-util/open-scene-view! project app-view "/switcher/fish.atlas" 128 128)]
               (is (g/has-undo? proj-graph))
               (g/undo! proj-graph)
               (is (not (g/has-undo? proj-graph)))))))

(deftest select-test
  (testing "asserts that all node-ids are non-nil"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)]
        (is (thrown? AssertionError (app-view/select app-view [nil]))))))
  (testing "preserves selection order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (are [s] (do (app-view/select! app-view s)
                   (= s (g/node-value app-view :selected-node-ids)))
          [sprite-0 sprite-1]
          [sprite-1 sprite-0]))))
  (testing "ensures selected nodes are distinct, preserving order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (are [in-s out-s] (do (app-view/select! app-view in-s)
                            (= out-s (g/node-value app-view :selected-node-ids)))
          [sprite-0 sprite-1 sprite-0 sprite-1] [sprite-0 sprite-1]
          [sprite-0 sprite-0 sprite-1 sprite-1] [sprite-0 sprite-1]
          [sprite-1 sprite-0 sprite-1 sprite-0] [sprite-1 sprite-0]))))
  (testing "selection for different 'tabs'"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node-0 (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")]
        (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))
        (let [root-node-1 (test-util/open-tab! project app-view "/logic/hierarchy.collection")]
          (is (= [root-node-1] (g/node-value app-view :selected-node-ids)))
          (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
          (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))))))
  (testing "selection removed with tabs"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node-0 (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            has-selection? (fn [path] (let [r (workspace/resolve-workspace-resource workspace path)]
                                        (contains? (g/node-value project :selected-node-ids-by-resource) r)))]
        (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))
        (is (has-selection? "/logic/two_atlas_sprites.collection"))
        (let [root-node-1 (test-util/open-tab! project app-view "/logic/hierarchy.collection")]
          (is (= [root-node-1] (g/node-value app-view :selected-node-ids)))
          (is (and (has-selection? "/logic/two_atlas_sprites.collection")
                (has-selection? "/logic/hierarchy.collection")))
          (test-util/close-tab! project app-view "/logic/hierarchy.collection")
          ;; New selection to clean out the lingering data from the previous tab
          (app-view/select! app-view [root-node-0])
          (is (has-selection? "/logic/two_atlas_sprites.collection")))))))

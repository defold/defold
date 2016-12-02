(ns integration.app-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [editor.app-view :as app-view]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.resource :as resource]
            [integration.test-util :as test-util])
  (:import [java.io File]
           [javax.imageio ImageIO]))

(deftest select-test
  (testing "asserts that all node-ids are non-nil"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)]
        (is (thrown? java.lang.AssertionError (app-view/select app-view [nil]))))))
  (testing "preserves selection order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (app-view/select! app-view [sprite-0 sprite-1])
        (is (= [sprite-0 sprite-1] (g/node-value app-view :selected-node-ids)))
        (app-view/select! app-view [sprite-1 sprite-0])
        (is (= [sprite-1 sprite-0] (g/node-value app-view :selected-node-ids))))))
  (testing "ensures selected nodes are distinct, preserving order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            root-node (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")
            [sprite-0 sprite-1] (map #(:node-id (test-util/outline root-node [%])) [0 1])]
        (g/transact (app-view/select app-view [sprite-0 sprite-1 sprite-0 sprite-1]))
        (is (= [sprite-0 sprite-1] (g/node-value app-view :selected-node-ids)))
        (g/transact (app-view/select app-view [sprite-0 sprite-0 sprite-1 sprite-1]))
        (is (= [sprite-0 sprite-1] (g/node-value app-view :selected-node-ids))))))
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
            root-node-0 (test-util/open-tab! project app-view "/logic/two_atlas_sprites.collection")]
        (is (= [root-node-0] (g/node-value app-view :selected-node-ids)))
        (is (= 1 (count (g/node-value project :selected-node-ids-by-resource))))
        (let [root-node-1 (test-util/open-tab! project app-view "/logic/hierarchy.collection")]
          (is (= [root-node-1] (g/node-value app-view :selected-node-ids)))
          (is (= 2 (count (g/node-value project :selected-node-ids-by-resource))))
          (test-util/close-tab! project app-view "/logic/hierarchy.collection")
          ;; New selection to clean out the lingering data from the previous tab
          (app-view/select! app-view [root-node-0])
          (is (= 1 (count (g/node-value project :selected-node-ids-by-resource)))))))))

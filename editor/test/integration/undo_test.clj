(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.core :as core]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.collection :as collection]
            [editor.game-object :as game-object]
            [editor.cubemap :as cubemap]
            [editor.atlas :as atlas]
            [editor.image :as image]
            [editor.scene :as scene]
            [editor.platformer :as platformer]
            [editor.switcher :as switcher]
            [editor.sprite :as sprite]
            [internal.clojure :as clojure]
            [internal.node :as in]
            [internal.system :as is])
  (:import [java.io File]
           [java.awt.image BufferedImage]
           [dynamo.types Region]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")

(g/defnode DummySceneView
  (inherits core/Scope)

  (property width t/Num)
  (property height t/Num)
  (input frame BufferedImage)
  (input input-handlers [Runnable])
  (output viewport Region (g/fnk [width height] (t/->Region 0 width 0 height))))

(defn make-dummy-view [graph width height]
  (first (ds/tx-nodes-added (ds/transact (g/make-node graph DummySceneView :width width :height height)))))

(defn- load-test-workspace [graph]
  (first
    (ds/tx-nodes-added
      (ds/transact
        (g/make-nodes
          graph
          [workspace [workspace/Workspace :root project-path]]
          (collection/register-resource-types workspace)
          (game-object/register-resource-types workspace)
          (cubemap/register-resource-types workspace)
          (image/register-resource-types workspace)
          (atlas/register-resource-types workspace)
          (platformer/register-resource-types workspace)
          (switcher/register-resource-types workspace)
          (sprite/register-resource-types workspace))))))

(defn- load-test-project
  [workspace proj-graph]
  (let [project (first
                      (ds/tx-nodes-added
                        (ds/transact
                          (g/make-nodes
                            proj-graph
                            [project [project/Project :workspace workspace]]
                            (g/connect workspace :resource-list project :resources)
                            (g/connect workspace :resource-types project :resource-types)))))
        project (project/load-project project (g/node-value workspace :resource-list))]
    (ds/reset-undo! proj-graph)
    project))

(defn- headless-create-view
  "Duplicates editor.boot/create-editor, except it doesn't build any GUI."
  [workspace project resource-node]
  (let [resource-type (project/get-resource-type resource-node)]
    (when (and resource-type (:setup-view-fn resource-type))
      (let [setup-view-fn (:setup-view-fn resource-type)
            setup-rendering-fn (:setup-rendering-fn resource-type)]
        (let [view-graph (ds/attach-graph (g/make-graph :volatility 100))
              view       (scene/make-preview-view view-graph 128 128)]
          (ds/transact (setup-view-fn resource-node view))
          (ds/transact (setup-rendering-fn resource-node (g/refresh view)))
          (g/refresh view))))))

(defn- has-undo? [graph]
  (ds/has-undo? graph))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            project-graph (ds/attach-graph-with-history (g/make-graph :volatility 1))
            project       (load-test-project workspace project-graph)]
        (is (false? (has-undo? project-graph)))
        (let [atlas-nodes (project/find-resources project "**/*.atlas")]
          (is (> (count atlas-nodes) 0)))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
           (with-clean-system
             (let [workspace     (load-test-workspace world)
                   project-graph (ds/attach-graph-with-history (g/make-graph :volatility 1))
                   project       (load-test-project workspace project-graph)]
               (let [atlas-nodes (project/find-resources project "**/*.atlas")
                     atlas-node (second (first atlas-nodes))
                     view (headless-create-view workspace project atlas-node)]
                 (is (not-nil? (g/node-value view :frame)))
                 (is (not (has-undo? project-graph))))))))

(deftest undo-node-deletion-reconnects-editor
  (testing "Undoing the deletion of a node reconnects it to its editor"
           (with-clean-system
             (let [workspace     (load-test-workspace world)
                   project-graph (ds/attach-graph-with-history (g/make-graph :volatility 1))
                   project       (load-test-project workspace project-graph)]
               (let [atlas-nodes (project/find-resources project "**/*.atlas")
                     atlas-node (second (first atlas-nodes))
                     view (headless-create-view workspace project atlas-node)
                     camera (t/lookup view :camera)]
                 (is (not-nil? (g/node-value camera :aabb)))
                 (ds/transact (g/delete-node (g/node-id atlas-node)))
                 (is (nil? (g/node-value camera :aabb)))
                 (ds/undo project-graph)
                 (is (not-nil? (g/node-value camera :aabb))))))))

(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
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
  (:import [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")

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
                            (g/connect workspace :resource-types project :resource-types)))))]
    (project/load-project project (g/node-value workspace :resource-list))))

(defn- headless-create-editor
  "Duplicates editor.boot/create-editor, except it doesn't build any GUI."
  [workspace project resource]
  (let [resource-node (project/get-resource-node project resource)
        resource-type (project/get-resource-type resource-node)]
    (when-let [setup-rendering-fn (and resource-type (:setup-rendering-fn resource-type))]
      (let [view-graph (ds/attach-graph (g/make-graph :volatility 100))
            view       (scene/make-scene-view view-graph nil nil)]
        (ds/transact (setup-rendering-fn resource-node view))
        view))))

(defn- has-undo? [graph]
  ; TODO - report if undo is available
  false)

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            project-graph (ds/attach-graph-with-history (g/make-graph :volatility 1))
            project       (load-test-project workspace project-graph)]
        (is (false? (has-undo? project-graph)))
        (let [atlas-nodes (project/find-resources project "**/*.atlas")]
          (is (> (count atlas-nodes) 0)))))))

#_(deftest open-editor
   (testing "Opening editor does not alter undo history"
     (with-clean-system
       (let [project       (load-test-project world)
             atlas-node    (first (project/find-resources project "**/*.atlas"))
             #_editor-node   #_(p/make-editor project (:filename atlas-node))
             history-count 0  ; TODO retrieve actual undo-history count
             ]
         (prn atlas-node)
         #_(is (not-nil? editor-node))
         #_(is (= history-count 0))))))

#_(deftest undo-node-deletion-reconnects-editor
   (testing "Undoing the deletion of a node reconnects it to its editor"
            (with-clean-system
              (let [project   (load-test-project world)
                    atlas-id  (g/node-id (first (p/nodes-with-extensions project ["atlas"])))
                    editor-id (g/node-id (p/make-editor project (:filename @atlas-id)))]
               (ds/transact (g/delete-node atlas-id))
               (is (not-nil? (g/node-by-id (ds/now) editor-id)))
               (is (nil? (g/node-value editor-id :node)))
               (ds/undo)
               (is (not-nil? (g/node-value editor-id :node)))))))

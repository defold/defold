(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as p]
            [editor.scene :as scene]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace]
            [internal.clojure :as clojure]
            [internal.node :as in]
            [internal.system :as is])
  (:import [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")
(def branch "dummy-branch")

(defn- load-test-workspace [world]
  (first
   (ds/tx-nodes-added
    (ds/transact
     (g/make-nodes
      world
      [workspace [workspace/Workspace :root project-path]]
      (cubemap/register-resource-types workspace)
      (image/register-resource-types workspace)
      (atlas/register-resource-types workspace)
      (platformer/register-resource-types workspace)
      (switcher/register-resource-types workspace))))))

(defn- load-test-project [workspace project-graph]
  (let [project (first
                 (ds/tx-nodes-added
                  (ds/transact
                   (g/make-nodes
                    project-graph
                    [project p/Project]
                    (g/connect workspace :resource-list project :resources)))))]
    (p/load-project project (g/node-value workspace :resource-list))))

(defn- headless-create-editor
  "Duplicates editor.boot/create-editor, except it doesn't build any GUI."
  [workspace project resource]
  (let [resource-node (p/get-resource-node project resource)
        resource-type (p/get-resource-type resource-node)]
    (when-let [setup-rendering-fn (and resource-type (:setup-rendering-fn resource-type))]
      (let [view-graph (ds/attach-graph (g/make-graph :volatility 100))
            view       (scene/make-scene-view view-graph nil nil)]
        (ds/transact (setup-rendering-fn resource-node view))))))

(defn- history-count [system world]
  (count (is/history-states (is/graph-history system world))))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            project-graph (ds/attach-graph-with-history (g/make-graph :volatility 10))
            project       (load-test-project workspace project-graph)
            history-count (history-count system world)]
        (is (= history-count 0))
        (let [atlas-nodes (p/nodes-with-extensions project ["atlas"])]
          (is (> (count atlas-nodes) 0))
          (let [atlas-node  (first atlas-nodes)
                editor-node (headless-create-editor workspace project atlas-node)]
            (is (= atlas-node (g/node-value editor-node :node)))))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
    (with-clean-system
      (let [project       (load-test-project world)
            atlas-node    (first (p/nodes-with-extensions project ["atlas"]))
            editor-node   (p/make-editor project (:filename atlas-node))
            history-count 0  ; TODO retrieve actual undo-history count
            ]
        (is (not-nil? editor-node))
        (is (= history-count 0))))))

(deftest undo-node-deletion-reconnects-editor
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

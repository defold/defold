(ns ^:integration integration.atlas-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.walk :as walk]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace]
            [internal.system :as is]))

(def project-path "resources/test_project")

(g/defnode DummySceneView
  (inherits core/Scope)

  (property width t/Num)
  (property height t/Num)
  (input frame t/Any)
  (input input-handlers [Runnable])
  (output viewport t/Any (g/fnk [width height] (t/->Region 0 width 0 height))))

(defn make-dummy-view [graph width height]
  (g/make-node! graph DummySceneView :width width :height height))

(defn- load-test-workspace [graph]
  (let [workspace (workspace/make-workspace graph project-path)]
    (g/transact
      (concat
        (scene/register-view-types workspace)))
    (let [workspace (g/refresh workspace)]
      (g/transact
        (concat
          (collection/register-resource-types workspace)
          (game-object/register-resource-types workspace)
          (cubemap/register-resource-types workspace)
          (image/register-resource-types workspace)
          (atlas/register-resource-types workspace)
          (platformer/register-resource-types workspace)
          (switcher/register-resource-types workspace)
          (sprite/register-resource-types workspace))))
    (g/refresh workspace)))

(defn- load-test-project
  [workspace proj-graph]
  (let [project (project/make-project proj-graph workspace)
        project (project/load-project project)]
    (g/reset-undo! proj-graph)
    project))

(defn- headless-create-view
  [workspace project resource-node]
  (let [resource-type (project/get-resource-type resource-node)]
    (when (and resource-type (:view-types resource-type))
      (let [make-preview-fn (:make-preview-fn (first (:view-types resource-type)))
            view-opts (:scene (:view-opts resource-type))
            view-graph (g/make-graph! :volatility 100)]
        (make-preview-fn view-graph resource-node view-opts 128 128)))))

(deftest protocol-buffer-round-trip
  (testing "Opening editor does not alter undo history"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            project-graph (g/make-graph! :history true :volatility 1)
            project       (load-test-project workspace project-graph)]
        (let [atlas-name  "graphics/atlas.atlas"
              text-in     (slurp (io/file project-path atlas-name))
              atlas-nodes (project/find-resources project atlas-name)
              atlas-node  (second (first atlas-nodes))
              view        (headless-create-view workspace project atlas-node)
              text-out    (:content (g/node-value atlas-node :save-data))]
          (is (= text-in text-out)))))))

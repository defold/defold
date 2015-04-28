(ns integration.save-test
  (:require [clojure.test :refer :all]
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
            [editor.workspace :as workspace])
  (:import [dynamo.types Region]
           [java.awt.image BufferedImage]
           [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")

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

(deftest save-all
  (testing "Saving all resource nodes in the project"
           (with-clean-system
             (let [queries       ["**/level1.platformer" "**/level01.switcher" "**/env.cubemap" "**/switcher.atlas" "**/atlas_sprite.collection" "**/atlas_sprite.go" "**/atlas.sprite"]
                   workspace     (load-test-workspace world)
                   project-graph (g/make-graph! :history true :volatility 1)
                   project       (load-test-project workspace project-graph)
                   save-data (group-by :resource (g/node-value project :save-data))]
               (doseq [query queries]
                 (let [[resource _] (first (project/find-resources project query))
                       save (first (get save-data resource))
                       file (slurp resource)]
                   (is (= file (:content save)))))))))

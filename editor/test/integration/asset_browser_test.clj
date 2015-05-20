(ns integration.asset-browser-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace])
  (:import [java.io File]
           [javax.imageio ImageIO]))

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

(deftest workspace-tree
  (testing "The file system can be retrieved as a tree"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            history-count 0  ; TODO retrieve actual undo-history count
            root          (g/node-value workspace :resource-tree)]
        (is (workspace/url root) "file:/")))))

(deftest asset-browser-search
  (testing "Searching for a resource produces a hit and renders a preview"
    (with-clean-system
      (let [ws-graph world
            proj-graph (g/make-graph! :history true :volatility 1)
            view-graph (g/make-graph! :volatility 100)
            workspace  (load-test-workspace ws-graph)
            project    (project/make-project proj-graph workspace)
            resources  (g/node-value workspace :resource-list)
            project    (project/load-project project)
            #_queries  #_["**/atlas_sprite.collection"]
            #_queries  #_["**/level01.switcher"]
            #_queries  #_["**/level1.platformer"]
            #_queries  #_["**/atlas.atlas"]
            #_queries  #_["**/env.cubemap"]
            #_queries  #_["**/atlas.sprite"]
            #_queries  #_["**/atlas_sprite.go"]
            queries  ["**/atlas.atlas" "**/env.cubemap" "**/level1.platformer" "**/level01.switcher" "**/atlas.sprite" "**/atlas_sprite.go" "**/atlas_sprite.collection"]]
        (doseq [query queries
                :let [results (project/find-resources project query)]]
          (is (= 1 (count results)))
          (let [resource-node (get (first results) 1)
                resource-type (project/get-resource-type resource-node)
                view-type (first (:view-types resource-type))
                make-preview-fn (:make-preview-fn view-type)
                view-opts ((:id view-type) (:view-opts resource-type))
                view (make-preview-fn view-graph resource-node view-opts 128 128)]
            (let [image (g/node-value view :frame)]
              (is (not (nil? image)))
              #_(let [file (File. "test.png")]
                 (ImageIO/write image "png" file)))))))))

#_(asset-browser-search)

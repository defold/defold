(ns integration.asset-browser-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
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
            [editor.resource :as resource]
            [integration.test-util :as test-util])
  (:import [java.io File]
           [javax.imageio ImageIO]))

(deftest workspace-tree
  (testing "The file system can be retrieved as a tree"
    (with-clean-system
      (let [workspace     (test-util/setup-workspace! world)
            root          (g/node-value workspace :resource-tree)]
        (is (resource/url root) "file:/")))))

(deftest asset-browser-search
  (testing "Searching for a resource produces a hit and renders a preview"
    (let [queries ["**/atlas.atlas" "**/env.cubemap" "**/level1.platformer" "**/level01.switcher"
                   "**/atlas.sprite" "**/atlas_sprite.go" "**/atlas_sprite.collection"]]
      (with-clean-system
        (let [workspace  (test-util/setup-workspace! world)
              project    (test-util/setup-project! workspace)
              app-view   (test-util/setup-app-view!)
              view-graph (g/make-graph! :history false :volatility 2)]
          (doseq [query queries
                  :let [results (project/find-resources project query)]]
            (is (= 1 (count results)))
            (let [resource-node (get (first results) 1)
                  resource-type (project/get-resource-type resource-node)
                  view-type (first (:view-types resource-type))
                  make-preview-fn (:make-preview-fn view-type)
                  view-opts (assoc ((:id view-type) (:view-opts resource-type))
                                   :app-view app-view
                                   :project project)
                  view (make-preview-fn view-graph resource-node view-opts 128 128)]
              (let [image (g/node-value view :frame)]
                (is (not (nil? image)))))))))))

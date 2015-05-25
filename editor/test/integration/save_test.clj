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
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [dynamo.types Region]
           [java.awt.image BufferedImage]
           [java.io File]))

(deftest save-all
  (testing "Saving all resource nodes in the project"
           (let [queries ["**/level1.platformer" "**/level01.switcher" "**/env.cubemap" "**/switcher.atlas"
                          "**/atlas_sprite.collection" "**/atlas_sprite.go" "**/atlas.sprite"]]
             (with-clean-system
               (let [workspace (test-util/setup-workspace! world)
                     project   (test-util/setup-project! workspace)
                     save-data (group-by :resource (g/node-value project :save-data))]
                 (doseq [query queries]
                   (let [[resource _] (first (project/find-resources project query))
                         save (first (get save-data resource))
                         file (slurp resource)]
                     (is (= file (:content save))))))))))

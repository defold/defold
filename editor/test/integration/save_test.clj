(ns integration.save-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [editor.project :as project]
            [integration.test-util :as test-util]))

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

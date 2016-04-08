(ns integration.save-test
  (:require [clojure.test :refer :all]
            [clojure.data :as data]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [integration.test-util :as test-util])
  (:import [java.io StringReader]
           [com.dynamo.gui.proto Gui$SceneDesc]))

(deftest save-all
  (testing "Saving all resource nodes in the project"
           (let [queries [["**/level1.platformer" nil]
                          ["**/level01.switcher" nil]
                          ["**/env.cubemap" nil]
                          ["**/switcher.atlas" nil]
                          ["**/atlas_sprite.collection" nil]
                          ["**/atlas_sprite.go" nil]
                          ["**/atlas.sprite" nil]
                          ["**/props.go" nil]
                          ["game.project" nil]
                          ["**/super_scene.gui" Gui$SceneDesc]
                          ["**/scene.gui" Gui$SceneDesc]]]
             (with-clean-system
               (let [workspace (test-util/setup-workspace! world)
                     project   (test-util/setup-project! workspace)
                     save-data (group-by :resource (project/save-data project))]
                 (doseq [[query pb-class] queries]
                   (let [[resource _] (first (project/find-resources project query))
                         save (first (get save-data resource))
                         file (slurp resource)]
                     (if pb-class
                       (let [pb-save (protobuf/read-text pb-class (StringReader. (:content save)))
                             pb-disk (protobuf/read-text pb-class resource)
                             path []
                             [disk save both] (data/diff (get-in pb-disk path) (get-in pb-save path))]
                         (is (nil? disk))
                         (is (nil? save)))
                       (is (= file (:content save)))))))))))

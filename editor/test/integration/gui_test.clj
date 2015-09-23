(ns integration.gui-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.project :as project])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-gui
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/logic/main.gui")
          gui-node (ffirst (g/sources-of node-id :node-outlines))])))

(deftest gui-scene-generation
 (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         scene (g/node-value node-id :scene)]
     (is (= 0.25 (get-in scene [:children 0 :children 2 :children 0 :renderable :user-data :color 3]))))))

(deftest gui-textures
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value node-id :outline)
         png-node (get-in outline [:children 0 :children 1 :node-id])
         png-tex (get-in outline [:children 1 :children 0 :node-id])]
     (is (some? png-tex))
     (is (= "png_texture" (prop png-node :texture)))
     (prop! png-tex :name "new-name")
     (is (= "new-name" (prop png-node :texture))))))

(deftest gui-atlas
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value node-id :outline)
         atlas-gui-node (get-in outline [:children 0 :children 2 :node-id])
         atlas-tex (get-in outline [:children 1 :children 1 :node-id])]
     (is (some? atlas-tex))
     (is (= "atlas_texture/anim" (prop atlas-gui-node :texture)))
     (prop! atlas-tex :name "new-name")
     (is (= "new-name/anim" (prop atlas-gui-node :texture))))))

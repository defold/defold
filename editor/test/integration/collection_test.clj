(ns integration.collection-test
  (:require [clojure.pprint :refer [pprint]]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [dynamo.util :as util]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.render.pass :as pass])
  (:import [dynamo.types Region]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]))

(defn- dump-outline [outline]
  {:self (type (:self outline)) :children (map dump-outline (:children outline))})

(deftest hierarchical-outline
  (testing "Hierarchical outline"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node      (test-util/resource-node project "/logic/hierarchy.collection")
                   outline   (g/node-value node :outline)]
               ; Two game objects under the collection
               (is (= 2 (count (:children outline))))
               ; One component and game object under the game object
               (is (= 2 (count (:children (first (:children outline))))))))))

(deftest hierarchical-scene
  (testing "Hierarchical scene"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node      (test-util/resource-node project "/logic/hierarchy.collection")
                   scene     (g/node-value node :scene)]
               ; Two game objects under the collection
               (is (= 2 (count (:children scene))))
               ; One component and game object under the game object
               (is (= 2 (count (:children (first (:children scene))))))))))

(deftest add-embedded-instance
  (testing "Hierarchical scene"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node      (test-util/resource-node project "/logic/hierarchy.collection")]
               ; Two game objects under the collection
               (is (= 2 (count (:children (g/node-value node :outline)))))
               ; Select the collection node
               (project/select! project [node])
               ; Run the add handler
               (handler/run :add [{:name :global :env {:selection [(g/node-id node)]}}])
               ; Three game objects under the collection
               (is (= 3 (count (:children (g/node-value node :outline)))))))))

(deftest empty-go
  (testing "Collection with a single empty game object"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node      (test-util/resource-node project "/collection/empty_go.collection")
                   zero-aabb (t/->AABB (Point3d. 0 0 0) (Point3d. 0 0 0))
                   outline   (g/node-value node :outline)
                   scene     (g/node-value node :scene)]
               ; Verify outline labels
               (is (= (list "Collection" "go") (map :label (tree-seq :children :children outline))))
               ; Verify AABBs
               (is (every? #(= zero-aabb %) (map :aabb (tree-seq :children :children (g/node-value node :scene)))))))))

(deftest unknown-components
  (testing "Load a collection with unknown components"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node      (test-util/resource-node project "/collection/unknown_components.collection")
                   outline   (g/node-value node :outline)
                   scene     (g/node-value node :scene)
                   zero-aabb (t/->AABB (Point3d. 0 0 0) (Point3d. 0 0 0))]
               ; Verify outline labels
               (is (= (list "Collection" "my_instance (/game_object/unknown_components.go)" "unknown (/game_object/test.unknown)")
                      (map :label (tree-seq :children :children outline))))
               ; Verify AABBs
               (is (every? #(= zero-aabb %) (map :aabb (tree-seq :children :children (g/node-value node :scene)))))))))

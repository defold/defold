(ns integration.scene-test
  (:require [clojure.test :refer :all]
            [clojure.pprint :refer [pprint]]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [dynamo.geom :as geom]
            [dynamo.util :as util]
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
            [internal.render.pass :as pass]
            [integration.test-util :as test-util])
  (:import [dynamo.types Region]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.vecmath Point3d Matrix4d]
           [javax.imageio ImageIO]))

(defn- make-aabb [min max]
  (reduce geom/aabb-incorporate (geom/null-aabb) (map #(Point3d. (double-array (conj % 0))) [min max])))

(defn- world-x [node]
  (let [scene (g/node-value node :scene)
        renderables (scene/scene->renderables scene)]
    (.getElement (:world-transform (first (second (first renderables)))) 0 3)))

(deftest gen-scene
  (testing "Scene generation"
           (let [cases {"/logic/atlas_sprite.collection"
                        (fn [node]
                          (let [go (ffirst (g/sources-of node :child-scenes))]
                            (is (= 0.0 (world-x node)))
                            (g/transact (g/set-property go :position [10 0 0]))
                            (is (= 10.0 (world-x node)))))
                        "/logic/atlas_sprite.go"
                        (fn [node]
                          (let [component (ffirst (g/sources-of node :child-scenes))]
                            (is (= 0.0 (world-x node)))
                            (g/transact (g/set-property component :position [10 0 0]))
                            (is (= 10.0 (world-x node)))))
                        "/sprite/atlas.sprite"
                        (fn [node]
                          (let [scene (g/node-value node :scene)
                                aabb (make-aabb [-101 -97] [101 97])]
                            (is (= (:aabb scene) aabb))))
                        "/car/env/env.cubemap"
                        (fn [node]
                          (let [scene (g/node-value node :scene)]
                            (is (= (:aabb scene) geom/unit-bounding-box))))
                        "/platformer/level1.platformer"
                        (fn [node]
                          (let [scene (g/node-value node :scene)
                                aabb (make-aabb [0 10] [20 10])]
                            (is (= (:aabb scene) aabb))))
                        "/switcher/level01.switcher"
                        (fn [node]
                          (let [scene (g/node-value node :scene)
                                aabb (make-aabb [-45 -45] [45 45])]
                            (is (= (:aabb scene) aabb))))
                        "/switcher/switcher.atlas"
                        (fn [node]
                          (let [scene (g/node-value node :scene)
                                aabb (make-aabb [0 0] [2048 1024])]
                            (is (= (:aabb scene) aabb))))
                        }]
             (with-clean-system
               (let [workspace     (test-util/setup-workspace! world)
                     project       (test-util/setup-project! workspace)]
                 (doseq [[path test-fn] cases]
                   (let [node (test-util/resource-node project path)]
                     (is (not (nil? node)) (format "Could not find '%s'" path))
                     (test-fn node))))))))

(deftest gen-renderables
  (testing "Renderables generation"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   app-view      (test-util/setup-app-view!)
                   path          "/sprite/small_atlas.sprite"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   renderables   (g/node-value view :renderables)]
               (is (reduce #(and %1 %2) (map #(contains? renderables %) [pass/transparent pass/selection])))))))

(deftest scene-selection
  (testing "Scene generation"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   app-view      (test-util/setup-app-view!)
                   path          "/logic/atlas_sprite.collection"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/empty-selection? project))
               ; Press
               (test-util/mouse-press! view 32 32)
               (is (test-util/selected? project go-node))
               ; Click
               (test-util/mouse-release! view 32 32)
               (is (test-util/selected? project go-node))
               ; Drag
               (test-util/mouse-drag! view 32 32 32 36)
               (is (test-util/selected? project go-node))
               ; Deselect - default to "root" node
               (test-util/mouse-press! view 0 0)
               (is (test-util/selected? project resource-node))
               ; Toggling
               (let [modifiers (if util/mac? [:meta] [:control])]
                 (test-util/mouse-click! view 32 32)
                 (is (test-util/selected? project go-node))
                 (test-util/mouse-click! view 32 32 modifiers)
                 (is (test-util/selected? project resource-node)))))))

(defn- pos [node]
  (g/node-value node :position))
(defn- rot [node]
  (g/node-value node :rotation))
(defn- scale [node]
  (g/node-value node :scale))

(deftest transform-tools
  (testing "Transform tools and manipulator interactions"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   app-view      (test-util/setup-app-view!)
                   path          "/logic/atlas_sprite.collection"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/empty-selection? project))
               ; Initial selection
               (test-util/mouse-click! view 64 64)
               (is (test-util/selected? project go-node))
               ; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 0.0 (.x (pos go-node))))
               ; Rotate tool
               (test-util/set-active-tool! app-view :rotate)
               (is (= 0.0 (.x (rot go-node))))
               (test-util/mouse-drag! view 64 64 64 68)
               (is (not= 0.0 (.x (rot go-node))))
               ; Scale tool
               (test-util/set-active-tool! app-view :scale)
               (is (= 1.0 (.x (scale go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 1.0 (.x (scale go-node))))))))

(deftest delete-undo-delete-selection
  (testing "Scene generation"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   project-graph (g/node->graph-id project)
                   app-view      (test-util/setup-app-view!)
                   path          "/logic/atlas_sprite.collection"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/empty-selection? project))
               ; Click
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? project go-node))
               ; Delete
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? project))
               ; Undo
               (g/undo project-graph)
               (is (test-util/selected? project go-node))
               ; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? project go-node))
               ; Delete again
               (g/transact (g/delete-node go-node))
               ; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/empty-selection? project))))))

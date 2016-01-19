(ns integration.scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.system :as system]
            [editor.collection :as collection]
            [editor.geom :as geom]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [integration.test-util :as test-util]
            [editor.gl.pass :as pass])
  (:import [javax.vecmath Point3d]))

(defn- make-aabb [min max]
  (reduce geom/aabb-incorporate (geom/null-aabb) (map #(Point3d. (double-array (conj % 0))) [min max])))

(deftest gen-scene
  (testing "Scene generation"
           (let [cases {"/logic/atlas_sprite.collection"
                        (fn [node-id]
                          (let [go (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-101 -97] [101 97])))
                            (g/transact (g/set-property go :position [10 0 0]))
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-91 -97] [111 97])))))
                        "/logic/atlas_sprite.go"
                        (fn [node-id]
                          (let [component (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-101 -97] [101 97])))
                            (g/transact (g/set-property component :position [10 0 0]))
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-91 -97] [111 97])))))
                        "/sprite/atlas.sprite"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [-101 -97] [101 97])]
                            (is (= (:aabb scene) aabb))))
                        "/car/env/env.cubemap"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)]
                            (is (= (:aabb scene) geom/unit-bounding-box))))
                        "/platformer/level1.platformer"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [0 10] [20 10])]
                            (is (= (:aabb scene) aabb))))
                        "/switcher/level01.switcher"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [-45 -45] [45 45])]
                            (is (= (:aabb scene) aabb))))
                        "/switcher/switcher.atlas"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [0 0] [2048 1024])]
                            (is (= (:aabb scene) aabb))))
                        }]
             (with-clean-system
               (let [workspace     (test-util/setup-workspace! world)
                     project       (test-util/setup-project! workspace)
                     app-view      (test-util/setup-app-view!)]
                 (doseq [[path test-fn] cases]
                   (let [node (test-util/resource-node project path)
                         view (test-util/open-scene-view! project app-view node 128 128)]
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
                   renderables   (g/node-value (g/graph-value (g/node-id->graph-id view) :renderer) :renderables)]
               (is (reduce #(and %1 %2) (map #(contains? renderables %) [pass/transparent pass/selection])))))))

(deftest scene-selection
  (testing "Scene selection"
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
               (let [modifiers (if system/mac? [:meta] [:control])]
                 (test-util/mouse-click! view 32 32)
                 (is (test-util/selected? project go-node))
                 (test-util/mouse-click! view 32 32 modifiers)
                 (is (test-util/selected? project resource-node)))))))

(deftest scene-multi-selection
  (testing "Scene multi selection"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   app-view      (test-util/setup-app-view!)
                   path          "/logic/two_atlas_sprites.collection"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   go-nodes      (map first (g/sources-of resource-node :child-scenes))]
               (is (test-util/empty-selection? project))
               ; Drag entire screen
               (test-util/mouse-drag! view 0 0 128 128)
               (is (every? #(test-util/selected? project %) go-nodes))))))

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
                   project-graph (g/node-id->graph-id project)
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
               (g/undo! project-graph)
               ; Rotate tool
               (test-util/set-active-tool! app-view :rotate)
               (is (= 0.0 (.x (rot go-node))))
               ;; begin drag at y = 80 to hit y axis (for x rotation)
               (test-util/mouse-drag! view 64 80 64 84)
               (is (not= 0.0 (.x (rot go-node))))
               (g/undo! project-graph)
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
                   project-graph (g/node-id->graph-id project)
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
               (g/undo! project-graph)
               (is (test-util/selected? project go-node))
               ; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? project go-node))
               ; Delete again
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? project))
               ;Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? project resource-node))))))

(deftest transform-tools-empty-go
  (testing "Transform tools and manipulator interactions"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world)
                   project       (test-util/setup-project! workspace)
                   app-view      (test-util/setup-app-view!)
                   path          "/collection/empty_go.collection"
                   resource-node (test-util/resource-node project path)
                   view          (test-util/open-scene-view! project app-view resource-node 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/empty-selection? project))
               ; Initial selection (empty go's are not selectable in the view)
               (project/select! project [go-node])
               (is (test-util/selected? project go-node))
               ; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 0.0 (.x (pos go-node))))))))

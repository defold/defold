(ns integration.benchmark-test
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

(def jit-retry-count 20)

(deftest scene->renderables-with-graph
 (testing "Scene converted into renderables"
          (with-clean-system
            (let [workspace      (test-util/setup-workspace! world "resources/massive_project")
                  project        (test-util/setup-project! workspace)
                  app-view       (test-util/setup-app-view!)
                  node           (test-util/resource-node project "/massive.collection")
                  view           (test-util/open-scene-view! project app-view node 128 128)
                  go-node-output (first (g/sources-of node :child-scenes))
                  renderer       (g/graph-value (g/node->graph-id view) :renderer)]
              (doseq [i (range jit-retry-count)]
                #_(g/transact (g/set-property (first go-node-output) :position [0 0 0]))
                (g/invalidate! [[(g/node-id (first go-node-output)) (second go-node-output)]])
                (g/node-value node :scene)
                (g/node-value renderer :renderables))))))

(deftest scene->renderables-without-graph
 (testing "Scene converted into renderables, pure conversion"
          (with-clean-system
            (let [workspace      (test-util/setup-workspace! world "resources/massive_project")
                  project        (test-util/setup-project! workspace)
                  app-view       (test-util/setup-app-view!)
                  node           (test-util/resource-node project "/massive.collection")
                  view           (test-util/open-scene-view! project app-view node 128 128)
                  go-node-output (first (g/sources-of node :child-scenes))]
              (g/invalidate! [[(g/node-id (first go-node-output)) (second go-node-output)]])
              (let [scene (g/node-value node :scene)
                    renderer (g/graph-value (g/node->graph-id view) :renderer)
                    camera (g/node-value renderer :camera)
                    viewport (g/node-value view :viewport)]
                (doseq [i (range jit-retry-count)]
                  (System/gc)
                  (scene/produce-render-data scene #{1 2 3} [] camera viewport)))))))

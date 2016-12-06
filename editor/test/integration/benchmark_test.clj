(ns integration.benchmark-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.scene :as scene]
            [integration.test-util :as test-util]))

(def jit-retry-count 20)

(deftest scene->renderables-with-graph
  (testing "Scene converted into renderables"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world "test/resources/massive_project")
            [node view] (test-util/open-scene-view! project app-view "/massive.collection" 128 128)
            go-node-output (first (g/sources-of node :child-scenes))]
        (doseq [i (range jit-retry-count)]
          (g/invalidate! [[(first go-node-output) (second go-node-output)]])
          (g/node-value node :scene)
          (g/node-value view :renderables))))))

(deftest scene->renderables-without-graph
  (testing "Scene converted into renderables, pure conversion"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world "test/resources/massive_project")
            [node view] (test-util/open-scene-view! project app-view "/massive.collection" 128 128)
            go-node-output (first (g/sources-of node :child-scenes))]
        (g/invalidate! [[(first go-node-output) (second go-node-output)]])
        (let [scene (g/node-value node :scene)
              camera (g/node-value view :camera)
              viewport (g/node-value view :viewport)]
          (doseq [i (range jit-retry-count)]
            (System/gc)
            (scene/produce-render-data scene #{1 2 3} [] camera)))))))

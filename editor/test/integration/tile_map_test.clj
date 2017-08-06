(ns integration.tile-map-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.tile-map :as tile-map]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest tile-map-outline
  (testing "shows all layers"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/tilegrid/with_layers.tilemap")
            outline   (g/node-value node-id :node-outline)]
        (is (= "Tile Map" (:label outline)))
        (is (= #{"layer1" "layer2" "blaha"} (set (map :label (:children outline)))))))))

(deftest tile-map-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/tilegrid/with_layers.tilemap")]
      (is (nil? (test-util/prop-error node-id :tile-source)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.tilesource")]]
        (test-util/with-prop [node-id :tile-source v]
          (is (g/error? (test-util/prop-error node-id :tile-source)))))
      (is (nil? (test-util/prop-error node-id :material)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
        (test-util/with-prop [node-id :material v]
          (is (g/error? (test-util/prop-error node-id :material)))))
      (let [layer (:node-id (test-util/outline node-id [0]))]
        (is (nil? (test-util/prop-error layer :z)))
        (doseq [v [-1.1 1.1]]
          (test-util/with-prop [layer :z v]
            (is (g/error? (test-util/prop-error layer :z)))))))))

(deftest tile-map-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/tilegrid/with_layers.tilemap")]
      (doseq [n (range 3)]
        (test-util/test-uses-assigned-material workspace project node-id
                                               :material
                                               [:children n :renderable :user-data :shader]
                                               [:children n :renderable :user-data :gpu-texture])))))

;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.tile-map-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gl.vertex2 :as vtx]
            [editor.localization :as localization]
            [editor.tile-map :as tile-map]
            [editor.tile-map-common :as tile-map-common]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt])
  (:import [java.nio ByteBuffer]))

(defn- vertex-buffer->vertices
  [vertex-buffer component-count]
  (let [^ByteBuffer source-buffer (vtx/buf vertex-buffer)
        ^ByteBuffer byte-buffer (.asReadOnlyBuffer source-buffer)
        _ (.order byte-buffer (.order source-buffer))
        _ (.rewind byte-buffer)
        float-buffer (.asFloatBuffer byte-buffer)
        values (float-array (.remaining float-buffer))]
    (.get float-buffer values)
    (mapv vec (partition component-count values))))

(defn- quad-triangles?
  [vertices]
  (and (= 6 (count vertices))
       (= (nth vertices 2) (nth vertices 3))
       (= (nth vertices 0) (nth vertices 5))))

(defn- layer-vertices
  [h-flip v-flip rotate90]
  (let [tile (tile-map-common/->Tile 0 0 0 h-flip v-flip rotate90)
        texture-set-data {:texture-set {:tile-width 2
                                        :tile-height 3
                                        :tile-count 1}
                          :uv-transforms [nil]}]
    (-> (tile-map/gen-layer-render-data {(tile-map-common/cell-index 0 0) tile} texture-set-data)
        :vbuf
        (vertex-buffer->vertices 5))))

(deftest tile-map-quad-producers-emit-triangles
  (testing "layer"
    (let [vertices (layer-vertices false false false)]
      (is (quad-triangles? vertices))
      (is (= [[0.0 0.0 0.0 0.0 1.0]
              [0.0 3.0 0.0 0.0 0.0]
              [2.0 3.0 0.0 1.0 0.0]
              [2.0 3.0 0.0 1.0 0.0]
              [2.0 0.0 0.0 1.0 1.0]
              [0.0 0.0 0.0 0.0 1.0]]
             vertices))))

  (testing "layer UV transforms"
    (doseq [[h-flip v-flip rotate90 expected-uvs]
            [[true false false [[1.0 1.0] [1.0 0.0] [0.0 0.0] [0.0 0.0] [0.0 1.0] [1.0 1.0]]]
             [false true false [[0.0 0.0] [0.0 1.0] [1.0 1.0] [1.0 1.0] [1.0 0.0] [0.0 0.0]]]
             [false false true [[0.0 1.0] [0.0 0.0] [1.0 0.0] [1.0 0.0] [1.0 1.0] [0.0 1.0]]]]]
      (let [vertices (layer-vertices h-flip v-flip rotate90)]
        (is (quad-triangles? vertices))
        (is (= expected-uvs (mapv #(subvec % 3) vertices))))))

  (testing "brush"
    (let [vbuf (tile-map/gen-brush-vbuf {:width 1
                                         :height 1
                                         :tiles [{:tile 0
                                                  :h-flip false
                                                  :v-flip false
                                                  :rotate90 false}]}
                                        [nil]
                                        2
                                        3)]
      (is (quad-triangles? (vertex-buffer->vertices vbuf 5)))))

  (testing "palette tiles"
    (let [tile-source-attributes {:width 2
                                  :height 3
                                  :tiles-per-column 1
                                  :tiles-per-row 1}
          texture-set-data {:uv-transforms [nil]
                            :texture-set {:tile-count 1}}
          vbuf (tile-map/gen-palette-tiles-vbuf tile-source-attributes texture-set-data)]
      (is (quad-triangles? (vertex-buffer->vertices vbuf 5)))))

  (testing "palette grid"
    (let [vbuf (tile-map/gen-palette-grid-vbuf {:width 2
                                                :height 3
                                                :visual-width 2
                                                :visual-height 3
                                                :tiles-per-column 1
                                                :tiles-per-row 1})
          vertices (vertex-buffer->vertices vbuf 7)]
      (is (= 24 (count vertices)))
      (is (every? quad-triangles? (partition 6 vertices))))))

(deftest tile-map-outline
  (testing "shows all layers"
    (test-util/with-loaded-project
      (let [node-id (test-util/resource-node project "/tilegrid/with_layers.tilemap")
            outline (g/node-value node-id :node-outline)]
        (is (= (localization/message "outline.tile-map") (:label outline)))
        (is (= #{"layer1" "layer2" "blaha"} (set (map :label (:children outline)))))))))

(deftest tile-map-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/tilegrid/with_layers.tilemap")]
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

(deftest tile-map-cell-order-deterministic
  (test-util/with-loaded-project
    (let [tilemap-id (test-util/resource-node project "/tilegrid/with_layers.tilemap")
          layer-ids (map gt/source-id (g/inputs (g/now) tilemap-id :layer-msgs))
          layer-id (some #(when (= "layer1" (g/node-value % :id)) %) layer-ids)]
      (when (is (some? layer-id))
        (let [cell-map (g/node-value layer-id :cell-map)
              brush-tile (some (comp (juxt :x :y) val) cell-map)
              brush (tile-map/make-brush-from-selection cell-map brush-tile brush-tile)
              cell-map' (reduce (fn [cell-map' pos]
                                  (tile-map/paint cell-map' pos brush))
                                cell-map
                                (take 128 (partition 2 (repeatedly #(rand-int 64)))))]
          (is (= (map val (sort-by key cell-map'))
                 (vals cell-map'))))))))

(deftest tile-map-rotate-brush-90-degrees
  ;; Initial brush:
  ;; 1 2 3
  ;; 4 5 6
  (let [brush {:width 3 :height 2 :tiles [{:x 0 :y 0 :tile 1 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 0 :tile 2 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 0 :tile 3 :h-flip false :v-flip false :rotate90 false}
                                          {:x 0 :y 1 :tile 4 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 1 :tile 5 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 1 :tile 6 :h-flip false :v-flip false :rotate90 false}]}
        rotated-brush (tile-map/rotate-brush-90-degrees brush)
        rotated-brush-180 (tile-map/rotate-brush-90-degrees rotated-brush)
        rotated-brush-270 (tile-map/rotate-brush-90-degrees rotated-brush-180)]
    ;; After 90-degree rotation
    ;; 3 6
    ;; 2 5
    ;; 1 4
    (is (= 2 (:width rotated-brush)))
    (is (= 3 (:height rotated-brush)))
    (is (= [{:x 2, :y 0, :tile 3, :h-flip false, :v-flip false, :rotate90 true}
            {:x 2, :y 1, :tile 6, :h-flip false, :v-flip false, :rotate90 true}
            {:x 1, :y 0, :tile 2, :h-flip false, :v-flip false, :rotate90 true}
            {:x 1, :y 1, :tile 5, :h-flip false, :v-flip false, :rotate90 true}
            {:x 0, :y 0, :tile 1, :h-flip false, :v-flip false, :rotate90 true}
            {:x 0, :y 1, :tile 4, :h-flip false, :v-flip false, :rotate90 true}]
           (:tiles rotated-brush)))
    ;; After 180-degree rotation
    ;; 6 5 4
    ;; 3 2 1
    (is (= 3 (:width rotated-brush-180)))
    (is (= 2 (:height rotated-brush-180)))
    (is (= [{:x 2, :y 1, :tile 6, :h-flip true, :v-flip true, :rotate90 false}
            {:x 1, :y 1, :tile 5, :h-flip true, :v-flip true, :rotate90 false}
            {:x 0, :y 1, :tile 4, :h-flip true, :v-flip true, :rotate90 false}
            {:x 2, :y 0, :tile 3, :h-flip true, :v-flip true, :rotate90 false}
            {:x 1, :y 0, :tile 2, :h-flip true, :v-flip true, :rotate90 false}
            {:x 0, :y 0, :tile 1, :h-flip true, :v-flip true, :rotate90 false}]
           (:tiles rotated-brush-180)))
    ;; After 270-degree rotation
    ;; 4 1
    ;; 5 2
    ;; 6 3
    (is (= 2 (:width rotated-brush-270)))
    (is (= 3 (:height rotated-brush-270)))
    (is (= [{:x 0, :y 1, :tile 4, :h-flip true, :v-flip true, :rotate90 true}
            {:x 0, :y 0, :tile 1, :h-flip true, :v-flip true, :rotate90 true}
            {:x 1, :y 1, :tile 5, :h-flip true, :v-flip true, :rotate90 true}
            {:x 1, :y 0, :tile 2, :h-flip true, :v-flip true, :rotate90 true}
            {:x 2, :y 1, :tile 6, :h-flip true, :v-flip true, :rotate90 true}
            {:x 2, :y 0, :tile 3, :h-flip true, :v-flip true, :rotate90 true}]
           (:tiles rotated-brush-270)))))

(deftest tile-map-flip-brush-horizontally
  ;; Initial brush:
  ;; 1 2 3
  ;; 4 5 6
  (let [brush {:width 3 :height 2 :tiles [{:x 0 :y 0 :tile 1 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 0 :tile 2 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 0 :tile 3 :h-flip false :v-flip false :rotate90 false}
                                          {:x 0 :y 1 :tile 4 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 1 :tile 5 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 1 :tile 6 :h-flip false :v-flip false :rotate90 false}]}
        flipped-brush (tile-map/flip-brush-horizontally brush)]
    (is (= 3 (:width flipped-brush)))
    (is (= 2 (:height flipped-brush)))
    ;; After horizontal flip
    ;; 3 2 1
    ;; 6 5 4
    (is (= [{:x 2 :y 0 :tile 3 :h-flip true :v-flip false :rotate90 false}
            {:x 1 :y 0 :tile 2 :h-flip true :v-flip false :rotate90 false}
            {:x 0 :y 0 :tile 1 :h-flip true :v-flip false :rotate90 false}
            {:x 2 :y 1 :tile 6 :h-flip true :v-flip false :rotate90 false}
            {:x 1 :y 1 :tile 5 :h-flip true :v-flip false :rotate90 false}
            {:x 0 :y 1 :tile 4 :h-flip true :v-flip false :rotate90 false}]
           (:tiles flipped-brush)))))

(deftest tile-map-flip-brush-vertically
  ;; Initial brush:
  ;; 1 2 3
  ;; 4 5 6
  (let [brush {:width 3 :height 2 :tiles [{:x 0 :y 0 :tile 1 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 0 :tile 2 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 0 :tile 3 :h-flip false :v-flip false :rotate90 false}
                                          {:x 0 :y 1 :tile 4 :h-flip false :v-flip false :rotate90 false}
                                          {:x 1 :y 1 :tile 5 :h-flip false :v-flip false :rotate90 false}
                                          {:x 2 :y 1 :tile 6 :h-flip false :v-flip false :rotate90 false}]}
        flipped-brush (tile-map/flip-brush-vertically brush)]
    (is (= 3 (:width flipped-brush)))
    (is (= 2 (:height flipped-brush)))
    ;; After vertical flip
    ;; 4 5 6
    ;; 1 2 3
    (is (= [{:x 0 :y 1 :tile 4 :h-flip false :v-flip true :rotate90 false}
            {:x 1 :y 1 :tile 5 :h-flip false :v-flip true :rotate90 false}
            {:x 2 :y 1 :tile 6 :h-flip false :v-flip true :rotate90 false}
            {:x 0 :y 0 :tile 1 :h-flip false :v-flip true :rotate90 false}
            {:x 1 :y 0 :tile 2 :h-flip false :v-flip true :rotate90 false}
            {:x 2 :y 0 :tile 3 :h-flip false :v-flip true :rotate90 false}]
           (:tiles flipped-brush)))))

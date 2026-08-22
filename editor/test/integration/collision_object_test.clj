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

(ns integration.collision-object-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.collision-object :as collision-object]
            [editor.defold-project :as project]
            [editor.gl.pass :as pass]
            [editor.localization :as localization]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL2]))

(defn- outline-seq
  [outline]
  (map :label (tree-seq :children :children outline)))

(deftest new-collision-object
  (testing "A new collision object"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/new.collisionobject")
            scene     (g/node-value node-id :scene)
            outline   (g/node-value node-id :node-outline)]
        (is (not (nil? scene)))
        (is (empty? (:children scene)))
        (is (empty? (:children outline)))))))

(deftest collision-object-with-three-shapes
  (testing "A collision object with shapes"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")
            outline   (g/node-value node-id :node-outline)
            scene     (g/node-value node-id :scene)]
        (is (= 3 (count (:children scene))))
        (is (= [(localization/message "outline.collision-object")
                (localization/message "outline.unnamed-collision-shape" {"shape" (localization/message "command.edit.add-embedded-component.variant.collision-object.option.sphere")})
                (localization/message "outline.unnamed-collision-shape" {"shape" (localization/message "command.edit.add-embedded-component.variant.collision-object.option.box")})
                (localization/message "outline.unnamed-collision-shape" {"shape" (localization/message "command.edit.add-embedded-component.variant.collision-object.option.capsule")})]
               (outline-seq outline)))))))

(deftest add-shapes
  (testing "Adding a sphere"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
        (app-view/select! app-view [node-id])
        (test-util/handler-run :edit.add-embedded-component [{:name :workbench :env {:selection [node-id] :app-view app-view}}] {:shape-type :type-sphere})
        (let [outline (g/node-value node-id :node-outline)]
          (is (= 4 (count (:children outline))))
          (is (= (localization/message "outline.unnamed-collision-shape" {"shape" (localization/message "command.edit.add-embedded-component.variant.collision-object.option.sphere")})
                 (last (outline-seq outline)))))))))

(deftest shape-options-follow-project-physics-type
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/collision_object/new.collisionobject")
          game-project (test-util/resource-node project "/game.project")
          command-contexts [{:name :workbench :env {:selection [node-id] :app-view app-view}}]
          shape-types (fn []
                        (into #{}
                              (map #(get-in % [:user-data :shape-type]))
                              (test-util/handler-options :edit.add-embedded-component command-contexts nil)))]
      (is (contains? (shape-types) :type-hull))
      (with-open [_ (test-util/make-system-reverter)]
        (test-util/set-setting! game-project ["physics" "type"] "2D")
        (is (not (contains? (shape-types) :type-hull)))
        (is (not (contains? (shape-types) :type-mesh)))))))

(deftest validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
      (testing "collision object"
        (test-util/with-prop [node-id :mass 0]
          (is (g/error? (test-util/prop-error node-id :mass))))
        (let [r (workspace/resolve-workspace-resource workspace "/nope.convexshape")]
          (test-util/with-prop [node-id :collision-shape r]
            (is (g/error? (test-util/prop-error node-id :collision-shape))))))
      (doseq [[type index props] [["sphere" 0 {:diameter 0.0}]
                                  ["box" 1 {:dimensions [0.0 0.0 0.0]}]
                                  ["capsule" 2 {:diameter 0.0
                                                :height -0.001}]]]
        (testing type
          (let [shape (:node-id (test-util/outline node-id [index]))]
            (doseq [[prop value] props]
              (test-util/with-prop [shape prop value]
                (is (g/error? (test-util/prop-error shape prop)))))))))))

(deftest shape-errors-block-build-targets
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
      (doseq [[type index props] [["sphere" 0 {:diameter 0.0}]
                                  ["box" 1 {:dimensions [0.0 0.0 0.0]}]
                                  ["capsule" 2 {:diameter 0.0
                                                :height -0.001}]]]
        (testing (str type " shape error blocks build targets")
          (let [shape (:node-id (test-util/outline node-id [index]))]
            (doseq [[prop value] props]
              (test-util/with-prop [shape prop value]
                (is (g/error? (g/node-value node-id :build-targets)))))))))))

(deftest mesh-shape-storage-and-compilation
  (let [source-key ["/models/level.gltf" "Ground" 3]
        source-shape {:shape-type :type-mesh
                      :id "ground"
                      :mesh-scene (nth source-key 0)
                      :mesh-name (nth source-key 1)
                      :mesh-index (nth source-key 2)
                      :data []}
        editable (#'collision-object/make-embedded-collision-shape
                   [{:shape-type :type-sphere :data [0.5]}
                    source-shape])
        editable-mesh (second (:shapes editable))]
    (testing "editable shapes contain only the source selection"
      (is (= 0 (:index editable-mesh)))
      (is (= 0 (:count editable-mesh)))
      (is (= (subvec source-key 0 2)
             ((juxt :mesh-scene :mesh-name) editable-mesh)))
      (is (= 3 (:mesh-index editable-mesh)))
      (is (not (contains? editable :indices))))

    (testing "compiled shapes share local vertex and triangle ranges"
      (let [mesh {:primitives [{:positions (float-array [0.0 0.0 0.0
                                                          1.0 0.0 0.0
                                                          0.0 1.0 0.0])
                                :indices (int-array [0 1 2])}
                               {:positions (float-array [0.0 0.0 1.0
                                                          1.0 0.0 1.0
                                                          0.0 1.0 1.0])
                                :indices (int-array [0 2 1])}]}
            collision-object-desc {:embedded-collision-shape
                                   {:data [0.5]
                                    :shapes [{:shape-type :type-sphere :index 0 :count 1}
                                             (dissoc source-shape :data)
                                             (assoc (dissoc source-shape :data) :id "ground-copy")]}}
            compiled (#'collision-object/compile-mesh-shapes
                       collision-object-desc
                       [{:source-key source-key :mesh mesh}])
            collision-shape (:embedded-collision-shape compiled)
            [_ first-mesh second-mesh] (:shapes collision-shape)]
        (is (= 19 (count (:data collision-shape))))
        (is (= [0 1 2 3 5 4] (:indices collision-shape)))
        (is (= {:index 1 :count 18 :triangle-index 0 :triangle-count 2}
               (select-keys first-mesh [:index :count :triangle-index :triangle-count])))
        (is (= (select-keys first-mesh [:index :count :triangle-index :triangle-count])
               (select-keys second-mesh [:index :count :triangle-index :triangle-count])))
        (is (coll/empty? (select-keys first-mesh [:mesh-scene :mesh-name :mesh-index])))))))

(deftest hull-shape-storage-and-compilation
  (let [source-key ["/models/level.gltf" "Ground" 3]
        source-shape {:shape-type :type-hull
                      :id "ground-hull"
                      :mesh-scene (nth source-key 0)
                      :mesh-name (nth source-key 1)
                      :mesh-index (nth source-key 2)
                      :data []}
        editable (#'collision-object/make-embedded-collision-shape
                   [{:shape-type :type-sphere :data [0.5]}
                    source-shape])
        editable-hull (second (:shapes editable))]
    (testing "editable Hull shapes contain only the source selection"
      (is (= 0 (:index editable-hull)))
      (is (= 0 (:count editable-hull)))
      (is (= (subvec source-key 0 2)
             ((juxt :mesh-scene :mesh-name) editable-hull)))
      (is (= 3 (:mesh-index editable-hull)))
      (is (not (contains? editable :indices))))

    (testing "compiled Hull shapes share vertex ranges without triangle data"
      (let [mesh {:primitives [{:positions (float-array [0.0 0.0 0.0
                                                          1.0 0.0 0.0
                                                          0.0 1.0 0.0])
                                :indices (int-array [0 1 2])}
                               {:positions (float-array [0.0 0.0 1.0
                                                          1.0 0.0 1.0
                                                          0.0 1.0 1.0])
                                :indices (int-array [0 2 1])}]}
            collision-object-desc {:embedded-collision-shape
                                   {:data [0.5]
                                    :shapes [{:shape-type :type-sphere :index 0 :count 1}
                                             (dissoc source-shape :data)
                                             (assoc (dissoc source-shape :data) :id "ground-hull-copy")]}}
            compiled (#'collision-object/compile-mesh-shapes
                       collision-object-desc
                       [{:source-key source-key :mesh mesh}])
            collision-shape (:embedded-collision-shape compiled)
            [_ first-hull second-hull] (:shapes collision-shape)]
        (is (= 19 (count (:data collision-shape))))
        (is (coll/empty? (:indices collision-shape)))
        (is (= {:index 1 :count 18}
               (select-keys first-hull [:index :count])))
        (is (= (select-keys first-hull [:index :count])
               (select-keys second-hull [:index :count])))
        (is (coll/empty? (select-keys first-hull [:triangle-index :triangle-count
                                                  :mesh-scene :mesh-name :mesh-index])))))))

(deftest mesh-source-shape-preview
  (let [hull-scene
        (collision-object/produce-mesh-shape-scene
          {:_node-id :hull
           :pose nil
           :color [1.0 1.0 1.0 1.0]
           :node-outline-key "Hull"
           :shape-type :type-hull
           :selected-collision-mesh
           {:primitives [{:positions (float-array [0.0 0.0 0.0
                                                   1.0 0.0 0.0
                                                   0.0 1.0 0.0])
                          :indices (int-array [0 1 1 2])
                          :triangles false}]}})

        mesh-scene
        (collision-object/produce-mesh-shape-scene
          {:_node-id :mesh
           :pose nil
           :color [1.0 1.0 1.0 1.0]
           :node-outline-key "Mesh"
           :shape-type :type-mesh
           :selected-collision-mesh
           {:primitives [{:positions (float-array [0.0 0.0 0.0
                                                   1.0 0.0 0.0
                                                   0.0 1.0 0.0])
                          :indices (int-array [0 1 2])
                          :triangles true}]}})]
    (testing "Hull preview displays every source position as a point"
      (is (= GL2/GL_POINTS (get-in hull-scene [:renderable :user-data :geometry :primitive-type])))
      (is (= 3 (count (get-in hull-scene [:renderable :user-data :geometry :vbuf]))))
      (is (= [pass/outline] (get-in hull-scene [:renderable :passes]))))

    (testing "Mesh preview remains a double-sided triangle surface"
      (is (= GL2/GL_TRIANGLES (get-in mesh-scene [:renderable :user-data :geometry :primitive-type])))
      (is (true? (get-in mesh-scene [:renderable :user-data :double-sided])))
      (is (= [pass/transparent pass/selection] (get-in mesh-scene [:renderable :passes]))))))

(deftest mesh-shape-source-selection-survives-load
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/collision_object/mesh_shape.collisionobject")
          shape-node-id (:node-id (test-util/outline node-id [0]))]
      (is (= (workspace/find-resource workspace "/mesh/quad.gltf")
             (test-util/prop shape-node-id :mesh-scene)))
      (is (= "Plane_001Mesh" (test-util/prop shape-node-id :mesh-name)))
      (is (= 0 (test-util/prop shape-node-id :mesh-index)))
      (is (= [:id :position :rotation :mesh-scene :mesh-index]
             (:display-order (properties/coalesce [(g/node-value shape-node-id :_properties)]))))
      (is (not (g/error? (g/node-value node-id :build-targets)))))))

(deftest hull-shape-source-selection-survives-load
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/collision_object/hull_shape.collisionobject")
          shape-node-id (:node-id (test-util/outline node-id [0]))]
      (is (= (workspace/find-resource workspace "/mesh/quad.gltf")
             (test-util/prop shape-node-id :mesh-scene)))
      (is (= "Plane_001Mesh" (test-util/prop shape-node-id :mesh-name)))
      (is (= 0 (test-util/prop shape-node-id :mesh-index)))
      (is (not (g/error? (g/node-value node-id :build-targets)))))))

(deftest group-property-read-only-for-tilemap
  (test-util/with-loaded-project
    (testing "group is editable when collision-shape is nil"
      (let [node-id (test-util/resource-node project "/collision_object/new.collisionobject")]
        (is (not (test-util/prop-read-only? node-id :group)))))

    (testing "group is read-only when collision-shape is a tilemap"
      (let [node-id (test-util/resource-node project "/collision_object/tile_map_collision_shape.collisionobject")]
        (is (test-util/prop-read-only? node-id :group))))

    (testing "existing group value is preserved under a tilemap collision-shape"
      (let [node-id (test-util/resource-node project "/collision_object/tile_map_collision_shape.collisionobject")]
        (is (= "default" (test-util/prop node-id :group)))))))

(deftest manip-scale-preserves-types
  (test-util/with-loaded-project
    (let [collision-object-path "/collision_object/three_shapes.collisionobject"
          collision-object (project/get-resource-node project collision-object-path)
          [[sphere-shape] [box-shape] [capsule-shape]] (g/sources-of collision-object :child-scenes)]

      (testing "Sphere Shape"
        (doseq [original-diameter [(float 10.0) (double 10.0)]]
          (with-open [_ (test-util/make-system-reverter)]
            (g/set-property! sphere-shape :diameter original-diameter)
            (test-util/manip-scale! sphere-shape [2.0 2.0 2.0])
            (test-util/ensure-number-type-preserving! original-diameter (g/node-value sphere-shape :diameter)))))

      (testing "Box Shape"
        (doseq [original-dimensions
                (mapv #(with-meta % {:version "original"})
                      [[(float 10.0) (float 10.0) (float 10.0)]
                       [(double 10.0) (double 10.0) (double 10.0)]
                       (vector-of :float 10.0 10.0 10.0)
                       (vector-of :double 10.0 10.0 10.0)])]
          (with-open [_ (test-util/make-system-reverter)]
            (g/set-property! box-shape :dimensions original-dimensions)
            (test-util/manip-scale! box-shape [2.0 2.0 2.0])
            (test-util/ensure-number-type-preserving! original-dimensions (g/node-value box-shape :dimensions)))))

      (testing "Capsule Shape"
        (doseq [original-value [(float 10.0) (double 10.0)]]
          (with-open [_ (test-util/make-system-reverter)]
            (g/set-properties! capsule-shape :diameter original-value :height original-value)
            (test-util/manip-scale! capsule-shape [2.0 2.0 2.0])
            (test-util/ensure-number-type-preserving! original-value (g/node-value capsule-shape :diameter))
            (test-util/ensure-number-type-preserving! original-value (g/node-value capsule-shape :height))))))))

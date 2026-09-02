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
  (:import [com.dynamo.gamesys.proto Physics$CollisionObjectDesc]
           [com.jogamp.opengl GL2]))

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

(deftest mesh-source-shape-storage
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
      (is (not (contains? editable :indices))))))

(deftest hull-source-shape-storage
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
      (is (not (contains? editable :indices))))))

(deftest mesh-source-shape-compilation
  (test-util/with-scratch-project "test/resources/test_project"
    (let [mesh-scene-node-id (test-util/resource-node project "/mesh/quad.gltf")
          mesh-set (get-in (g/node-value mesh-scene-node-id :content) [:mesh-set])]
      (is (coll/empty? (get-in mesh-set [:raw-models 0 :meshes])))
      (is (pos? (count (:models (g/node-value mesh-scene-node-id :collision-mesh-set))))))
    (doseq [[path triangles?] [["/collision_object/hull_shape.collisionobject" false]
                               ["/collision_object/mesh_shape.collisionobject" true]]]
      (let [node-id (test-util/resource-node project path)]
        (with-open [_ (test-util/build! node-id)]
          (let [collision-object (Physics$CollisionObjectDesc/parseFrom (test-util/node-build-output node-id))
                collision-shape (.getEmbeddedCollisionShape collision-object)
                shape (.getShapes collision-shape 0)]
            (is (pos? (.getDataCount collision-shape)))
            (is (pos? (.getCount shape)))
            (is (not (.hasMeshScene shape)))
            (is (not (.hasMeshName shape)))
            (is (not (.hasMeshIndex shape)))
            (if triangles?
              (do
                (is (pos? (.getIndicesCount collision-shape)))
                (is (pos? (.getTriangleCount shape))))
              (do
                (is (zero? (.getIndicesCount collision-shape)))
                (is (not (.hasTriangleIndex shape)))
                (is (not (.hasTriangleCount shape)))))))))))

(deftest mesh-source-shape-preview
  (test-util/with-loaded-project
    (doseq [[path primitive-type passes triangles?]
            [["/collision_object/hull_shape.collisionobject" GL2/GL_POINTS [pass/outline] false]
             ["/collision_object/mesh_shape.collisionobject" GL2/GL_TRIANGLES [pass/transparent pass/selection] true]]]
      (let [collision-object (test-util/resource-node project path)
            shape-node-id (:node-id (test-util/outline collision-object [0]))
            selected-renderable (g/node-value shape-node-id :selected-collision-mesh-renderable)
            scene (g/node-value shape-node-id :scene)
            renderable (get-in scene [:children 0 :renderable])
            geometry (get-in renderable [:user-data :geometry])]
        (is (= primitive-type (:primitive-type geometry)))
        (is (= passes (:passes renderable)))
        (is (not (contains? geometry :vbuf)))
        (is (identical? (get-in selected-renderable [:primitives 0 :position-buffer])
                        (:position-buffer geometry)))
        (if triangles?
          (do
            (is (some? (:index-buffer geometry)))
            (is (true? (get-in renderable [:user-data :double-sided]))))
          (do
            (is (nil? (:index-buffer geometry)))
            (is (pos? (get-in renderable [:user-data :point-count])))))))))

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

(deftest inline-hull-data-survives-load-save-and-build
  (test-util/with-scratch-project "test/resources/test_project"
    (let [node-id (test-util/resource-node project "/collision_object/inline_hull.collisionobject")
          shape-node-id (:node-id (test-util/outline node-id [0]))
          expected-data [-1.0 -1.0 -1.0
                         1.0 -1.0 -1.0
                         -1.0 1.0 -1.0
                         1.0 1.0 -1.0
                         -1.0 -1.0 1.0
                         1.0 -1.0 1.0
                         -1.0 1.0 1.0
                         1.0 1.0 1.0]]
      (is (= expected-data (test-util/prop shape-node-id :inline-data)))
      (is (false? (get-in (g/node-value shape-node-id :_properties)
                          [:properties :mesh-scene :visible])))
      (test-util/with-prop [node-id :friction 0.6]
        (let [embedded-collision-shape (:embedded-collision-shape (g/node-value node-id :save-value))
              shape (first (:shapes embedded-collision-shape))]
          (is (= expected-data (:data embedded-collision-shape)))
          (is (= 0 (:index shape)))
          (is (= 24 (:count shape)))
          (is (not (contains? shape :mesh-scene)))
          (is (not (contains? shape :mesh-name)))
          (is (not (contains? shape :mesh-index)))))
      (with-open [_ (test-util/build! node-id)]
        (let [collision-object (Physics$CollisionObjectDesc/parseFrom (test-util/node-build-output node-id))
              collision-shape (.getEmbeddedCollisionShape collision-object)
              shape (.getShapes collision-shape 0)]
          (is (= expected-data (mapv double (.getDataList collision-shape))))
          (is (= 0 (.getIndex shape)))
          (is (= 24 (.getCount shape))))))))

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

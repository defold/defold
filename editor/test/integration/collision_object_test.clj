;; Copyright 2020-2025 The Defold Foundation
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
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

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

(deftest validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/collision_object/three_shapes.collisionobject")]
      (testing "collision object"
               (test-util/with-prop [node-id :mass 0]
                 (is (g/error? (test-util/prop-error node-id :mass))))
               (let [r (workspace/resolve-workspace-resource workspace "/nope.convexshape")]
                 (test-util/with-prop [node-id :collision-shape r]
                   (is (g/error? (test-util/prop-error node-id :collision-shape))))))
      (doseq [[type index props] [["sphere" 0 {:diameter -1}]
                                  ["box" 1 {:dimensions [-1 1 1]}]
                                  ["capsule" 2 {:diameter -1
                                                :height -1}]]]
        (testing type
               (let [shape (:node-id (test-util/outline node-id [index]))]
                 (doseq [[prop value] props]
                   (test-util/with-prop [shape prop value]
                    (is (g/error? (test-util/prop-error shape prop)))))))))))

(deftest manip-scale-preserves-types
  (test-util/with-loaded-project
    (let [project-graph (g/node-id->graph-id project)
          collision-object-path "/collision_object/three_shapes.collisionobject"
          collision-object (project/get-resource-node project collision-object-path)
          [[sphere-shape] [box-shape] [capsule-shape]] (g/sources-of collision-object :child-scenes)]

      (testing "Sphere Shape"
        (doseq [original-diameter [(float 10.0) (double 10.0)]]
          (with-open [_ (test-util/make-graph-reverter project-graph)]
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
          (with-open [_ (test-util/make-graph-reverter project-graph)]
            (g/set-property! box-shape :dimensions original-dimensions)
            (test-util/manip-scale! box-shape [2.0 2.0 2.0])
            (test-util/ensure-number-type-preserving! original-dimensions (g/node-value box-shape :dimensions)))))

      (testing "Capsule Shape"
        (doseq [original-value [(float 10.0) (double 10.0)]]
          (with-open [_ (test-util/make-graph-reverter project-graph)]
            (g/set-properties! capsule-shape :diameter original-value :height original-value)
            (test-util/manip-scale! capsule-shape [2.0 2.0 2.0])
            (test-util/ensure-number-type-preserving! original-value (g/node-value capsule-shape :diameter))
            (test-util/ensure-number-type-preserving! original-value (g/node-value capsule-shape :height))))))))

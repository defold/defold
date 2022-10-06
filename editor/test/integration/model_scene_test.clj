;; Copyright 2020 The Defold Foundation
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

(ns integration.model-scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.math :as math]
            [editor.model-scene :as model-scene]
            [editor.types :as types])
  (:import [javax.vecmath Point3d]))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          aabb (g/node-value node-id :aabb)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)
          dist (.distance max min)] ; distance in meters (converted from centimeters in the loader)
      (is (< 10 dist 20)))))

(deftest vbs
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          mesh (first (g/node-value node-id :meshes))
          scene (g/node-value node-id :scene)
          user-data (get-in scene [:renderable :user-data])
          meshes (:meshes user-data)
          mesh (first meshes)
          indices (:indices mesh)
          vb (-> (model-scene/->vtx-pos-nrm-tex (alength ^ints indices))
               (model-scene/mesh->vb! (math/->mat4) :vertex-space-world mesh (get user-data :scratch-arrays)))]
      (is (= (count vb) (alength (get mesh :indices)))))))

(deftest invalid-scene
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/invalid.dae")
          scene (g/node-value node-id :scene)]
      (is (g/error? scene)))))

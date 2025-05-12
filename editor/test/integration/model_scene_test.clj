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

(ns integration.model-scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.model-scene :as model-scene]
            [editor.types :as types]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [javax.vecmath Point3d]))

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          scene (g/node-value node-id :scene)
          aabb (:aabb scene)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)
          dist (.distance max min)] ; distance in meters (converted from centimeters in the loader)
      (is (< 10 dist 20)))))

(deftest vbs
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/test.dae")
          scene (g/node-value node-id :scene)
          user-data (get-in scene [:renderable :user-data])
          world-transform (math/->mat4)
          normal-transform (math/->mat4)
          vertex-attribute-bytes (byte-array 0)
          mesh-renderable-data (:mesh-renderable-data user-data)
          vertex-count (count (:position-data mesh-renderable-data))
          vb (-> (->vtx-pos-nrm-tex vertex-count)
                 (model-scene/mesh->vb! world-transform normal-transform false false vertex-attribute-bytes mesh-renderable-data))]
      (is (= vertex-count (count vb))))))

(deftest invalid-scene
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/invalid.dae")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (g/error? scene)))))

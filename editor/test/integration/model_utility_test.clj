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

(ns integration.model-utility-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [editor.model-loader :as model-loader]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(defn- load-scene [workspace project file-path]
  (let [resource (workspace/file-resource workspace file-path)
        node-id (project/get-resource-node project resource)]
    (model-loader/load-scene node-id resource (project/settings project))))

(deftest mesh-normals
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/quad.gltf")
          content mesh-set
          normals (->> (get-in content [:models 0 :meshes 0 :normals])
                       (partition 3))]
      (is (seq normals))
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  normals)))))

(deftest mesh-texcoords
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/quad.gltf")
          content mesh-set
          texcoords (->> (get-in content [:models 0 :meshes 0 :texcoord0])
                         (partition 2))]
      (is (seq texcoords))
      (is (every? (fn [[u v]]
                    (and (<= 0.0 u 1.0)
                         (<= 0.0 v 1.0)))
                  texcoords)))))

(deftest bones
  (test-util/with-loaded-project
    (let [{:keys [animation-set bones]} (load-scene workspace project "/mesh/treasure_chest.gltf")]
      (is (= 3 (count bones)))
      (is (set/subset? (:bone-list animation-set) (set (map :id bones)))))))

(deftest external-buffers
  (test-util/with-loaded-project
    (let [{:keys [buffers]} (load-scene workspace project "/mesh/triangle/gltf/Triangle.gltf")]
      (is (= 1 (count buffers)))
      (is (= "simpleTriangle.bin" (.uri (first buffers))))
      (is (= 44 (count (.buffer (first buffers))))))))

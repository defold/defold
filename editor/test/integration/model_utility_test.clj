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

(ns integration.model-utility-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.model-loader :as model-loader]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(defn- sequence-buffer-in-mesh [mesh data-stride data-key index-key]
  (let [partitioned-data (vec (partition data-stride (mesh data-key)))
        sequenced-data (into [] (mapcat #(partitioned-data %) (mesh index-key)))
        sequenced-indices (range (/ (count sequenced-data) data-stride))]
    (assoc mesh
      data-key sequenced-data
      index-key sequenced-indices)))

(defn- sequence-vertices-in-mesh [mesh]
  (-> mesh
      (sequence-buffer-in-mesh 3 :positions :position-indices)
      (sequence-buffer-in-mesh 3 :normals :normals-indices)
      (sequence-buffer-in-mesh 2 :texcoord0 :texcoord0-indices)))

(defn- sequence-vertices-in-mesh-entry [mesh-entry]
  (update mesh-entry :meshes #(mapv sequence-vertices-in-mesh %)))

(defn- sequence-vertices-in-mesh-set [mesh-set]
  (update mesh-set :mesh-entries #(mapv sequence-vertices-in-mesh-entry %)))

(defn- load-scene [workspace project file-path]
  (let [resource (workspace/file-resource workspace file-path)
        node-id (project/get-resource-node project resource)]
    (model-loader/load-scene node-id resource)))

(deftest mesh-normals
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/box_blender.dae")
          content (sequence-vertices-in-mesh-set mesh-set)]
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  (->> (get-in content [:mesh-entries 0 :meshes 0 :normals])
                    (partition 3)))))))

(deftest mesh-texcoords
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace project "/mesh/plane.dae")
          content (sequence-vertices-in-mesh-set mesh-set)]
      (let [c (get-in content [:mesh-entries 0 :meshes 0])
            zs (map #(nth % 2) (partition 3 (:positions c)))
            vs (map second (partition 2 (:texcoord0 c)))]
        (is (= vs (map (fn [y] (- 1.0 (* (+ y 1.0) 0.5))) zs)))))))

(deftest comma-decimal-points-throws-number-format-exception
  (test-util/with-loaded-project
    (is (g/error? (load-scene workspace project "/mesh/test_autodesk_dae.dae")))))

(deftest bones
  (test-util/with-loaded-project
    (let [{:keys [animation-set bones]} (load-scene workspace project "/mesh/treasure_chest.dae")]
      (is (= 3 (count bones)))
      (is (set/subset? (:bone-list animation-set) (set (map :id bones)))))))

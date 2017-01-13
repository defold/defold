(ns integration.collada-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.collada :as collada]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(defn- sequence-buffer-in-mesh [mesh data-stride data-key index-key]
  (let [partitioned-data (vec (partition data-stride (mesh data-key)))
        sequenced-data (into [] (mapcat #(partitioned-data %) (mesh index-key)))
        sequenced-indices (range (/ (count sequenced-data) data-stride))]
    (assoc mesh
      data-key sequenced-data
      index-key sequenced-indices)))

(defn- sequence-vertices-in-mesh [mesh]
  (-> mesh
      (sequence-buffer-in-mesh 3 :positions :indices)
      (sequence-buffer-in-mesh 3 :normals :normals-indices)
      (sequence-buffer-in-mesh 2 :texcoord0 :texcoord0-indices)))

(defn- sequence-vertices-in-mesh-entry [mesh-entry]
  (update mesh-entry :meshes #(mapv sequence-vertices-in-mesh %)))

(defn- sequence-vertices-in-mesh-set [mesh-set]
  (update mesh-set :mesh-entries #(mapv sequence-vertices-in-mesh-entry %)))

(defn- load-mesh-set [workspace file-path]
  (with-open [stream (->> file-path
                          (workspace/file-resource workspace)
                          io/input-stream)]
    (collada/->mesh-set stream)))

(deftest normals
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (load-mesh-set workspace "/mesh/box_blender.dae")
                    sequence-vertices-in-mesh-set)]
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  (->> (get-in content [:mesh-entries 0 :meshes 0 :normals])
                    (partition 3)))))))

(deftest texcoords
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          content (-> (load-mesh-set workspace "/mesh/plane.dae")
                    sequence-vertices-in-mesh-set)]
      (let [c (get-in content [:mesh-entries 0 :meshes 0])
            zs (map #(nth % 2) (partition 3 (:positions c)))
            vs (map second (partition 2 (:texcoord0 c)))]
        (is (= vs (map (fn [y] (- 1.0 (* (+ y 1.0) 0.5))) zs)))))))

(deftest comma-decimal-points-throws-number-format-exception
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)]
      (is (thrown? NumberFormatException (load-mesh-set workspace "/mesh/test_autodesk_dae.dae"))))))

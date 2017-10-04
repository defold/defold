(ns integration.collada-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.collada :as collada]
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
      (sequence-buffer-in-mesh 3 :positions :indices)
      (sequence-buffer-in-mesh 3 :normals :normals-indices)
      (sequence-buffer-in-mesh 2 :texcoord0 :texcoord0-indices)))

(defn- sequence-vertices-in-mesh-entry [mesh-entry]
  (update mesh-entry :meshes #(mapv sequence-vertices-in-mesh %)))

(defn- sequence-vertices-in-mesh-set [mesh-set]
  (update mesh-set :mesh-entries #(mapv sequence-vertices-in-mesh-entry %)))

(defn- load-scene [workspace file-path]
  (with-open [stream (->> file-path
                          (workspace/file-resource workspace)
                          io/input-stream)]
    (collada/load-scene stream)))

(deftest mesh-normals
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace "/mesh/box_blender.dae")
          content (sequence-vertices-in-mesh-set mesh-set)]
      (is (every? (fn [[x y z]]
                    (< (Math/abs (- (Math/sqrt (+ (* x x) (* y y) (* z z))) 1.0)) 0.000001))
                  (->> (get-in content [:mesh-entries 0 :meshes 0 :normals])
                    (partition 3)))))))

(deftest mesh-texcoords
  (test-util/with-loaded-project
    (let [{:keys [mesh-set]} (load-scene workspace "/mesh/plane.dae")
          content (sequence-vertices-in-mesh-set mesh-set)]
      (let [c (get-in content [:mesh-entries 0 :meshes 0])
            zs (map #(nth % 2) (partition 3 (:positions c)))
            vs (map second (partition 2 (:texcoord0 c)))]
        (is (= vs (map (fn [y] (- 1.0 (* (+ y 1.0) 0.5))) zs)))))))

(deftest comma-decimal-points-throws-number-format-exception
  (test-util/with-loaded-project
    (is (thrown? NumberFormatException (load-scene workspace "/mesh/test_autodesk_dae.dae")))))

(defn- remove-empty-channels [track]
  (into {} (filter (comp seq val)) track))

(deftest animations
  (test-util/with-loaded-project
    (let [{:keys [animation-set skeleton]} (load-scene workspace "/mesh/treasure_chest.dae")
          bone-count (count (:bones skeleton))]
      (is (= 1 (-> animation-set :animations count)))
      (let [animation (-> animation-set :animations first)]
        (is (= "" (:id animation)))
        (is (= 0 (count (:event-tracks animation))))
        (is (= 0 (count (:mesh-tracks animation))))
        (is (= 0 (count (:ik-tracks animation))))

        (testing "All bones are animated"
          (is (<= bone-count (count (:tracks animation)))))

        ; Tracks contain keyframes for position, rotation and scale channels.
        ; Several tracks can target the same bone, but there should not be
        ; multiple tracks that target the same channel for a bone.
        (doseq [[bone-index data-by-channel] (->> (:tracks animation)
                                                  (group-by :bone-index)
                                                  (sort-by key)
                                                  (map (fn [[bone-index bone-tracks]]
                                                         [bone-index (->> bone-tracks
                                                                          (map #(dissoc % :bone-index))
                                                                          (map remove-empty-channels)
                                                                          (apply merge-with (constantly ::conflicting-data)))])))]
          (testing "Bone exists in skeleton"
            (is (< bone-index bone-count)))

          (testing "Channels are not animated by multiple tracks"
            (doseq [[channel data] data-by-channel]
              (is (not= ::conflicting-data data)
                  (str "Found multiple tracks targetting " channel " for bone " bone-index))))

          (testing "Channel data matches expected strides"
            (are [stride channel]
              (= 0 (mod (count (data-by-channel channel)) stride))
              3 :positions
              4 :rotations
              3 :scale))

          (testing "At least two keys per channel"
            (are [stride channel]
              (<= (* 2 stride) (count (data-by-channel channel)))
              3 :positions
              4 :rotations
              3 :scale)))))))

(deftest bones
  (test-util/with-loaded-project
    (let [{:keys [animation-set skeleton]} (load-scene workspace "/mesh/treasure_chest.dae")]
      (is (= 3 (count (:bones skeleton))))
      (is (set/subset? (:bone-list animation-set) (set (map :id (:bones skeleton))))))))

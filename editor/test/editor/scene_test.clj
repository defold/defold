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

(ns editor.scene-test
  (:require [clojure.test :refer :all]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.scene :as scene])
  (:import [javax.vecmath Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest overlay-selected-node-properties-test
  (let [selected-node-properties
        [{:node-id 1
          :properties {:position {:value [0.0 0.0 0.0]
                                  :original-value [9.0 9.0 9.0]}
                       :rotation {:value [0.0 0.0 0.0 1.0]}}}
         {:node-id 2
          :properties {:position {:value [10.0 0.0 0.0]}
                       :scale {:value [1.0 1.0 1.0]}}}]]
    (testing "overlays preview values onto matching node-id/property pairs"
      (is (= [{:node-id 1
               :properties {:position {:value [1.0 2.0 3.0]
                                       :original-value [9.0 9.0 9.0]}
                            :rotation {:value [0.0 0.0 0.0 1.0]}}}
              {:node-id 2
               :properties {:position {:value [20.0 0.0 0.0]}
                            :scale {:value [3.0 3.0 3.0]}}}]
             (scene/overlay-selected-node-properties selected-node-properties
                                                     {1 {:position [1.0 2.0 3.0]}
                                                      2 {:position [20.0 0.0 0.0]
                                                         :scale [3.0 3.0 3.0]}}))))

    (testing "ignores unmatched preview entries"
      (is (= selected-node-properties
             (scene/overlay-selected-node-properties selected-node-properties
                                                     {3 {:position [1.0 2.0 3.0]}
                                                      1 {:scale [2.0 2.0 2.0]}}))))

    (testing "returns the original payload when preview overrides are absent"
      (is (= selected-node-properties
             (scene/overlay-selected-node-properties selected-node-properties nil))))))

(deftest flatten-scene-renderables-same-node-id-preview-overrides-test
  (let [preview-user-data (atom [])
        scene {:node-id 0
               :renderable {:passes [pass/transparent]
                            :batch-key :batch
                            :select-batch-key :batch}
               :children [{:node-id 1
                           :renderable {:passes [pass/transparent]
                                        :batch-key :batch
                                        :select-batch-key :batch}
                           :children [{:node-id 1
                                       :renderable {:passes [pass/transparent]
                                                    :batch-key :batch
                                                    :select-batch-key :batch
                                                    :preview-fn (fn [local-aabb user-data override-value-by-prop-kw]
                                                                  (swap! preview-user-data conj override-value-by-prop-kw)
                                                                  [local-aabb user-data])}}]}]}
        preview-overrides {1 {:position [1.0 2.0 3.0]
                              :custom :value}}
        renderables-by-pass (-> (#'scene/make-pass-renderables)
                                (#'scene/flatten-scene-renderables!
                                  true
                                  true
                                  scene
                                  preview-overrides
                                  #{}
                                  #{}
                                  #{}
                                  geom/Identity4d
                                  []
                                  [(:node-id scene)]
                                  geom/NoRotation
                                  (Vector3d. 1.0 1.0 1.0)
                                  geom/Identity4d
                                  (fn [_node-id] 1))
                                (#'scene/persist-pass-renderables!))
        [_root-renderable parent-renderable child-renderable] (get renderables-by-pass pass/transparent)]
    (testing "transform preview overrides are only applied once for repeated node ids"
      (is (= [1.0 2.0 3.0]
             [(.x ^Vector3d (:world-translation parent-renderable))
              (.y ^Vector3d (:world-translation parent-renderable))
              (.z ^Vector3d (:world-translation parent-renderable))]))
      (is (= [1.0 2.0 3.0]
             [(.x ^Vector3d (:world-translation child-renderable))
              (.y ^Vector3d (:world-translation child-renderable))
              (.z ^Vector3d (:world-translation child-renderable))])))
    (testing "child preview-fn still receives non-transform overrides"
      (is (= [{:custom :value}] @preview-user-data)))))

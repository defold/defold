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
            [editor.gl.pass :as pass]
            [editor.math :as math]
            [editor.scene :as scene])
  (:import [javax.vecmath Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest displayed-node-properties-test
  (let [selected-node-properties
        [{:node-id 1
          :properties {:position {:value [0.0 0.0 0.0]
                                  :original-value [9.0 9.0 9.0]}
                       :rotation {:value [0.0 0.0 0.0 1.0]}}}
         {:node-id 2
          :properties {:position {:value [10.0 0.0 0.0]}
                       :scale {:value [1.0 1.0 1.0]}}}]]

    (testing "Overlays preview values onto matching node-id/property pairs."
      (is (= [{:node-id 1
               :properties {:position {:value [1.0 2.0 3.0]
                                       :original-value [9.0 9.0 9.0]}
                            :rotation {:value [0.0 0.0 0.0 1.0]}}}
              {:node-id 2
               :properties {:position {:value [20.0 0.0 0.0]}
                            :scale {:value [3.0 3.0 3.0]}}}]
             (scene/displayed-node-properties
               selected-node-properties
               {1 {:position [1.0 2.0 3.0]}
                2 {:position [20.0 0.0 0.0]
                   :scale [3.0 3.0 3.0]}}))))

    (testing "Ignores unmatched preview entries."
      (is (= selected-node-properties
             (scene/displayed-node-properties
               selected-node-properties
               {3 {:position [1.0 2.0 3.0]}
                1 {:scale [2.0 2.0 2.0]}}))))

    (testing "Returns the original input when there are no preview overrides."
      (is (identical? selected-node-properties
                      (scene/displayed-node-properties
                        selected-node-properties
                        nil)))
      (is (identical? selected-node-properties
                      (scene/displayed-node-properties
                        selected-node-properties
                        {}))))))

(deftest flatten-scene-renderables-same-node-id-preview-overrides-test
  (let [preview-fn-overrides-atom (atom [])

        preview-fn
        (fn preview-fn [local-aabb user-data prop-kw->override-value]
          (swap! preview-fn-overrides-atom conj prop-kw->override-value)
          [local-aabb user-data])

        scene
        {:node-id 0
         :renderable {:passes [pass/transparent]}
         :children [{:node-id 1
                     :renderable {:passes [pass/transparent]}
                     :children [{:node-id 1
                                 :renderable {:passes [pass/transparent]
                                              :preview-fn preview-fn}}]}]}

        preview-overrides
        {1 {:position [1.0 2.0 3.0]
            :custom :value}}

        renderables-by-pass (#'scene/flatten-scene scene preview-overrides #{} #{} #{} math/identity-mat4)
        [_ parent-renderable child-renderable] (get renderables-by-pass pass/transparent)]

    (testing "Transform preview overrides are only applied once for repeated node ids."
      (is (= (Vector3d. 1.0 2.0 3.0) (:world-translation parent-renderable)))
      (is (= (Vector3d. 1.0 2.0 3.0) (:world-translation child-renderable))))

    (testing "Child :preview-fn still receives non-transform overrides."
      (is (= [{:custom :value}] @preview-fn-overrides-atom)))))

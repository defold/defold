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
            [editor.scene :as scene]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest collapse-property-preview-overrides-test
  (testing "collapses node-id-path keyed overrides to node-id keyed overrides"
    (is (= {1 {:position [1.0 2.0 3.0]}
            2 {:scale [2.0 2.0 2.0]}}
           (scene/collapse-property-preview-overrides {[7 1] {:position [1.0 2.0 3.0]}
                                                       [8 2] {:scale [2.0 2.0 2.0]}}))))

  (testing "preserves identical overrides that collapse onto the same node-id"
    (is (= {1 {:position [1.0 2.0 3.0]}}
           (scene/collapse-property-preview-overrides {[7 1] {:position [1.0 2.0 3.0]}
                                                       [8 1] {:position [1.0 2.0 3.0]}}))))

  (testing "fails fast on conflicting overrides for the same node-id/property"
    (is (thrown? AssertionError
                 (scene/collapse-property-preview-overrides {[7 1] {:position [1.0 2.0 3.0]}
                                                             [8 1] {:position [4.0 5.0 6.0]}})))))

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

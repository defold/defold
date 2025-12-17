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

(ns editor.breakpoints-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.data :as code.data]
            [editor.code.script :as code.script]
            [editor.breakpoints-view :as breakpoints-view]
            [integration.test-util :as test-util]))

(def ^:private project-path "test/resources/geometry_wars")

(deftest set-regions-with-action-test
  (test-util/with-loaded-project project-path
    (let [evaluation-context (g/make-evaluation-context)
          f #'breakpoints-view/set-regions-with-action!
          set-regions-with-action! #(f project %1 %2 %3 evaluation-context)
          script-resource (test-util/resource workspace "/main/main.script")
          script-node (test-util/resource-node project script-resource)
          lines (g/node-value script-node :lines evaluation-context)
          call-set-regions! (fn [existing-breakpoints new-breakpoints action-fn]
                              (set-regions-with-action!
                                existing-breakpoints
                                new-breakpoints
                                action-fn))
          set-breakpoints-on-script! (fn [breakpoints]
                                       (let [regions (g/node-value script-node :regions)]
                                         (g/set-property! script-node :regions
                                           (mapv (partial #'breakpoints-view/breakpoint->region lines) breakpoints))))]
      (testing "new breakpoints are mapped to regions"
        (let [breakpoints [{:row 1 :resource script-resource :condition "x > 5" :enabled true}
                           {:row 2 :resource script-resource :enabled false}]
              _ (call-set-regions! [] breakpoints (fn [all new] (into all new)))
              regions (g/node-value script-node :regions)]
          (is (= 2 (count regions)))
          (doseq [[bp region] (map vector breakpoints regions)]
            (is (= (code.script/region->breakpoint script-resource region) bp)))))

      (testing "existing breakpoints are mapped to regions"
        (let [breakpoints [{:row 1 :resource script-resource :condition "x > 5" :enabled true}
                           {:row 2 :resource script-resource :enabled false}]
              _ (set-breakpoints-on-script! breakpoints)
              _ (call-set-regions! breakpoints breakpoints (fn [all new] new))
              regions (g/node-value script-node :regions)]
          (is (= 2 (count regions)))
          (doseq [[bp region] (map vector breakpoints regions)]
            (is (= (code.script/region->breakpoint script-resource region) bp)))))

      (testing "missing breakpoints are mapped to empty"
        (let [breakpoints [{:row 1 :resource script-resource :condition "x > 5" :enabled true}
                           {:row 2 :resource script-resource :enabled false}]
              _ (set-breakpoints-on-script! breakpoints)
              _ (call-set-regions! [] breakpoints #(vec (remove (set %2) %1)))
              regions (g/node-value script-node :regions)]
          (is (not (seq regions)))))
      (testing "handles modified, removed, and new breakpoints"
        (let [original-bps [{:row 1 :resource script-resource :enabled true}
                            {:row 2 :resource script-resource :enabled true}
                            {:row 3 :resource script-resource :enabled true}]
              modified-bps [{:row 1 :resource script-resource :condition "modified1" :enabled true}
                            {:row 2 :resource script-resource :enabled false}
                            {:row 5 :resource script-resource :condition "new1" :enabled true}
                            {:row 6 :resource script-resource :enabled false}]
              _ (call-set-regions! original-bps modified-bps (fn [all new] new))
              new-bps (mapv (partial code.script/region->breakpoint script-resource)
                            (g/node-value script-node :regions))]

          (is (= 4 (count new-bps)))

          (let [modified-1 (first (filter #(= 1 (:row %)) new-bps))
                modified-2 (first (filter #(= 2 (:row %)) new-bps))]
            (is (= "modified1" (:condition modified-1)))
            (is (true? (:enabled modified-1)))
            (is (nil? (:condition modified-2)))
            (is (false? (:enabled modified-2))))

          (is (nil? (first (filter #(= 3 (:row %)) new-bps))))

          (let [new-1 (first (filter #(= 5 (:row %)) new-bps))
                new-2 (first (filter #(= 6 (:row %)) new-bps))]
            (is (= "new1" (:condition new-1)))
            (is (true? (:enabled new-1)))
            (is (nil? (:condition new-2)))
            (is (false? (:enabled new-2)))))))))

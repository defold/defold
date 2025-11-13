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
            [editor.code.data :as code-data]
            [editor.breakpoints-view :as breakpoints-view]
            [integration.test-util :as test-util]))

(def ^:private project-path "test/resources/geometry_wars")

(deftest update-script-regions-from-breakpoints-test
  (test-util/with-loaded-project project-path
    (let [update-fn (var-get #'breakpoints-view/update-script-regions-from-breakpoints)
          script-resource (test-util/resource workspace "/main/main.script")
          script-node (test-util/resource-node (dev/project) script-resource)
          ec (g/make-evaluation-context)
          lines (g/node-value script-node :lines ec)]

      (testing "new breakpoints are mapped to regions"
        (let [breakpoints [{:row 1 :resource script-resource :condition "x > 5" :active true}
                           {:row 2 :resource script-resource :active false}]
              result (update-fn script-node breakpoints ec)]
          (is (= 2 (count result)))
          (doseq [[bp region] (map vector breakpoints result)]
            (is (= (code-data/region->breakpoint script-resource region) bp)))))

      (testing "existing breakpoints are mapped to regions"
        (let [regions [(code-data/make-breakpoint-region lines 1)
                       (code-data/make-breakpoint-region lines 2)]
              breakpoints [{:row 1 :resource script-resource :condition "x > 5" :active true}
                           {:row 2 :resource script-resource :active false}]
              result (update-fn script-node breakpoints ec)]
          (is (= 2 (count result)))
          (doseq [[bp region] (map vector breakpoints result)]
            (is (= (code-data/region->breakpoint script-resource region) bp)))))

      (testing "missing breakpoints are mapped to empty"
        (let [regions [(code-data/make-breakpoint-region lines 1)
                       (code-data/make-breakpoint-region lines 2)]
              breakpoints []
              result (update-fn script-node breakpoints ec)]
          (is (not (seq result)))))
      (testing "handles modified, removed, and new breakpoints"
        (let [existing-regions [(code-data/make-breakpoint-region lines 1)
                                (code-data/make-breakpoint-region lines 2)
                                (code-data/make-breakpoint-region lines 3)
                                (code-data/make-breakpoint-region lines 4)]
              breakpoints [{:row 1 :resource script-resource :condition "modified1" :active true}
                           {:row 2 :resource script-resource :active false}
                           {:row 5 :resource script-resource :condition "new1" :active true}
                           {:row 6 :resource script-resource :active false}]
              result (update-fn script-node breakpoints ec)]
          
          (is (= 4 (count result)))
          
          (let [modified-1 (first (filter #(= 1 (:row (code-data/region->breakpoint script-resource %))) result))
                modified-2 (first (filter #(= 2 (:row (code-data/region->breakpoint script-resource %))) result))]
            (is (= "modified1" (:condition (code-data/region->breakpoint script-resource modified-1))))
            (is (true? (:active (code-data/region->breakpoint script-resource modified-1))))
            (is (nil? (:condition (code-data/region->breakpoint script-resource modified-2))))
            (is (false? (:active (code-data/region->breakpoint script-resource modified-2)))))
          
          (is (nil? (first (filter #(= 3 (:row (code-data/region->breakpoint script-resource %))) result))))
          (is (nil? (first (filter #(= 4 (:row (code-data/region->breakpoint script-resource %))) result))))
          
          (let [new-1 (first (filter #(= 5 (:row (code-data/region->breakpoint script-resource %))) result))
                new-2 (first (filter #(= 6 (:row (code-data/region->breakpoint script-resource %))) result))]
            (is (= "new1" (:condition (code-data/region->breakpoint script-resource new-1))))
            (is (true? (:active (code-data/region->breakpoint script-resource new-1))))
            (is (nil? (:condition (code-data/region->breakpoint script-resource new-2))))
            (is (false? (:active (code-data/region->breakpoint script-resource new-2))))))))))

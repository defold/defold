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

(ns editor.breakpoints-tab-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.breakpoints-tab :as breakpoints-tab]
            [integration.test-util :as test-util]))

(def ^:private project-path "test/resources/geometry_wars")

(deftest new-breakpoints-are-mapped-to-regions
  (test-util/with-loaded-project project-path
    (let [update-script-regions-from-breakpoints (var-get #'breakpoints-tab/update-script-regions-from-breakpoints)
          script-resource (test-util/resource workspace "/main/main.script")
          script-node (test-util/resource-node (dev/project) script-resource)
          breakpoints [{:row 1 :resource script-resource :condition "x > 5" :active true}
                       {:row 2 :resource script-resource :active false}]
          ec (g/make-evaluation-context)]
      
      (let [result (update-script-regions-from-breakpoints script-node breakpoints ec)]
        (is (= 2 (count result)))
        (doseq [[bp region] (map vector breakpoints result)]
          (is (= (code-data/region->breakpoint script-resource region) bp)))))))

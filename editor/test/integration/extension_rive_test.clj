;; Copyright 2020-2024 The Defold Foundation
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

(ns integration.extension-rive-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/rive_project")

(deftest registered-resource-types-test
  (test-util/with-loaded-project project-path
    (is (= #{} (test-util/protobuf-resource-exts-that-read-defaults workspace)))))

(deftest collection-usage-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))

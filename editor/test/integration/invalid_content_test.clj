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

(ns integration.invalid-content-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(def ^:private invalid-content-project-path "test/resources/invalid_content_project")

(deftest malformed-collection-reports-invalid-content
  (test-util/with-loaded-project invalid-content-project-path :logging-suppressed true
    (let [node-id (test-util/resource-node project "/main/invalid_child_ref.collection")
          error (test-util/build-error! node-id)
          invalid-content-cause (first (:causes error))
          exception (-> invalid-content-cause :user-data :exception)]
      (is (g/error? error))
      (is (= :invalid-content (-> invalid-content-cause :user-data :type)))
      (is (= "Unresolved child id 'missing' referenced by parent 'root'."
             (ex-message exception))))))

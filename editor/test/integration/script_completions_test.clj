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

(ns integration.script-completions-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(deftest component-id-completions-test
  (test-util/with-loaded-project
    (testing "script attached to a game object completes its component ids"
      (let [script-node (test-util/resource-node project "/logic/main.script")
            completions (get (g/node-value script-node :completions) "#")]
        (is (= ["camera" "gui" "script" "session_proxy"] (mapv :name completions)))
        (is (every? #(= "/logic/main.go" (:detail %)) completions))))
    (testing "component ids follow graph changes"
      (let [script-node (test-util/resource-node project "/logic/main.script")
            go-node (test-util/resource-node project "/logic/main.go")
            camera-node (get (g/node-value go-node :component-ids) "camera")]
        (g/transact (g/delete-node camera-node))
        (is (= ["gui" "script" "session_proxy"]
               (mapv :name (get (g/node-value script-node :completions) "#"))))))))

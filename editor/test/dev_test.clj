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

(ns dev-test
  (:require [clojure.test :refer :all]
            [dev :as dev]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(deftest node-type-accessors-test
  (test-util/with-loaded-project "test/resources/empty_project"
    (is (= :editor.workspace/Workspace (g/node-type-kw (dev/workspace))))
    (is (= :editor.defold-project/Project (g/node-type-kw (dev/project))))
    (is (= (project/workspace (dev/project)) (dev/workspace)))
    (is (= app-view (dev/app-view)))))

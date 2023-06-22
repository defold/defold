;; Copyright 2020-2023 The Defold Foundation
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
            [dynamo.graph :as g]))

(set! *warn-on-reflection* true)

(deftest node-type-accessors-test
  (are [expected-node-type-kw node-id]
    (= expected-node-type-kw (g/node-type-kw node-id))

    ;; Data model nodes.
    :editor.workspace/Workspace (dev/workspace)
    :editor.defold-project/Project (dev/project)

    ;; View nodes.
    :editor.app-view/AppView (dev/app-view)
    :editor.asset-browser/AssetBrowser (dev/assets-view)
    :editor.changes-view/ChangesView (dev/changed-files-view)
    :editor.outline-view/OutlineView (dev/outline-view)
    :editor.properties-view/PropertiesView (dev/properties-view)
    :editor.curve-view/CurveView (dev/curve-view)
    :editor.code.view/CodeEditorView (dev/console-view)))

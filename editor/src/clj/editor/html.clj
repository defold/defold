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

(ns editor.html
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.localization :as localization]))

(g/defnode HtmlNode
  (inherits r/CodeEditorResourceNode)

  (output html g/Str :cached (g/fnk [save-value] (data/lines->string save-value))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
    :ext "html"
    :label (localization/message "resource.type.html")
    :node-type HtmlNode
    :view-types [:html :code]
    :view-opts nil))

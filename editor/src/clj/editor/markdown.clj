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

(ns editor.markdown
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.localization :as localization]
            [editor.markdown-view :as markdown-view]))

(g/defnode MarkdownNode
  (inherits r/CodeEditorResourceNode)

  (output html g/Str :cached (g/fnk [save-value]
                               (str "<!DOCTYPE html>"
                                    "<html><head></head><body>"
                                    (-> save-value
                                        data/lines->string
                                        markdown-view/markdown->html)
                                    "</body></html>"))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
    :ext "md"
    :label (localization/message "resource.type.markdown")
    :node-type MarkdownNode
    :view-types [:html :code]
    :view-opts nil))

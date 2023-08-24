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

(ns editor.markdown
  (:require [dynamo.graph :as g]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [org.commonmark.parser Parser]
           [org.commonmark.renderer.html HtmlRenderer]))

(defn markdown->html
  [markdown]
  (let [parser (.build (Parser/builder))
        doc (.parse parser markdown)
        renderer (.build (HtmlRenderer/builder))]
    (.render renderer doc)))

(g/defnode MarkdownNode
  (inherits resource-node/ResourceNode)

  (output markdown g/Str :cached (g/fnk [_node-id resource]
                                   (resource-io/with-error-translation resource _node-id :markdown
                                     (slurp resource :encoding "UTF-8"))))

  (output html g/Str :cached (g/fnk [markdown]
                               (str "<!DOCTYPE html>"
                                    "<html><head></head><body>"
                                    (markdown->html markdown)
                                    "</body></html>"))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
    :ext "md"
    :label "Markdown"
    :textual? true
    :node-type MarkdownNode
    :view-types [:html :text]
    :view-opts nil))

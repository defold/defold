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
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [util.text-util :as text-util]))

(g/defnode HtmlNode
  (inherits resource-node/ResourceNode)

  (output html g/Str :cached (g/fnk [_node-id resource]
                               (resource-io/with-error-translation resource _node-id :html
                                 (slurp resource :encoding "UTF-8")))))

(defn- search-value-fn [node-id _resource evaluation-context]
  (g/node-value node-id :html evaluation-context))

(defn search-fn
  ([search-string]
   (text-util/search-string->re-pattern search-string :case-insensitive))
  ([html re-pattern]
   (text-util/text->text-matches html re-pattern)))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
    :ext "html"
    :label "HTML"
    :textual? true
    :search-fn search-fn
    :search-value-fn search-value-fn
    :node-type HtmlNode
    :view-types [:html :text]
    :view-opts nil))

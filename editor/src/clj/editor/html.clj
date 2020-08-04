;; Copyright 2020 The Defold Foundation
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
  (:require
   [dynamo.graph :as g]
   [editor.resource-node :as resource-node]
   [editor.workspace :as workspace]))

(g/defnode HtmlNode
  (inherits resource-node/ResourceNode)

  (output html g/Str (g/fnk [resource] (slurp resource :encoding "UTF-8"))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "html"
                                    :label "HTML"
                                    :node-type HtmlNode
                                    :view-types [:html]
                                    :view-opts nil))
